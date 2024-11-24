/* SPX - A simple profiler for PHP
 * Copyright (C) 2017-2024 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "main/php.h"
#include "main/SAPI.h"

/* _GNU_SOURCE is implicitly defined since PHP 8.2 https://github.com/php/php-src/pull/8807 */
#ifndef _GNU_SOURCE
#   define _GNU_SOURCE /* vasprintf */
#endif

#include <stdio.h>

#include "spx_php.h"
#include "spx_thread.h"
#include "spx_str_builder.h"
#include "spx_utils.h"

#if ZEND_MODULE_API_NO >= 20151012
#   define ZE_HASHTABLE_FOREACH(ht, entry, block)               \
do {                                                            \
    void * entry;                                               \
    zend_hash_internal_pointer_reset(ht);                       \
    while ((entry = zend_hash_get_current_data(ht)) != NULL) {  \
        zend_hash_move_forward(ht);                             \
        block                                                   \
    }                                                           \
} while (0)
#else
#   define ZE_HASHTABLE_FOREACH(ht, entry, block)        \
do {                                                     \
    HashPosition pos_;                                   \
    void * entry;                                        \
    zend_hash_internal_pointer_reset_ex(ht, &pos_);      \
    while (                                              \
        SUCCESS == zend_hash_get_current_data_ex(        \
            ht,                                          \
            (void **)&entry,                             \
            &pos_                                        \
        )                                                \
    ) {                                                  \
        zend_hash_move_forward_ex(ht, &pos_);            \
        block                                            \
    }                                                    \
} while (0)
#endif

typedef void (*execute_internal_func_t) (
    zend_execute_data * execute_data,
#if ZEND_MODULE_API_NO >= 20151012
    zval * return_value
#else
#if ZEND_MODULE_API_NO >= 20121212
    struct _zend_fcall_info * fci,
#endif
    int ret
#endif
    TSRMLS_DC
);

static struct {
#if ZEND_MODULE_API_NO < 20121212
    void (*execute) (zend_op_array * op_array TSRMLS_DC);
#else
    void (*execute_ex) (zend_execute_data * execute_data TSRMLS_DC);
#endif
    execute_internal_func_t previous_zend_execute_internal;
    execute_internal_func_t execute_internal;

    zend_op_array * (*zend_compile_file)(zend_file_handle * file_handle, int type TSRMLS_DC);
    zend_op_array * (*zend_compile_string)(
#if PHP_API_VERSION >= 20200930
        zend_string * source_string,
        const
#else
        zval * source_string,
#endif
        char * filename
#if PHP_API_VERSION >= 20210903
        , zend_compile_position position
#endif
        TSRMLS_DC
    );

#if ZEND_MODULE_API_NO >= 20151012
    int (*gc_collect_cycles)(void);
#endif

    void (*zend_error_cb) (
        int type,
#if PHP_API_VERSION >= 20210902
        zend_string *error_filename,
#else
        const char *error_filename,
#endif
        const uint error_lineno,
#if PHP_API_VERSION >= 20200930
        zend_string *message
#else
        const char *format,
        va_list args
#endif
    );
} ze_hooked_func = {
    NULL, NULL, NULL,
    NULL, NULL,
#if ZEND_MODULE_API_NO >= 20151012
    NULL,
#endif
    NULL
};

#if ZEND_MODULE_API_NO >= 20151012
static SPX_THREAD_TLS struct {
    void * (*malloc) (size_t size);
    void (*free) (void * ptr);
    void * (*realloc) (void * ptr, size_t size);
    size_t (*block_size) (void * ptr);
} ze_tls_hooked_func = {
    NULL, NULL, NULL, NULL
};
#endif

static SPX_THREAD_TLS struct {
    struct {
        struct {
            void (*before)(void);
            void (*after)(void);
        } user, internal;
    } ex_hook;

    int global_hooks_enabled;
    int execution_disabled;

    size_t user_depth;
    int request_shutdown;
    int collect_userland_stats;

    size_t file_count;
    size_t line_count;
    size_t class_count;
    size_t function_count;
    size_t opcode_count;
    size_t file_opcode_count;
    size_t error_count;

    size_t alloc_count;
    size_t alloc_bytes;
    size_t free_count;
    size_t free_bytes;

    const char * active_function_name;
} context;

static void execute_data_function(const zend_execute_data * execute_data, spx_php_function_t * function TSRMLS_DC);
static void reset_context(void);

