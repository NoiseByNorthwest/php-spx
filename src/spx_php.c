/* SPX - A simple profiler for PHP
 * Copyright (C) 2017-2025 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
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

/*
    Observer API, despite being available since PHP 8.0, is not enough stable
    with PHP 8.0 & 8.1. For instance tests/spx_auto_start_002_observer_api.phpt
    crashes with observer API and PHP 8.0-8.1.
    This is why SPX makes it available only for PHP 8.2+.
*/
#if ZEND_MODULE_API_NO >= 20220829
#   define HAVE_PHP_OBSERVER_API
#endif

#ifdef HAVE_PHP_OBSERVER_API
#   include "Zend/zend_observer.h"
#endif

#if defined(_WIN32) && ZEND_MODULE_API_NO >= 20170718
#   include "win32/console.h"
#endif

/* _GNU_SOURCE is implicitly defined since PHP 8.2 https://github.com/php/php-src/pull/8807 */
#ifndef _GNU_SOURCE
#   define _GNU_SOURCE /* vasprintf */
#endif

#include <stdio.h>

#include "spx_php.h"
#include "spx_thread.h"
#include "spx_str_builder.h"
#include "spx_utils.h"

#define ZE_HASHTABLE_FOREACH(ht, entry, block)                  \
do {                                                            \
    void * entry;                                               \
    zend_hash_internal_pointer_reset(ht);                       \
    while ((entry = zend_hash_get_current_data(ht)) != NULL) {  \
        zend_hash_move_forward(ht);                             \
        block                                                   \
    }                                                           \
} while (0)

typedef void (*execute_internal_func_t) (
    zend_execute_data * execute_data,
    zval * return_value
);

static struct {
    void (*execute_ex) (zend_execute_data * execute_data);
    execute_internal_func_t previous_zend_execute_internal;
    execute_internal_func_t execute_internal;

    zend_op_array * (*zend_compile_file)(zend_file_handle * file_handle, int type);
    zend_op_array * (*zend_compile_string)(
#if ZEND_MODULE_API_NO >= 20200930
        zend_string * source_string,
        const
#else
        zval * source_string,
#endif
        char * filename
#if ZEND_MODULE_API_NO >= 20210903
        , zend_compile_position position
#endif
    );

    int (*gc_collect_cycles)(void);

    void (*zend_error_cb) (
        int type,
#if ZEND_MODULE_API_NO >= 20210902
        zend_string * error_filename,
#else
        const char * error_filename,
#endif
        const uint error_lineno,
#if ZEND_MODULE_API_NO >= 20200930
        zend_string *message
#else
        const char *format,
        va_list args
#endif
    );
} ze_hooked_func = {
    NULL, NULL, NULL,
    NULL, NULL,
    NULL,
    NULL
};

static SPX_THREAD_TLS struct {
    void * (*malloc) (size_t size);
    void (*free) (void * ptr);
    void * (*realloc) (void * ptr, size_t size);
    size_t (*block_size) (void * ptr);
} ze_tls_hooked_func = {
    NULL, NULL, NULL, NULL
};

static SPX_THREAD_TLS struct {
    struct {
        void (*before)(void);
        void (*after)(void);
        int internal_functions;
    } ex_hook;

    int use_observer_api;
    int global_hooks_enabled;
    int execution_disabled;

    size_t depth;

    struct {
        union {
            const zend_execute_data * execute_data;
            const char * function_name;
        } data;
        uint8_t special_function;
    } stack[SPX_PHP_STACK_CAPACITY];

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
} context;

static void execute_data_function(
    const zend_execute_data * execute_data,
    spx_php_function_t * function
);

static void reset_context(void);

static size_t ze_mm_block_size(void * ptr);
static size_t ze_mm_custom_block_size(void * ptr);
static void * ze_mm_malloc(size_t size);
static void ze_mm_free(void * ptr);
static void * ze_mm_realloc(void * ptr, size_t size);

