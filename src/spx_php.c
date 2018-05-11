#define _GNU_SOURCE /* vasprintf */
#include <stdio.h>
#undef _GNU_SOURCE /* to avoid clash in main/php_config.h */

#include "main/php.h"
#include "main/SAPI.h"

#include "spx_php.h"
#include "spx_thread.h"
#include "spx_str_builder.h"

#if ZEND_MODULE_API_NO >= 20151012
typedef size_t zend_write_len_t;
#else
typedef uint zend_write_len_t;
#endif

static struct {
    zend_write_func_t zend_write;

    void (*execute_ex) (zend_execute_data * execute_data TSRMLS_DC);
    void (*execute_internal) (
        zend_execute_data * execute_data,
#if ZEND_MODULE_API_NO >= 20151012
        zval * return_value
#else
        struct _zend_fcall_info * fci,
        int ret
#endif
        TSRMLS_DC
    );

    int (*gc_collect_cycles)(void);

    void (*zend_error_cb) (
        int type,
        const char *error_filename,
        const uint error_lineno,
        const char *format,
        va_list args
    );
} ze_hook = {
    NULL,
    NULL, NULL,
    NULL,
    NULL
};

static SPX_THREAD_TLS struct {
    struct {
        struct {
            void (*before)(void);
            void (*after)(void);
        } user, internal;
    } ex_hook;

    int execution_disabled;

    size_t error_count;
    int gc_active;

    struct {
        void (*handler) (INTERNAL_FUNCTION_PARAMETERS);
#if ZEND_MODULE_API_NO >= 20151012
        zend_internal_arg_info * arg_info;
        uint32_t num_args;
        uint32_t fn_flags;
#else
        zend_arg_info * arg_info;
#endif
    } fastcgi_finish_request;
} context;

static int hook_zend_write(const char * str, zend_write_len_t len);

static void hook_execute_ex(zend_execute_data * execute_data TSRMLS_DC);
static void hook_execute_internal(
    zend_execute_data * execute_data,
#if ZEND_MODULE_API_NO >= 20151012
    zval * return_value
#else
    struct _zend_fcall_info * fci,
    int ret
#endif
    TSRMLS_DC
);

#if ZEND_MODULE_API_NO >= 20151012
static int hook_gc_collect_cycles(void);
#endif

static void hook_zend_error_cb(
    int type,
    const char *error_filename,
    const uint error_lineno,
    const char *format,
    va_list args
);

static HashTable * get_global_array(const char * name);
static size_t get_array_size(HashTable * ht);

int spx_php_is_cli_sapi(void)
{
    return 0 == strcmp(sapi_module.name, "cli");
}

void spx_php_current_function(spx_php_function_t * function)
{
    TSRMLS_FETCH();

    if (context.gc_active) {
        function->class_name = "";
        function->call_type = "";
        function->func_name = "gc_collect_cycles";
    } else {
        function->file_name = zend_get_executed_filename(TSRMLS_C);
        function->line = zend_get_executed_lineno(TSRMLS_C);

        function->class_name = get_active_class_name(&function->call_type TSRMLS_CC);
        function->func_name = get_active_function_name(TSRMLS_C);

#if ZEND_MODULE_API_NO >= 20151012
        const zend_function * func = EG(current_execute_data)->func;
        /*
         *  Required for PHP 7+ to avoid function name default'd to "main" in this case
         *  (including file level code).
         *  See get_active_function_name() implementation in php-src.
         */
        if (func->type == ZEND_USER_FUNCTION && !func->common.function_name) {
            function->func_name = NULL;
        }
        /*
         *  This hack is required for PHP 7.1 to prevent a segfault while dereferencing function->func_name
         *  TODO: open an issue if not yet tracked
         */
        if (func->type == ZEND_INTERNAL_FUNCTION && !func->common.function_name) {
            function->func_name = NULL;
        }
#endif

        if (!function->func_name) {
            function->class_name = "";
            function->call_type = "";
            function->func_name = function->file_name;
        }
    }

    function->hash_code =
        zend_inline_hash_func(function->func_name, strlen(function->func_name)) ^
        zend_inline_hash_func(function->class_name, strlen(function->class_name))
    ;
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

    spx_str_builder_t * str_builder = spx_str_builder_create(1024);
    if (!str_builder) {
        goto error;
    }

    HashTable * argv_array = Z_ARRVAL_P(argv);
    zval * entry;
    int i = 0;

    zend_hash_internal_pointer_reset(argv_array);
    while ((entry = zend_hash_get_current_data(argv_array)) != NULL) {
        if (Z_TYPE_P(entry) == IS_STRING) {
            if (i++ > 0) {
                spx_str_builder_append_char(str_builder, ' ');
            }

            if (0 == spx_str_builder_append_str(str_builder, Z_STRVAL_P(entry))) {
                break;
            }
        }

        zend_hash_move_forward(argv_array);
    }
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

    spx_str_builder_t * str_builder = spx_str_builder_create(1024);
    if (!str_builder) {
        goto error;
    }

    HashTable * argv_array = Z_ARRVAL_PP(argv);
    HashPosition pos;
    zval ** entry;
    int i = 0;

    zend_hash_internal_pointer_reset_ex(argv_array, &pos);
    while (zend_hash_get_current_data_ex(argv_array, (void **)&entry, &pos) == SUCCESS) {
        if (Z_TYPE_PP(entry) == IS_STRING) {
            if (i++ > 0) {
                spx_str_builder_append_char(str_builder, ' ');
            }

            if (0 == spx_str_builder_append_str(str_builder, Z_STRVAL_PP(entry))) {
                break;
            }
        }

        zend_hash_move_forward_ex(argv_array, &pos);
    }
#endif

    char * command_line = strdup(spx_str_builder_str(str_builder));
    spx_str_builder_destroy(str_builder);

    return command_line;

error:
    return strdup("n/a");
}