#if ZEND_MODULE_API_NO >= 20151012
static size_t ze_mm_block_size(void * ptr);
static size_t ze_mm_custom_block_size(void * ptr);
static void * ze_mm_malloc(size_t size);
static void ze_mm_free(void * ptr);
static void * ze_mm_realloc(void * ptr, size_t size);

static void * tls_hook_malloc(size_t size);
static void tls_hook_free(void * ptr);
static void * tls_hook_realloc(void * ptr, size_t size);
#endif

#if ZEND_MODULE_API_NO < 20121212
static void global_hook_execute(zend_op_array * op_array TSRMLS_DC);
#else
static void global_hook_execute_ex(zend_execute_data * execute_data TSRMLS_DC);
#endif
static void global_hook_execute_internal(
    zend_execute_data * execute_data,
#if ZEND_MODULE_API_NO >= 20151012
    zval * return_value
#else
#if ZEND_MODULE_API_NO >= 20121212
    struct _zend_fcall_info * fci,
#endif
    int ret
#endif
    TSRMLS_DC
);

static zend_op_array * global_hook_zend_compile_file(zend_file_handle * file_handle, int type TSRMLS_DC);
static zend_op_array * global_hook_zend_compile_string(
#if PHP_API_VERSION >= 20200930
    zend_string * source_string,
    const
#else
    zval * source_string,
#endif
    char * filename
#if PHP_API_VERSION >= 20210903
    , zend_compile_position position
#endif
        TSRMLS_DC
);

#if ZEND_MODULE_API_NO >= 20151012
static int global_hook_gc_collect_cycles(void);
#endif

static void global_hook_zend_error_cb(
    int type,
#if PHP_API_VERSION >= 20210902
    zend_string *error_filename,
#else
    const char *error_filename,
#endif
    const uint error_lineno,
#if PHP_API_VERSION >= 20200930
    zend_string *message
#else
    const char *format,
    va_list args
#endif
);

static void update_userland_stats(void);

static HashTable * get_global_array(const char * name);

int spx_php_is_cli_sapi(void)
{
    return 0 == strcmp(sapi_module.name, "cli");
}

void spx_php_current_function(spx_php_function_t * function)
{
    TSRMLS_FETCH();

    function->hash_code = 0;
    function->class_name = "";
    function->func_name = "";

    if (context.active_function_name) {
        function->class_name = "";
        function->func_name = context.active_function_name;
    } else {
        execute_data_function(EG(current_execute_data), function TSRMLS_CC);
    }

    function->hash_code =
        zend_inline_hash_func(function->func_name, strlen(function->func_name)) ^
        zend_inline_hash_func(function->class_name, strlen(function->class_name))
    ;
}

const char * spx_php_ini_get_string(const char * name)
{
    return zend_ini_string(
        /*
         * This cast is checked
         */
        (char *)name,
        strlen(name)
#if ZEND_MODULE_API_NO < 20151012
            + 1
#endif
        ,
        0
    );
}

double spx_php_ini_get_double(const char * name)
{
    return zend_ini_double(
        /*
         * This cast is checked
         */
        (char *)name,
        strlen(name)
#if ZEND_MODULE_API_NO < 20151012
            + 1
#endif
        ,
        0
    );
}

const char * spx_php_global_array_get(const char * name, const char * key)
{
    HashTable * global_array = get_global_array(name);
    if (!global_array) {
        return NULL;
    }

#if ZEND_MODULE_API_NO >= 20151012
    zval * pv = zend_hash_str_find(
        global_array,
        key,
        strlen(key)
    );

    if (!pv) {
        return NULL;
    }

    convert_to_string_ex(pv);

    return Z_STRVAL_P(pv);
#else
    zval ** ppv;
    if (
        zend_hash_find(
            global_array,
            key,
            strlen(key) + 1,
            (void **) &ppv
        ) == SUCCESS
    ) {
        return Z_STRVAL_PP(ppv);
    }

    return NULL;
#endif
}