static void * tls_hook_malloc(size_t size);
static void tls_hook_free(void * ptr);
static void * tls_hook_realloc(void * ptr, size_t size);

static void global_hook_execute_ex(zend_execute_data * execute_data);

static void global_hook_execute_internal(
    zend_execute_data * execute_data,
    zval * return_value
);

#ifdef HAVE_PHP_OBSERVER_API
static zend_observer_fcall_handlers observer_api_init(zend_execute_data * execute_data);
static void observer_api_begin(zend_execute_data * execute_data);
static void observer_api_end(zend_execute_data * execute_data, zval * return_value);
#endif

static zend_op_array * global_hook_zend_compile_file(zend_file_handle * file_handle, int type);
static zend_op_array * global_hook_zend_compile_string(
#if ZEND_MODULE_API_NO >= 20200930
    zend_string * source_string,
    const
#else
    zval * source_string,
#endif
    char * filename
#if ZEND_MODULE_API_NO >= 20210903
    , zend_compile_position position
#endif
);

static int global_hook_gc_collect_cycles(void);

static void global_hook_zend_error_cb(
    int type,
#if ZEND_MODULE_API_NO >= 20210902
    zend_string * error_filename,
#else
    const char * error_filename,
#endif
    const uint error_lineno,
#if ZEND_MODULE_API_NO >= 20200930
    zend_string *message
#else
    const char *format,
    va_list args
#endif
);

static void push_frame(uint8_t special_function, void * data);
static void update_userland_stats(void);

static HashTable * get_global_array(const char * name);

int spx_php_is_cli_sapi(void)
{
    return 0 == strcmp(sapi_module.name, "cli");
}

int spx_php_are_ansi_sequences_supported(void)
{
    return
        spx_php_is_cli_sapi()
            && isatty(STDOUT_FILENO)
#if defined(_WIN32) && ZEND_MODULE_API_NO >= 20170718
            && php_win32_console_fileno_has_vt100(STDOUT_FILENO)
#endif
    ;
}

#if 0
void spx_php_print_stack(void)
{
    int i;

    fprintf(stderr, "\nActual stack:\n");

    const zend_execute_data * execute_data;

    execute_data = EG(current_execute_data);
    i = 0;
    while (execute_data) {
        i++;
        execute_data = execute_data->prev_execute_data;
    }

    const size_t depth = i;

    execute_data = EG(current_execute_data);
    i = 0;
    while (execute_data) {
        fprintf(
            stderr,
            " - depth = %3lu, execute_data = %p, flags: %b, type: %d",
            depth - i,
            execute_data,
            execute_data->func ? execute_data->func->common.fn_flags : 0,
            execute_data->func ? execute_data->func->common.type : 0
        );

        fprintf(stderr, ", execute_data->opline = %p", execute_data->opline);
        if (
            /* opline is corrupted in this case, is it a ZE bug ? */
            (intptr_t) execute_data->opline < 0xff
            /* opline seems to be corrupted in this case, is it a ZE bug ? */
            || ! (execute_data->func->common.fn_flags & (1 << 25))
        ) {
            fprintf(stderr, ", corrupted opline\n");
        } else {
            fprintf(
                stderr,
                ", execute_data->opline->lineno = %d\n",
                execute_data->opline ?
                    execute_data->opline->lineno : 0
            );
        }

        i++;

        execute_data = execute_data->prev_execute_data;
    }

    spx_php_function_t function;
    int current_depth;

    for (i = 0; i < 2; i++) {
        fprintf(stderr, "Tracked stack (%s):\n", i == 0 ? "short" : "full");

        current_depth = context.depth;

        while (current_depth >= 1) {
            if (context.stack[current_depth - 1].special_function) {
                fprintf(
                    stderr,
                    " - depth = %3d, special function = %s\n",
                    current_depth,
                    context.stack[current_depth - 1].data.function_name
                );
            } else {
                function.depth = current_depth;
                function.hash_code = 0;
                function.class_name = "";
                function.func_name = "";
                function.file_name = "";
                function.line = 0;

                execute_data = context.stack[current_depth - 1].data.execute_data;

                execute_data_function(
                    execute_data,
                    &function
                );

                fprintf(
                    stderr,
                    " - depth = %3d, execute_data = %p, internal = %s",
                    current_depth,
                    execute_data,
                    execute_data->func && execute_data->func->type == ZEND_INTERNAL_FUNCTION ?
                        "true" : "false"
                );

                if (i == 0) {
                    fprintf(stderr, "\n");
                } else {
                    if (
                        /* opline is corrupted in this case, is it a ZE bug ? */
                        (intptr_t) execute_data->opline < 0xff
                        /* opline seems to be corrupted in this case, is it a ZE bug ? */
                        || (
                            execute_data->func
                            && ! (execute_data->func->common.fn_flags & (1 << 25))
                        )
                    ) {
                        fprintf(stderr, ", corrupted opline\n");
                    } else {
                        fprintf(
                            stderr,
                            ", flags = %b, name = %s::%s, execute_data->opline->lineno = %d\n",
                            execute_data->func ? execute_data->func->common.fn_flags : 0,
                            function.class_name,
                            function.func_name,
                            execute_data->opline ?
                                execute_data->opline->lineno : 0
                        );
                    }
                }
            }

            current_depth--;
        }
    }
}
#endif