size_t spx_php_zend_memory_usage(void)
{
    TSRMLS_FETCH();

    return zend_memory_usage(0 TSRMLS_CC);
}

size_t spx_php_zend_root_buffer_length(void)
{
    TSRMLS_FETCH();

    size_t length = 0;
    const gc_root_buffer * current = GC_G(roots).next;

    while (current != &GC_G(roots)) {
        length++;
        current = current->next;
    }

    return length;
}

size_t spx_php_zend_included_file_count(void)
{
    return get_array_size(&EG(included_files));
}

size_t spx_php_zend_class_count(void)
{
    return get_array_size(EG(class_table));
}

size_t spx_php_zend_function_count(void)
{
    return get_array_size(EG(function_table));
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

void spx_php_hooks_init(void)
{
    ze_hook.zend_write = zend_write;
    zend_write = hook_zend_write;

    ze_hook.execute_ex = execute_ex;
    zend_execute_ex = hook_execute_ex;

    ze_hook.execute_internal = execute_internal;
    zend_execute_internal = hook_execute_internal;

#if ZEND_MODULE_API_NO >= 20151012
    ze_hook.gc_collect_cycles = gc_collect_cycles;
    gc_collect_cycles = hook_gc_collect_cycles;
#endif

    ze_hook.zend_error_cb = zend_error_cb;
    zend_error_cb = hook_zend_error_cb;
}

void spx_php_hooks_shutdown(void)
{
    if (ze_hook.zend_write) {
        zend_write = ze_hook.zend_write;
        ze_hook.zend_write = NULL;
    }

    if (ze_hook.execute_ex) {
        zend_execute_ex = ze_hook.execute_ex;
        ze_hook.execute_ex = NULL;
    }

    if (ze_hook.execute_internal) {
        zend_execute_internal = ze_hook.execute_internal;
        ze_hook.execute_internal = NULL;
    }

#if ZEND_MODULE_API_NO >= 20151012
    if (ze_hook.gc_collect_cycles) {
        gc_collect_cycles = ze_hook.gc_collect_cycles;
        ze_hook.gc_collect_cycles = NULL;
    }
#endif

    if (ze_hook.zend_error_cb) {
        zend_error_cb = ze_hook.zend_error_cb;
        ze_hook.zend_error_cb = NULL;
    }
}

void spx_php_execution_init(void)
{
    context.ex_hook.user.before = NULL;
    context.ex_hook.user.after = NULL;
    context.ex_hook.internal.before = NULL;
    context.ex_hook.internal.after = NULL;

    context.execution_disabled = 0;
    context.error_count = 0;
    context.gc_active = 0;
}

void spx_php_execution_shutdown(void)
{
    spx_php_execution_init();
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

static int hook_zend_write(const char * str, zend_write_len_t len)
{
    return ze_hook.zend_write(str, len);
}

static void hook_execute_ex(zend_execute_data * execute_data TSRMLS_DC)
{
    if (context.execution_disabled) {
        return;
    }

    if (context.ex_hook.user.before) {
        context.ex_hook.user.before();
    }

    ze_hook.execute_ex(execute_data TSRMLS_CC);

    if (context.ex_hook.user.after) {
        context.ex_hook.user.after();
    }
}

static void hook_execute_internal(
    zend_execute_data * execute_data,
#if ZEND_MODULE_API_NO >= 20151012
    zval * return_value
#else
    struct _zend_fcall_info * fci,
    int ret
#endif
    TSRMLS_DC
) {
    if (context.execution_disabled) {
        return;
    }

    if (context.ex_hook.internal.before) {
        context.ex_hook.internal.before();
    }

    ze_hook.execute_internal(
        execute_data,
#if ZEND_MODULE_API_NO >= 20151012
        return_value
#else
        fci,
        ret
#endif
        TSRMLS_CC
    );

    if (context.ex_hook.internal.after) {
        context.ex_hook.internal.after();
    }
}

#if ZEND_MODULE_API_NO >= 20151012
static int hook_gc_collect_cycles(void)
{
    if (context.execution_disabled) {
        return 0;
    }

    context.gc_active = 1;

    if (context.ex_hook.user.before) {
        context.ex_hook.user.before();
    }

    const int count = ze_hook.gc_collect_cycles();

    if (context.ex_hook.user.after) {
        context.ex_hook.user.after();
    }

    context.gc_active = 0;

    return count;
}
#endif

static void hook_zend_error_cb(
    int type,
    const char *error_filename,
    const uint error_lineno,
    const char *format,
    va_list args
) {
    context.error_count++;
    ze_hook.zend_error_cb(type, error_filename, error_lineno, format, args);
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

static size_t get_array_size(HashTable * ht)
{
#if ZEND_MODULE_API_NO >= 20151012
    return (size_t) zend_array_count(ht);
#else
    return (size_t) zend_hash_num_elements(ht);
#endif
}