char * spx_php_build_command_line(void)
{
    HashTable * global_array = get_global_array("_SERVER");
    if (!global_array) {
        return NULL;
    }

    const char * argv_key = "argv";

#if ZEND_MODULE_API_NO >= 20151012
    zval * argv = zend_hash_str_find(
        global_array,
        argv_key,
        strlen(argv_key)
    );

    if (!argv) {
        goto error;
    }

    if (Z_TYPE_P(argv) != IS_ARRAY) {
        goto error;
    }

    spx_str_builder_t * str_builder = spx_str_builder_create(2 * 1024);
    if (!str_builder) {
        goto error;
    }

    HashTable * argv_array = Z_ARRVAL_P(argv);
#else
    zval ** argv;
    if (
        zend_hash_find(
            global_array,
            argv_key,
            strlen(argv_key) + 1,
            (void **) &argv
        ) != SUCCESS
    ) {
        goto error;
    }

    if (Z_TYPE_PP(argv) != IS_ARRAY) {
        goto error;
    }

    spx_str_builder_t * str_builder = spx_str_builder_create(2 * 1024);
    if (!str_builder) {
        goto error;
    }

    HashTable * argv_array = Z_ARRVAL_PP(argv);
#endif
    int i = 0;

    ZE_HASHTABLE_FOREACH(argv_array, entry, {
#if ZEND_MODULE_API_NO >= 20151012
        zval * zval_entry = entry;
#else
        zval * zval_entry = *(zval **)entry;
#endif
        if (Z_TYPE_P(zval_entry) == IS_STRING) {
            if (i++ > 0) {
                spx_str_builder_append_char(str_builder, ' ');
            }

            if (0 == spx_str_builder_append_str(str_builder, Z_STRVAL_P(zval_entry))) {
                break;
            }
        }
    });

    char * command_line = strdup(spx_str_builder_str(str_builder));
    spx_str_builder_destroy(str_builder);

    return command_line;

error:
    return NULL;
}

size_t spx_php_zend_memory_usage(void)
{
    TSRMLS_FETCH();

    return zend_memory_usage(0 TSRMLS_CC);
}


size_t spx_php_zend_memory_alloc_count(void)
{
    return context.alloc_count;
}

size_t spx_php_zend_memory_alloc_bytes(void)
{
    return context.alloc_bytes;
}

size_t spx_php_zend_memory_free_count(void)
{
    return context.free_count;
}

size_t spx_php_zend_memory_free_bytes(void)
{
    return context.free_bytes;
}

size_t spx_php_zend_gc_run_count(void)
{
#if ZEND_MODULE_API_NO >= 20180731
    zend_gc_status status;
    zend_gc_get_status(&status);

    return status.runs;
#else
    TSRMLS_FETCH();

    return GC_G(gc_runs);
#endif
}

size_t spx_php_zend_gc_root_buffer_length(void)
{
#if ZEND_MODULE_API_NO >= 20180731
    zend_gc_status status;
    zend_gc_get_status(&status);

    return status.num_roots;
#else
    TSRMLS_FETCH();

    size_t length = 0;
    const gc_root_buffer * current = GC_G(roots).next;

    while (current != &GC_G(roots)) {
        length++;
        current = current->next;
    }

    return length;
#endif
}

size_t spx_php_zend_gc_collected_count(void)
{
#if ZEND_MODULE_API_NO >= 20180731
    zend_gc_status status;
    zend_gc_get_status(&status);

    return status.collected;
#else
    TSRMLS_FETCH();

    return GC_G(collected);
#endif
}

size_t spx_php_zend_included_file_count(void)
{
    return context.file_count;
}


size_t spx_php_zend_included_line_count(void)
{
    return context.line_count;
}

size_t spx_php_zend_class_count(void)
{
    if (!context.collect_userland_stats) {
        context.collect_userland_stats = 1;

        update_userland_stats();
    }

    return context.class_count;
}

size_t spx_php_zend_function_count(void)
{
    if (!context.collect_userland_stats) {
        context.collect_userland_stats = 1;

        update_userland_stats();
    }

    return context.function_count;
}

size_t spx_php_zend_opcode_count(void)
{
    if (!context.collect_userland_stats) {
        context.collect_userland_stats = 1;

        update_userland_stats();
    }

    return context.opcode_count;
}

size_t spx_php_zend_object_count(void)
{
    TSRMLS_FETCH();

    size_t i, count = 0;
    for (i = 1; i < EG(objects_store).top; i++) {
        if (
#if ZEND_MODULE_API_NO >= 20151012
            IS_OBJ_VALID(EG(objects_store).object_buckets[i])
#else
            EG(objects_store).object_buckets[i].valid
#endif
        ) {
            count++;
        }
    }

    return count;
}

size_t spx_php_zend_error_count(void)
{
    return context.error_count;
}