size_t spx_php_current_depth(void)
{
    return context.depth;
}

void spx_php_current_function(spx_php_function_t * function)
{
    spx_php_function_at(context.depth, function);
}

int spx_php_previous_function(const spx_php_function_t * current, spx_php_function_t * previous)
{
    if (current->depth == 1) {
        return 0;
    }

    spx_php_function_at(current->depth - 1, previous);

    return 1;
}

int spx_php_previous_userland_function(const spx_php_function_t * current, spx_php_function_t * previous)
{
    if (current->depth == 1) {
        return 0;
    }

    size_t prev_depth = current->depth - 1;

    for (;;) {
        spx_php_function_at(prev_depth, previous);
        if (! spx_php_is_internal_function(previous)) {
            break;
        }

        if (prev_depth == 0) {
            return 0;
        }

        prev_depth--;
    }

    return 1;
}

void spx_php_function_at(size_t depth, spx_php_function_t * function)
{
    if (depth > context.depth || depth == 0) {
        spx_utils_die("Invalid specified depth");
    }

    function->depth = depth;
    function->hash_code = 0;
    function->class_name = "";
    function->func_name = "";
    function->file_name = "";
    function->line = 0;

    if (context.stack[function->depth - 1].special_function) {
        function->class_name = "";
        function->func_name = context.stack[function->depth - 1].data.function_name;
        function->hash_code = zend_inline_hash_func(function->func_name, strlen(function->func_name));
    } else {
        execute_data_function(
            context.stack[function->depth - 1].data.execute_data,
            function
        );
    }
}

uint8_t spx_php_is_internal_function(const spx_php_function_t * function)
{
    if (context.stack[function->depth - 1].special_function) {
        return 1;
    }

    const zend_execute_data * execute_data = context.stack[function->depth - 1].data.execute_data;

    if (execute_data->func && execute_data->func->type == ZEND_INTERNAL_FUNCTION) {
        return 1;
    }

    return 0;
}

size_t spx_php_function_call_site_line(const spx_php_function_t * function)
{
    if (context.depth < 2) {
        return 0;
    }

    const zend_execute_data * prev_execute_data = NULL;
    int i;

    for (i = function->depth - 2; i >= 0; i--) {
        if (! context.stack[i].special_function) {
            prev_execute_data = context.stack[i].data.execute_data;

            break;
        }
    }

    while (
        prev_execute_data
            && prev_execute_data->func
            && prev_execute_data->func->type == ZEND_INTERNAL_FUNCTION
    ) {
        /*
            While the caller is an internal function (e.g. calling a callback) we have to take its caller
        */
        prev_execute_data = prev_execute_data->prev_execute_data;
    }

    if (! prev_execute_data)  {
        return 0;
    }

    return prev_execute_data->opline->lineno;
}