void spx_php_global_hooks_set(void)
{
#if ZEND_MODULE_API_NO < 20121212
    ze_hooked_func.execute = zend_execute;
    zend_execute = global_hook_execute;
#else
    ze_hooked_func.execute_ex = zend_execute_ex;
    zend_execute_ex = global_hook_execute_ex;
#endif

    ze_hooked_func.previous_zend_execute_internal = zend_execute_internal;
    ze_hooked_func.execute_internal = zend_execute_internal ?
        zend_execute_internal : execute_internal
    ;
    zend_execute_internal = global_hook_execute_internal;

    ze_hooked_func.zend_compile_file = zend_compile_file;
    zend_compile_file = global_hook_zend_compile_file;

    ze_hooked_func.zend_compile_string = zend_compile_string;
    zend_compile_string = global_hook_zend_compile_string;

#if ZEND_MODULE_API_NO >= 20151012
    ze_hooked_func.gc_collect_cycles = gc_collect_cycles;
    gc_collect_cycles = global_hook_gc_collect_cycles;
#endif

    ze_hooked_func.zend_error_cb = zend_error_cb;
    zend_error_cb = global_hook_zend_error_cb;
}

void spx_php_global_hooks_unset(void)
{
#if ZEND_MODULE_API_NO < 20121212
    if (ze_hooked_func.execute) {
        zend_execute = ze_hooked_func.execute;
        ze_hooked_func.execute = NULL;
    }
#else
    if (ze_hooked_func.execute_ex) {
        zend_execute_ex = ze_hooked_func.execute_ex;
        ze_hooked_func.execute_ex = NULL;
    }
#endif

    if (ze_hooked_func.execute_internal) {
        zend_execute_internal = ze_hooked_func.previous_zend_execute_internal;
        ze_hooked_func.previous_zend_execute_internal = NULL;
        ze_hooked_func.execute_internal = NULL;
    }

    if (ze_hooked_func.zend_compile_file) {
        zend_compile_file = ze_hooked_func.zend_compile_file;
        ze_hooked_func.zend_compile_file = NULL;
    }

    if (ze_hooked_func.zend_compile_string) {
        zend_compile_string = ze_hooked_func.zend_compile_string;
        ze_hooked_func.zend_compile_string = NULL;
    }

#if ZEND_MODULE_API_NO >= 20151012
    if (ze_hooked_func.gc_collect_cycles) {
        gc_collect_cycles = ze_hooked_func.gc_collect_cycles;
        ze_hooked_func.gc_collect_cycles = NULL;
    }
#endif

    if (ze_hooked_func.zend_error_cb) {
        zend_error_cb = ze_hooked_func.zend_error_cb;
        ze_hooked_func.zend_error_cb = NULL;
    }
}

void spx_php_global_hooks_disable(void)
{
    context.global_hooks_enabled = 0;
}

void spx_php_execution_finalize(void)
{
    if (!context.request_shutdown) {
        return;
    }

    if (context.ex_hook.internal.after) {
        context.active_function_name = "::php_request_shutdown";
        context.ex_hook.internal.after();
        context.active_function_name = NULL;
    }

    context.request_shutdown = 0;
}

void spx_php_execution_init(void)
{
    reset_context();

#if ZEND_MODULE_API_NO >= 20151012
    zend_mm_heap * ze_mm_heap = zend_mm_get_heap();

    /*
     * FIXME document why we need ze_mm_custom_block_size instead of ze_mm_block_size
     * when there is no previous MM custom handler.
     */
    ze_tls_hooked_func.block_size = ze_mm_custom_block_size;

    zend_mm_get_custom_handlers(
        ze_mm_heap,
        &ze_tls_hooked_func.malloc,
        &ze_tls_hooked_func.free,
        &ze_tls_hooked_func.realloc
    );

    if (
        !ze_tls_hooked_func.malloc
        || !ze_tls_hooked_func.free
        || !ze_tls_hooked_func.realloc
    ) {
        ze_tls_hooked_func.malloc = ze_mm_malloc;
        ze_tls_hooked_func.free = ze_mm_free;
        ze_tls_hooked_func.realloc = ze_mm_realloc;
        ze_tls_hooked_func.block_size = ze_mm_block_size;
    }

    zend_mm_set_custom_handlers(
        ze_mm_heap,
        tls_hook_malloc,
        tls_hook_free,
        tls_hook_realloc
    );
#endif
}

void spx_php_execution_shutdown(void)
{
#if ZEND_MODULE_API_NO >= 20151012
    if (
        ze_tls_hooked_func.malloc
        && ze_tls_hooked_func.free
        && ze_tls_hooked_func.realloc
    ) {
        zend_mm_heap * ze_mm_heap = zend_mm_get_heap();

        if (
            /*
             * ze_tls_hooked_func.malloc was defaulted to ze_mm_malloc only if there were no
             * previous custom handlers.
             */
            ze_tls_hooked_func.malloc != ze_mm_malloc
        ) {
            zend_mm_set_custom_handlers(
                ze_mm_heap,
                ze_tls_hooked_func.malloc,
                ze_tls_hooked_func.free,
                ze_tls_hooked_func.realloc
            );
        } else {
            /*
             *  This ugly hack, breaking strict aliasing rule and zend_mm_heap ADT
             *  encapsulation, is the only way to restore internal state of heap
             *  (prior to spx_php_execution_init()).
             *  It supposes "use_custom_heap" is an int and the first field of
             *  zend_mm_heap type.
             *  Setting back original allocator handlers (wrapped in this TU by
             *  ze_(*alloc|free) functions) does not work as it causes SIGSEV
             *  at module shutdown (in a child of zend_unregister_functions()).
             */
            *((int *) ze_mm_heap) = 0;
            if (!is_zend_mm()) {
                spx_utils_die("Zend MM heap corrupted");
            }
        }

        ze_tls_hooked_func.malloc = NULL;
        ze_tls_hooked_func.free = NULL;
        ze_tls_hooked_func.realloc = NULL;
        ze_tls_hooked_func.block_size = NULL;
    }
#endif

    reset_context();
}

void spx_php_execution_disable(void)
{
    context.execution_disabled = 1;
}

void spx_php_execution_hook(void (*before)(void), void (*after)(void), int internal)
{
    if (internal) {
        context.ex_hook.internal.before = before;
        context.ex_hook.internal.after = after;
    } else {
        context.ex_hook.user.before = before;
        context.ex_hook.user.after = after;
    }
}

void spx_php_output_add_header_line(const char * header_line)
{
    TSRMLS_FETCH();

    sapi_header_line ctr = {0};

    /*
        This cast is checked, header_line will be first duped before being manipulated.
        See sapi_header_op() implementation.
    */
    ctr.line = (char *)header_line;
    ctr.line_len = strlen(header_line);

    sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);
}

void spx_php_output_add_header_linef(const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    char * buf;
    int printed = vasprintf(&buf, fmt, ap);
    va_end(ap);

    if (printed < 0) {
        return;
    }

    spx_php_output_add_header_line(buf);
    free(buf);
}

void spx_php_output_send_headers(void)
{
    TSRMLS_FETCH();

    sapi_send_headers(TSRMLS_C);
}

size_t spx_php_output_direct_write(const void * ptr, size_t len)
{
    TSRMLS_FETCH();

    return sapi_module.ub_write(ptr, len TSRMLS_CC);
}

size_t spx_php_output_direct_print(const char * str)
{
    return spx_php_output_direct_write(str, strlen(str));
}

int spx_php_output_direct_printf(const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    char * buf;
    int printed = vasprintf(&buf, fmt, ap);
    va_end(ap);

    if (printed < 0) {
        return printed;
    }

    printed = spx_php_output_direct_print(buf);
    free(buf);

    return printed;
}

void spx_php_log_notice(const char * fmt, ...)
{
    if (0 == spx_php_ini_get_double("log_errors")) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);

    char * buf;
    const int printed = vasprintf(&buf, fmt, ap);
    va_end(ap);

    if (printed < 0) {
        return;
    }

    zend_error(E_NOTICE, "SPX: %s", buf);

    free(buf);
}