const char * spx_php_ini_get_string(const char * name)
{
    return zend_ini_string(
        /*
         * This cast is checked
         */
        (char *)name,
        strlen(name),
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
        strlen(name),
        0
    );
}

const char * spx_php_global_array_get(const char * name, const char * key)
{
    HashTable * global_array = get_global_array(name);
    if (!global_array) {
        return NULL;
    }

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
}

char * spx_php_build_command_line(void)
{
    HashTable * global_array = get_global_array("_SERVER");
    if (!global_array) {
        return NULL;
    }

    const char * argv_key = "argv";

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

    int i = 0;

    ZE_HASHTABLE_FOREACH(argv_array, entry, {
        zval * zval_entry = entry;
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
    return zend_memory_usage(0);
}

size_t spx_php_zend_memory_peak_usage(void)
{
    return zend_memory_peak_usage(0);
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
    size_t i, count = 0;
    for (i = 1; i < EG(objects_store).top; i++) {
        if (
            IS_OBJ_VALID(EG(objects_store).object_buckets[i])
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

void spx_php_global_hooks_init(void)
{
#ifdef HAVE_PHP_OBSERVER_API
    zend_observer_fcall_register(observer_api_init);
#endif
}

void spx_php_global_hooks_set(int use_observer_api)
{
#ifndef HAVE_PHP_OBSERVER_API
    use_observer_api = 0;
#endif

    if (!use_observer_api) {
        ze_hooked_func.execute_ex = zend_execute_ex;
        zend_execute_ex = global_hook_execute_ex;

        ze_hooked_func.previous_zend_execute_internal = zend_execute_internal;
        ze_hooked_func.execute_internal = zend_execute_internal ?
            zend_execute_internal : execute_internal
        ;
        zend_execute_internal = global_hook_execute_internal;
    }

    ze_hooked_func.zend_compile_file = zend_compile_file;
    zend_compile_file = global_hook_zend_compile_file;

    ze_hooked_func.zend_compile_string = zend_compile_string;
    zend_compile_string = global_hook_zend_compile_string;

    ze_hooked_func.gc_collect_cycles = gc_collect_cycles;
    gc_collect_cycles = global_hook_gc_collect_cycles;

    ze_hooked_func.zend_error_cb = zend_error_cb;
    zend_error_cb = global_hook_zend_error_cb;
}

void spx_php_global_hooks_unset(void)
{
    if (ze_hooked_func.execute_ex) {
        zend_execute_ex = ze_hooked_func.execute_ex;
        ze_hooked_func.execute_ex = NULL;
    }

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

    if (ze_hooked_func.gc_collect_cycles) {
        gc_collect_cycles = ze_hooked_func.gc_collect_cycles;
        ze_hooked_func.gc_collect_cycles = NULL;
    }

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

    if (context.ex_hook.after && context.ex_hook.internal_functions) {
        if (
            context.depth != 1
                || ! context.stack[context.depth - 1].special_function
                || strcmp(context.stack[context.depth - 1].data.function_name, "::php_request_shutdown") != 0
        ) {
            spx_utils_die("stack inconsistency");
        }

        context.ex_hook.after();
        context.depth--;
    }

    if (context.depth != 0) {
        spx_utils_die("Stack tracking inconsistency");
    }

    context.request_shutdown = 0;
}

void spx_php_execution_init(int use_observer_api)
{
    reset_context();

#ifndef HAVE_PHP_OBSERVER_API
    use_observer_api = 0;
#endif

    context.use_observer_api = use_observer_api;

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
}

void spx_php_execution_shutdown(void)
{
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

    reset_context();
}

void spx_php_execution_disable(void)
{
    context.execution_disabled = 1;
}

void spx_php_execution_hook(void (*before)(void), void (*after)(void), int internal_functions)
{
    context.ex_hook.before = before;
    context.ex_hook.after = after;
    context.ex_hook.internal_functions = internal_functions;
}

int spx_php_execution_hook_are_internal_functions_traced()
{
    return context.ex_hook.internal_functions;
}

void spx_php_output_add_header_line(const char * header_line)
{
    sapi_header_line ctr = {0};

    /*
        This cast is checked, header_line will be first duped before being manipulated.
        See sapi_header_op() implementation.
    */
    ctr.line = (char *)header_line;
    ctr.line_len = strlen(header_line);

    sapi_header_op(SAPI_HEADER_REPLACE, &ctr);
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
    sapi_send_headers();
}

size_t spx_php_output_direct_write(const void * ptr, size_t len)
{
    return sapi_module.ub_write(ptr, len);
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

static void execute_data_function(
    const zend_execute_data * execute_data,
    spx_php_function_t * function
) {
    int closure = 0;
    const zend_function * func = NULL;
    const zend_class_entry * ce = NULL;

    if (execute_data && zend_is_executing()) {
        func = execute_data->func;

        closure = func->common.fn_flags & ZEND_ACC_CLOSURE;

        if (func->type == ZEND_EVAL_CODE) {
            function->file_name = "eval()";
        } else if (func->common.type != ZEND_INTERNAL_FUNCTION) {
            function->file_name = ZSTR_VAL(func->op_array.filename);
            if (
                /* FIXME check if this case still occurs since the addition of the "type != ZEND_INTERNAL_FUNCTION" check above */
                /* ZE bug causing func->op_array.filename to be corrupted ?*/
                (intptr_t) function->file_name < 0xff
            ) {
                function->file_name = "";
            }
        }

        function->line = func->op_array.line_start;

        switch (func->type) {
            case ZEND_USER_FUNCTION:
            case ZEND_INTERNAL_FUNCTION:
            {
                ce = func->common.scope;
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
    }

    if (!function->func_name[0]) {
        function->class_name = "";

        while (execute_data && (!execute_data->func || !ZEND_USER_CODE(execute_data->func->type))) {
            execute_data = execute_data->prev_execute_data;
        }

        if (execute_data) {
            function->func_name = ZSTR_VAL(execute_data->func->op_array.filename);
        } else {
            function->func_name = "[no active file]";
        }
    }

    if (func && ! closure) {
        /*
            Hashing the (non-closure) function address is safe since it always point to the same
            function table entry for the whole script's lifespan.
        */
        function->hash_code = zend_inline_hash_func((void *)&func, sizeof(void *));
        if (ce && ce->ce_flags & ZEND_ACC_ANON_CLASS) {
            function->hash_code ^= zend_inline_hash_func(function->class_name, strlen(function->class_name));
        }
    } else {
        function->hash_code =
            zend_inline_hash_func(function->file_name, strlen(function->file_name))
                ^ zend_inline_hash_func((void *) &function->line, sizeof(function->line))
        ;
    }
}

static void reset_context(void)
{
    context.ex_hook.before = NULL;
    context.ex_hook.after = NULL;
    context.ex_hook.internal_functions = 0;

    context.use_observer_api = 0;
    context.global_hooks_enabled = 1;
    context.execution_disabled = 0;
    context.depth = 0;
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

static void global_hook_execute_ex(zend_execute_data * execute_data)
{
    if (!context.global_hooks_enabled) {
        ze_hooked_func.execute_ex(execute_data);

        return;
    }

    if (context.execution_disabled) {
        return;
    }

    if (context.use_observer_api) {
        ze_hooked_func.execute_ex(execute_data);

        return;
    }

    push_frame(0, execute_data);

    if (context.ex_hook.before) {
        context.ex_hook.before();
    }

    ze_hooked_func.execute_ex(execute_data);

    if (context.ex_hook.after) {
        context.ex_hook.after();
    }

    context.depth--;

    /*
     *  FIXME: it might not works with prepend files
     */
    if (context.depth == 0 && !context.request_shutdown) {
        context.request_shutdown = 1;

        if (context.ex_hook.internal_functions) {
            push_frame(1, "::php_request_shutdown");
            context.ex_hook.before();
        }
    }
}

static void global_hook_execute_internal(
    zend_execute_data * execute_data,
    zval * return_value
) {
    if (!context.global_hooks_enabled) {
        ze_hooked_func.execute_internal(
            execute_data,
            return_value
        );

        return;
    }

    if (context.execution_disabled) {
        return;
    }

    if (context.use_observer_api) {
        ze_hooked_func.execute_internal(
            execute_data,
            return_value
        );

        return;
    }

    push_frame(0, execute_data);

    if (
        context.ex_hook.internal_functions
            && context.ex_hook.before
    ) {
        context.ex_hook.before();
    }

    ze_hooked_func.execute_internal(
        execute_data,
        return_value
    );

    if (
        context.ex_hook.internal_functions
            && context.ex_hook.after
    ) {
        context.ex_hook.after();
    }

    context.depth--;
}

#ifdef HAVE_PHP_OBSERVER_API

static zend_observer_fcall_handlers observer_api_init(zend_execute_data * execute_data)
{
    zend_observer_fcall_handlers handlers;

    /*
        Returning {null,null} handlers in cases where there is nothing to instrument
        counter-intuitively degrades performance.
        This why such cases are directly handled in observer_api_begin() & observer_api_end()
        via early returns.
    */

    handlers.begin = observer_api_begin;
    handlers.end = observer_api_end;

    return handlers;
}

static void observer_api_begin(zend_execute_data * execute_data)
{
    if (!context.use_observer_api) {
        /* No noticeable instrumentation overhead when returning right here. */
        return;
    }

    if (!context.global_hooks_enabled) {
        return;
    }

    push_frame(0, execute_data);

    if (
        context.ex_hook.before && (
            context.ex_hook.internal_functions
                || ! (
                    execute_data->func
                        && execute_data->func->type == ZEND_INTERNAL_FUNCTION
                )
        )
    ) {
        context.ex_hook.before();
    }
}

static void observer_api_end(zend_execute_data * execute_data, zval * return_value)
{
    if (!context.use_observer_api) {
        /* No noticeable instrumentation overhead when returning right here. */
        return;
    }

    if (!context.global_hooks_enabled) {
        return;
    }

    if (
        context.ex_hook.after && (
            context.ex_hook.internal_functions
                || ! (
                    execute_data->func
                        && execute_data->func->type == ZEND_INTERNAL_FUNCTION
                )
        )
    ) {
        context.ex_hook.after();
    }

    context.depth--;

    if (! context.ex_hook.internal_functions) {
        return;
    }

    /*
     *  FIXME: it might not works with prepend files
     */
    if (context.depth == 0 && !context.request_shutdown) {
        context.request_shutdown = 1;

        if (context.ex_hook.before) {
            push_frame(1, "::php_request_shutdown");
            context.ex_hook.before();
        }
    }
}

#endif

static zend_op_array * global_hook_zend_compile_file(zend_file_handle * file_handle, int type)
{
    if (!context.global_hooks_enabled) {
        return ze_hooked_func.zend_compile_file(file_handle, type);
    }

    if (context.execution_disabled) {
        return NULL;
    }

    if (
        ! context.ex_hook.before
        || ! context.ex_hook.after
        || ! context.ex_hook.internal_functions
    ) {
        return ze_hooked_func.zend_compile_file(file_handle, type);
    }

    push_frame(1, "::zend_compile_file");
    context.ex_hook.before();

    zend_op_array * op_array = ze_hooked_func.zend_compile_file(file_handle, type);

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

    context.ex_hook.after();
    context.depth--;

    return op_array;
}

static zend_op_array * global_hook_zend_compile_string(
#if ZEND_MODULE_API_NO >= 20200930
    zend_string * source_string,
    const
#else
    zval * source_string,
#endif
    char * filename
#if ZEND_MODULE_API_NO >= 20210903
    , zend_compile_position position
#endif
) {
    if (!context.global_hooks_enabled) {
        return ze_hooked_func.zend_compile_string(
            source_string,
            filename
#if ZEND_MODULE_API_NO >= 20210903
            , position
#endif
        );
    }

    if (context.execution_disabled) {
        return NULL;
    }

    if (
        ! context.ex_hook.before
        || ! context.ex_hook.after
        || ! context.ex_hook.internal_functions
    ) {
        return ze_hooked_func.zend_compile_string(
            source_string,
            filename
#if ZEND_MODULE_API_NO >= 20210903
            , position
#endif
        );
    }

    push_frame(1, "::zend_compile_string");
    context.ex_hook.before();

    zend_op_array * op_array = ze_hooked_func.zend_compile_string(
        source_string,
        filename
#if ZEND_MODULE_API_NO >= 20210903
        , position
#endif
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

    context.ex_hook.after();
    context.depth--;

    return op_array;
}

static int global_hook_gc_collect_cycles(void)
{
    if (!context.global_hooks_enabled) {
        return ze_hooked_func.gc_collect_cycles();
    }

    if (context.execution_disabled) {
        return 0;
    }

    if (
        ! context.ex_hook.before
        || ! context.ex_hook.after
        || ! context.ex_hook.internal_functions
    ) {
        return ze_hooked_func.gc_collect_cycles();
    }

    push_frame(1, "::gc_collect_cycles");
    context.ex_hook.before();

    const int count = ze_hooked_func.gc_collect_cycles();

    context.ex_hook.after();
    context.depth--;

    return count;
}

static void global_hook_zend_error_cb(
    int type,
#if ZEND_MODULE_API_NO >= 20210902
    zend_string * error_filename,
#else
    const char * error_filename,
#endif
    const uint error_lineno,
#if ZEND_MODULE_API_NO >= 20200930
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
#if ZEND_MODULE_API_NO >= 20200930
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
#if ZEND_MODULE_API_NO >= 20200930
        message
#else
        format,
        args
#endif
    );
}

static void push_frame(uint8_t special_function, void * data)
{
    if (context.depth == SPX_PHP_STACK_CAPACITY) {
        spx_utils_die("SPX_PHP_STACK_CAPACITY exceeded");
    }

    context.depth++;
    context.stack[context.depth - 1].special_function = special_function;
    context.stack[context.depth - 1].data.execute_data = data;
}

static void update_userland_stats(void)
{
    context.class_count = 0;
    context.function_count = 0;
    context.opcode_count = context.file_opcode_count;

    ZE_HASHTABLE_FOREACH(EG(class_table), entry, {
        zval * zval_entry = entry;
        if (Z_TYPE_P(zval_entry) != IS_PTR) {
            continue;
        }

        zend_class_entry * ce = Z_PTR_P(zval_entry);

        if (ce->type != ZEND_USER_CLASS) {
            continue;
        }

        context.class_count++;

        ZE_HASHTABLE_FOREACH(&ce->function_table, entry, {
            zval * zval_entry = entry;
            if (Z_TYPE_P(zval_entry) != IS_PTR) {
                continue;
            }

            zend_function * func = Z_PTR_P(zval_entry);

            if (func->common.scope != ce) {
                continue;
            }

            context.function_count++;
            context.opcode_count += func->op_array.last;
        });
    });

    ZE_HASHTABLE_FOREACH(EG(function_table), entry, {
        zval * zval_entry = entry;
        if (Z_TYPE_P(zval_entry) != IS_PTR) {
            continue;
        }

        zend_function * func = Z_PTR_P(zval_entry);

        if (func->type != ZEND_USER_FUNCTION) {
            continue;
        }

        context.function_count++;
        context.opcode_count += func->op_array.last;
    });
}

static HashTable * get_global_array(const char * name)
{
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
}