static void execute_data_function(const zend_execute_data * execute_data, spx_php_function_t * function TSRMLS_DC)
{
    if (zend_is_executing(TSRMLS_C)) {
#if ZEND_MODULE_API_NO >= 20151012
        const zend_function * func = execute_data->func;
        switch (func->type) {
            case ZEND_USER_FUNCTION:
            case ZEND_INTERNAL_FUNCTION:
            {
                const zend_class_entry * ce = func->common.scope;
                if (ce) {
                    function->class_name = ZSTR_VAL(ce->name);
                }
            }
        }

        switch (func->type) {
            case ZEND_USER_FUNCTION:
            {
                zend_string * function_name = func->common.function_name;
                if (function_name) {
                    function->func_name = ZSTR_VAL(function_name);
                }

                break;
            }

            case ZEND_INTERNAL_FUNCTION:
                function->func_name = ZSTR_VAL(func->common.function_name);
        }
#else
        switch (execute_data->function_state.function->type) {
            case ZEND_USER_FUNCTION:
            case ZEND_INTERNAL_FUNCTION:
            {
                zend_class_entry *ce = execute_data->function_state.function->common.scope;
                if (ce) {
                    function->class_name = ce->name;
                }
            }
        }

        switch (execute_data->function_state.function->type) {
            case ZEND_USER_FUNCTION:
            {
                const char * function_name = (
                        (zend_op_array *) execute_data->function_state.function
                    )
                    ->function_name
                ;

                if (function_name) {
                    function->func_name = function_name;
                }

                break;
            }
            case ZEND_INTERNAL_FUNCTION:
                function->func_name = (
                        (zend_internal_function *) execute_data->function_state.function
                    )
                    ->function_name
                ;
        }
#endif

#if ZEND_MODULE_API_NO >= 20151012
        /*
         *  Required for PHP 7+ to avoid function name default'd to "main" in this case
         *  (including file level code).
         *  See get_active_function_name() implementation in php-src.
         */
        if (func->type == ZEND_USER_FUNCTION && !func->common.function_name) {
            function->func_name = "";
        }
        /*
         *  This hack is required for PHP 7.1 to prevent a segfault while dereferencing function->func_name
         *  TODO: open an issue if not yet tracked
         */
        if (func->type == ZEND_INTERNAL_FUNCTION && !func->common.function_name) {
            function->func_name = "";
        }
#endif
    }

    if (!function->func_name[0]) {
        function->class_name = "";

#if ZEND_MODULE_API_NO >= 20151012
        while (execute_data && (!execute_data->func || !ZEND_USER_CODE(execute_data->func->type))) {
            execute_data = execute_data->prev_execute_data;
        }

        if (execute_data) {
            function->func_name = ZSTR_VAL(execute_data->func->op_array.filename);
        } else {
            function->func_name = "[no active file]";
        }
#else
        if (EG(active_op_array)) {
            function->func_name = EG(active_op_array)->filename;
        } else {
            function->func_name = "[no active file]";
        }
#endif
    }
}

static void reset_context(void)
{
    context.ex_hook.user.before = NULL;
    context.ex_hook.user.after = NULL;
    context.ex_hook.internal.before = NULL;
    context.ex_hook.internal.after = NULL;

    context.global_hooks_enabled = 1;
    context.execution_disabled = 0;
    context.user_depth = 0;
    context.request_shutdown = 0;
    context.collect_userland_stats = 0;
    context.file_count = 0;
    context.line_count = 0;
    context.class_count = 0;
    context.function_count = 0;
    context.opcode_count = 0;
    context.file_opcode_count = 0;
    context.error_count = 0;

    context.alloc_count = 0;
    context.alloc_bytes = 0;
    context.free_count = 0;
    context.free_bytes = 0;
}

#if ZEND_MODULE_API_NO >= 20151012
static size_t ze_mm_block_size(void * ptr)
{
    return zend_mm_block_size(zend_mm_get_heap(), ptr);
}

static size_t ze_mm_custom_block_size(void * ptr)
{
    return 0;
}

static void * ze_mm_malloc(size_t size)
{
    return zend_mm_alloc(zend_mm_get_heap(), size);
}

static void ze_mm_free(void * ptr)
{
    zend_mm_free(zend_mm_get_heap(), ptr);
}

static void * ze_mm_realloc(void * ptr, size_t size)
{
    return zend_mm_realloc(zend_mm_get_heap(), ptr, size);
}

static void * tls_hook_malloc(size_t size)
{
    void * ptr = ze_tls_hooked_func.malloc(size);

    if (ptr) {
        context.alloc_count++;
        context.alloc_bytes += ze_tls_hooked_func.block_size(ptr);
    }

    return ptr;
}

static void tls_hook_free(void * ptr)
{
    if (ptr) {
        context.free_count++;
        context.free_bytes += ze_tls_hooked_func.block_size(ptr);
    }

    ze_tls_hooked_func.free(ptr);
}

static void * tls_hook_realloc(void * ptr, size_t size)
{
    const size_t old_size = ptr ? ze_tls_hooked_func.block_size(ptr) : 0;
    void * new = ze_tls_hooked_func.realloc(ptr, size);
    const size_t new_size = new ? ze_tls_hooked_func.block_size(new) : 0;

    if (ptr && new) {
        if (ptr != new) {
            context.free_count++;
            context.free_bytes += old_size;
            context.alloc_count++;
            context.alloc_bytes += new_size;
        } else {
            const int diff = new_size - old_size;
            context.alloc_bytes += diff > 0 ? diff : 0;
        }
    } else if (new) {
        context.alloc_count++;
        context.alloc_bytes += new_size;
    }

    return new;
}
#endif

#if ZEND_MODULE_API_NO < 20121212
static void global_hook_execute(zend_op_array * op_array TSRMLS_DC)
#else
static void global_hook_execute_ex(zend_execute_data * execute_data TSRMLS_DC)
#endif
{
    if (!context.global_hooks_enabled) {
    #if ZEND_MODULE_API_NO < 20121212
        ze_hooked_func.execute(op_array TSRMLS_CC);
    #else
        ze_hooked_func.execute_ex(execute_data TSRMLS_CC);
    #endif

        return;
    }

    if (context.execution_disabled) {
        return;
    }

    context.user_depth++;

    if (context.ex_hook.user.before) {
        context.ex_hook.user.before();
    }

#if ZEND_MODULE_API_NO < 20121212
    ze_hooked_func.execute(op_array TSRMLS_CC);
#else
    ze_hooked_func.execute_ex(execute_data TSRMLS_CC);
#endif

    if (context.ex_hook.user.after) {
        context.ex_hook.user.after();
    }

    context.user_depth--;

    /*
     *  FIXME: it might not works with prepend files
     */
    if (context.user_depth == 0 && !context.request_shutdown) {
        context.request_shutdown = 1;

        if (context.ex_hook.internal.before) {
            context.active_function_name = "::php_request_shutdown";
            context.ex_hook.internal.before();
            context.active_function_name = NULL;
        }
    }
}

static void global_hook_execute_internal(
    zend_execute_data * execute_data,
#if ZEND_MODULE_API_NO >= 20151012
    zval * return_value
#else
#if ZEND_MODULE_API_NO >= 20121212
    struct _zend_fcall_info * fci,
#endif
    int ret
#endif
    TSRMLS_DC
) {
    if (!context.global_hooks_enabled) {
        ze_hooked_func.execute_internal(
            execute_data,
    #if ZEND_MODULE_API_NO >= 20151012
            return_value
    #else
    #if ZEND_MODULE_API_NO >= 20121212
            fci,
    #endif
            ret
    #endif
            TSRMLS_CC
        );

        return;
    }

    if (context.execution_disabled) {
        return;
    }

    if (context.ex_hook.internal.before) {
        context.ex_hook.internal.before();
    }

    ze_hooked_func.execute_internal(
        execute_data,
#if ZEND_MODULE_API_NO >= 20151012
        return_value
#else
#if ZEND_MODULE_API_NO >= 20121212
        fci,
#endif
        ret
#endif
        TSRMLS_CC
    );

    if (context.ex_hook.internal.after) {
        context.ex_hook.internal.after();
    }
}

static zend_op_array * global_hook_zend_compile_file(zend_file_handle * file_handle, int type TSRMLS_DC)
{
    if (!context.global_hooks_enabled) {
        return ze_hooked_func.zend_compile_file(file_handle, type TSRMLS_CC);
    }

    if (context.execution_disabled) {
        return NULL;
    }

    context.active_function_name = "::zend_compile_file";

    if (context.ex_hook.internal.before) {
        context.ex_hook.internal.before();
    }

    zend_op_array * op_array = ze_hooked_func.zend_compile_file(file_handle, type TSRMLS_CC);

    if (op_array) {
        context.file_count++;
        context.file_opcode_count += op_array->last - 1;

        /*
         *  FIXME: needs review
         */
        context.line_count += 1 + op_array->opcodes[op_array->last - 1].lineno;

        if (context.collect_userland_stats) {
            update_userland_stats();
        }
    }

    if (context.ex_hook.internal.after) {
        context.ex_hook.internal.after();
    }

    context.active_function_name = NULL;

    return op_array;
}

static zend_op_array * global_hook_zend_compile_string(
#if PHP_API_VERSION >= 20200930
    zend_string * source_string,
    const
#else
    zval * source_string,
#endif
    char * filename
#if PHP_API_VERSION >= 20210903
    , zend_compile_position position
#endif
        TSRMLS_DC
) {
    if (!context.global_hooks_enabled) {
        return ze_hooked_func.zend_compile_string(
            source_string,
            filename
#if PHP_API_VERSION >= 20210903
            , position
#endif
            TSRMLS_CC
        );
    }

    if (context.execution_disabled) {
        return NULL;
    }

    context.active_function_name = "::zend_compile_string";

    if (context.ex_hook.internal.before) {
        context.ex_hook.internal.before();
    }

    zend_op_array * op_array = ze_hooked_func.zend_compile_string(
        source_string,
        filename
#if PHP_API_VERSION >= 20210903
        , position
#endif
        TSRMLS_CC
    );

    if (op_array) {
        context.file_opcode_count += op_array->last - 1;

        /*
         *  FIXME: needs review
         */
        context.line_count += 1 + op_array->opcodes[op_array->last - 1].lineno;

        if (context.collect_userland_stats) {
            /*
             *  FIXME: it might not works with anonymous classes/functions
             */
            update_userland_stats();
        }
    }

    if (context.ex_hook.internal.after) {
        context.ex_hook.internal.after();
    }

    context.active_function_name = NULL;

    return op_array;
}

#if ZEND_MODULE_API_NO >= 20151012
static int global_hook_gc_collect_cycles(void)
{
    if (!context.global_hooks_enabled) {
        return ze_hooked_func.gc_collect_cycles();
    }

    if (context.execution_disabled) {
        return 0;
    }

    context.active_function_name = "::gc_collect_cycles";

    if (context.ex_hook.internal.before) {
        context.ex_hook.internal.before();
    }

    const int count = ze_hooked_func.gc_collect_cycles();

    if (context.ex_hook.internal.after) {
        context.ex_hook.internal.after();
    }

    context.active_function_name = NULL;

    return count;
}
#endif

static void global_hook_zend_error_cb(
    int type,
#if PHP_API_VERSION >= 20210902
    zend_string *error_filename,
#else
    const char *error_filename,
#endif
    const uint error_lineno,
#if PHP_API_VERSION >= 20200930
    zend_string *message
#else
    const char *format,
    va_list args
#endif
) {
    if (!context.global_hooks_enabled) {
        ze_hooked_func.zend_error_cb(
            type,
            error_filename,
            error_lineno,
#if PHP_API_VERSION >= 20200930
            message
#else
            format,
            args
#endif
        );

        return;
    }

    if (context.execution_disabled) {
        return;
    }

    context.error_count++;
    ze_hooked_func.zend_error_cb(
        type,
        error_filename,
        error_lineno,
#if PHP_API_VERSION >= 20200930
        message
#else
        format,
        args
#endif
    );
}

static void update_userland_stats(void)
{
    TSRMLS_FETCH();

    context.class_count = 0;
    context.function_count = 0;
    context.opcode_count = context.file_opcode_count;

    ZE_HASHTABLE_FOREACH(EG(class_table), entry, {
#if ZEND_MODULE_API_NO >= 20151012
        zval * zval_entry = entry;
        if (Z_TYPE_P(zval_entry) != IS_PTR) {
            continue;
        }

        zend_class_entry * ce = Z_PTR_P(zval_entry);
#else
        zend_class_entry * ce = *(zend_class_entry **)entry;
#endif

        if (ce->type != ZEND_USER_CLASS) {
            continue;
        }

        context.class_count++;

        ZE_HASHTABLE_FOREACH(&ce->function_table, entry, {
#if ZEND_MODULE_API_NO >= 20151012
            zval * zval_entry = entry;
            if (Z_TYPE_P(zval_entry) != IS_PTR) {
                continue;
            }

            zend_function * func = Z_PTR_P(zval_entry);
#else
            zend_function * func = entry;
#endif

            if (func->common.scope != ce) {
                continue;
            }

            context.function_count++;
            context.opcode_count += func->op_array.last;
        });
    });

    ZE_HASHTABLE_FOREACH(EG(function_table), entry, {
#if ZEND_MODULE_API_NO >= 20151012
        zval * zval_entry = entry;
        if (Z_TYPE_P(zval_entry) != IS_PTR) {
            continue;
        }

        zend_function * func = Z_PTR_P(zval_entry);
#else
        zend_function * func = entry;
#endif

        if (func->type != ZEND_USER_FUNCTION) {
            continue;
        }

        context.function_count++;
        context.opcode_count += func->op_array.last;
    });
}

static HashTable * get_global_array(const char * name)
{
#if ZEND_MODULE_API_NO >= 20151012
    zend_string * name_zs = zend_string_init(name, strlen(name), 0);

    zend_is_auto_global(name_zs);

    zval * zv_array = zend_hash_find(&EG(symbol_table), name_zs);
    if (!zv_array) {
        return NULL;
    }

    if (Z_TYPE_P(zv_array) != IS_ARRAY) {
        return NULL;
    }

    return Z_ARRVAL_P(zv_array);
#else
    TSRMLS_FETCH();

    zend_is_auto_global(name, strlen(name) TSRMLS_CC);

    zval ** zv_array;
    if (zend_hash_find(
        &EG(symbol_table),
        name,
        strlen(name) + 1,
        (void **) &zv_array
    ) != SUCCESS) {
        return NULL;
    }

    if (Z_TYPE_PP(zv_array) != IS_ARRAY) {
        return NULL;
    }

    return Z_ARRVAL_PP(zv_array);
#endif
}
