#include "main/php.h"
#include "main/SAPI.h"
#include "Zend/zend_extensions.h"

#include "spx_php.h"
#include "spx_thread.h"

#if ZEND_EXTENSION_API_NO >= 320151012
typedef size_t zend_write_len_t;
#else
typedef uint zend_write_len_t;
#endif

static struct {
    zend_write_func_t zend_write;
    int  (*send_headers) (sapi_headers_struct * sapi_headers);
    void (*send_header)  (sapi_header_struct * sapi_header, void * server_context);
    void (*flush)        (void * server_context);

    void (*execute_ex) (zend_execute_data * execute_data);
    void (*execute_internal) (
        zend_execute_data * execute_data,
#if ZEND_EXTENSION_API_NO >= 320151012
        zval * return_value
#else
        struct _zend_fcall_info * fci,
        int ret
#endif
    );

    void (*zend_error_cb) (
        int type,
        const char *error_filename,
        const uint error_lineno,
        const char *format,
        va_list args
    );
} ze_hook = {
    NULL, NULL, NULL, NULL,
    NULL, NULL,
    NULL
};

static SPX_THREAD_TLS struct {
    struct {
        struct {
            void (*before)(void);
            void (*after)(void);
        } user, internal;
    } ex_hook;

    size_t error_count;

    int output_disabled;
    struct {
        void (*handler) (INTERNAL_FUNCTION_PARAMETERS);
#if ZEND_EXTENSION_API_NO >= 320151012
        zend_internal_arg_info * arg_info;
        uint32_t num_args;
        uint32_t fn_flags;
#else
        zend_arg_info * arg_info;
#endif
    } fastcgi_finish_request;
} context;

static int hook_zend_write(const char * str, zend_write_len_t len);
static int hook_send_headers(sapi_headers_struct * sapi_headers);
static void hook_send_header(sapi_header_struct * sapi_header, void * server_context);
static void hook_flush(void * server_context);

static void hook_execute_ex(zend_execute_data * execute_data TSRMLS_DC);
static void hook_execute_internal(
    zend_execute_data * execute_data,
#if ZEND_EXTENSION_API_NO >= 320151012
    zval * return_value
#else
    struct _zend_fcall_info * fci,
    int ret
#endif
    TSRMLS_DC
);

static void hook_zend_error_cb(
    int type,
    const char *error_filename,
    const uint error_lineno,
    const char *format,
    va_list args
);

static PHP_FUNCTION(null_zend_function);
static zend_internal_function * get_zend_internal_function(const char * name);

void spx_php_current_function(spx_php_function_t * function)
{
    TSRMLS_FETCH();

    function->file_name = zend_get_executed_filename(TSRMLS_C);
    function->line = zend_get_executed_lineno(TSRMLS_C);

    function->class_name = get_active_class_name(&function->call_type TSRMLS_CC);
    function->func_name = get_active_function_name(TSRMLS_C);

#if ZEND_EXTENSION_API_NO >= 320151012
    /*
     *  This hack is required for PHP 7.1 to prevent a segfault while dereferencing function->func_name
     *  TODO: open an issue if not yet tracked
     */
    const zend_function * func = EG(current_execute_data)->func;
    if (func->type == ZEND_INTERNAL_FUNCTION && !func->common.function_name) {
        function->func_name = NULL;
    }
#endif

    if (!function->func_name) {
        /* approximation of include* / require* & eval */
        function->func_name = function->file_name;
    }

    function->hash_code =
        zend_inline_hash_func(function->func_name, strlen(function->func_name)) ^
        zend_inline_hash_func(function->class_name, strlen(function->class_name))
    ;
}

const char * spx_php_global_array_get(const char * global_array_name, const char * key)
{
#if ZEND_EXTENSION_API_NO >= 320151012
    zend_string * global_array_name_zs = zend_string_init(global_array_name, strlen(global_array_name), 0);

    zend_is_auto_global(global_array_name_zs);
    zval * global_array = zend_hash_find(&EG(symbol_table), global_array_name_zs);
    if (!global_array) {
        return NULL;
    }

    zval * pv = zend_hash_str_find(
        Z_ARRVAL_P(global_array),
        key,
        strlen(key)
    );

    if (!pv) {
        return NULL;
    }

    convert_to_string_ex(pv);

    return Z_STRVAL_P(pv);
#else
    TSRMLS_FETCH();

    zend_is_auto_global(global_array_name, strlen(global_array_name) TSRMLS_CC);

    zval ** global_array;
    if (zend_hash_find(
        &EG(symbol_table),
        global_array_name,
        strlen(global_array_name) + 1,
        (void **) &global_array
    ) != SUCCESS) {
        return NULL;
    }

    zval ** ppv;
    if (
        zend_hash_find(
            Z_ARRVAL_PP(global_array),
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

size_t spx_php_zend_object_count(void)
{
    TSRMLS_FETCH();

    size_t i, count = 0;
    for (i = 1; i < EG(objects_store).top; i++) {
        if (
#if ZEND_EXTENSION_API_NO >= 320151012
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

    if (sapi_module.send_headers) {
        ze_hook.send_headers = sapi_module.send_headers;
        sapi_module.send_headers = hook_send_headers;
    }

    if (sapi_module.send_header) {
        ze_hook.send_header = sapi_module.send_header;
        sapi_module.send_header = hook_send_header;
    }

    if (sapi_module.flush) {
        ze_hook.flush = sapi_module.flush;
        sapi_module.flush = hook_flush;
    }

    ze_hook.execute_ex = execute_ex;
    zend_execute_ex = hook_execute_ex;

    ze_hook.execute_internal = execute_internal;
    zend_execute_internal = hook_execute_internal;

    ze_hook.zend_error_cb = zend_error_cb;
    zend_error_cb = hook_zend_error_cb;
}

void spx_php_hooks_shutdown(void)
{
    if (ze_hook.zend_write) {
        zend_write = ze_hook.zend_write;
        ze_hook.zend_write = NULL;
    }

    if (ze_hook.send_headers) {
        sapi_module.send_headers = ze_hook.send_headers;
        ze_hook.send_headers = NULL;
    }

    if (ze_hook.send_header) {
        sapi_module.send_header = ze_hook.send_header;
        ze_hook.send_header = NULL;
    }

    if (ze_hook.flush) {
        sapi_module.flush = ze_hook.flush;
        ze_hook.flush = NULL;
    }

    if (ze_hook.execute_ex) {
        zend_execute_ex = ze_hook.execute_ex;
        ze_hook.execute_ex = NULL;
    }

    if (ze_hook.execute_internal) {
        zend_execute_internal = ze_hook.execute_internal;
        ze_hook.execute_internal = NULL;
    }

    if (ze_hook.zend_error_cb) {
        zend_error_cb = ze_hook.zend_error_cb;
        ze_hook.zend_error_cb = NULL;
    }
}

void spx_php_context_init(void)
{
    context.ex_hook.user.before = NULL;
    context.ex_hook.user.after = NULL;
    context.ex_hook.internal.before = NULL;
    context.ex_hook.internal.after = NULL;

    context.error_count = 0;
    context.output_disabled = 0;
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

void spx_php_output_disable(void)
{
    if (context.output_disabled == 1) {
        return;
    }

    context.output_disabled = 1;

    /*
     *  fastcgi_finish_request() function nullification.
     *  Required to prevent user land PHP code to finalize/close FPM output before end
     *  of script and thus forbidding SPX to send additional output.
     */
    context.fastcgi_finish_request.handler = NULL;
    context.fastcgi_finish_request.arg_info = NULL;
    zend_internal_function * func = get_zend_internal_function("fastcgi_finish_request");
    if (func) {
        context.fastcgi_finish_request.handler = func->handler;
        context.fastcgi_finish_request.arg_info = func->arg_info;
#if ZEND_EXTENSION_API_NO >= 320151012
        context.fastcgi_finish_request.num_args = func->num_args;
        context.fastcgi_finish_request.fn_flags = func->fn_flags;
#endif

        func->handler = PHP_FN(null_zend_function);
        func->arg_info = NULL;
#if ZEND_EXTENSION_API_NO >= 320151012
        func->num_args = 0;
        func->fn_flags &= ~(ZEND_ACC_VARIADIC | ZEND_ACC_HAS_TYPE_HINTS);
#endif
    }
}

void spx_php_output_restore(void)
{
    if (context.output_disabled == 0) {
        return;
    }

    context.output_disabled = 0;

    zend_internal_function * func = get_zend_internal_function("fastcgi_finish_request");
    if (func && context.fastcgi_finish_request.handler) {
        func->handler = context.fastcgi_finish_request.handler;
        func->arg_info = context.fastcgi_finish_request.arg_info;
#if ZEND_EXTENSION_API_NO >= 320151012
        func->num_args = context.fastcgi_finish_request.num_args;
        func->fn_flags = context.fastcgi_finish_request.fn_flags;
#endif

        context.fastcgi_finish_request.handler = NULL;
        context.fastcgi_finish_request.arg_info = NULL;
    }
}

size_t spx_php_output_direct_write(const void * ptr, size_t len)
{
    return sapi_module.ub_write(ptr, len);
}

size_t spx_php_output_direct_print(const char * str)
{
    return spx_php_output_direct_write(str, strlen(str));
}

void spx_php_ouput_finalize(void)
{
    TSRMLS_FETCH();

    /*
     *  This side effect is required to avoid ZE sending default/user headers right
     *  after extensions RSHUTDOWN handlers, as it is done in output layer shutdown
     *  step (see php_request_shutdown()).
     */
    SG(headers_sent) = 1;
}

static int hook_zend_write(const char * str, zend_write_len_t len)
{
    if (context.output_disabled) {
        return len;
    }

    return ze_hook.zend_write(str, len);
}

static int hook_send_headers(sapi_headers_struct * sapi_headers)
{
    if (context.output_disabled) {
        return SAPI_HEADER_SENT_SUCCESSFULLY;
    }

    return ze_hook.send_headers(sapi_headers);
}

static void hook_send_header(sapi_header_struct * sapi_header, void * server_context)
{
    if (context.output_disabled) {
        return;
    }

    ze_hook.send_header(sapi_header, server_context);
}

static void hook_flush(void * server_context)
{
    if (context.output_disabled) {
        return;
    }

    ze_hook.flush(server_context);
}

static void hook_execute_ex(zend_execute_data * execute_data TSRMLS_DC)
{
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
#if ZEND_EXTENSION_API_NO >= 320151012
    zval * return_value
#else
    struct _zend_fcall_info * fci,
    int ret
#endif
    TSRMLS_DC
) {
    if (context.ex_hook.internal.before) {
        context.ex_hook.internal.before();
    }

    ze_hook.execute_internal(
        execute_data,
#if ZEND_EXTENSION_API_NO >= 320151012
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

static PHP_FUNCTION(null_zend_function)
{
    RETURN_TRUE;
}

static zend_internal_function * get_zend_internal_function(const char * name)
{
#if ZEND_EXTENSION_API_NO >= 320151012
    return zend_hash_str_find_ptr(
        CG(function_table),
        name,
        strlen(name)
    );
#else
    TSRMLS_FETCH();

    zend_internal_function * func;
    if (
        zend_hash_find(
            CG(function_table),
            name,
            strlen(name) + 1,
            (void **)&func
        ) == SUCCESS
    ) {
        return func;
    }

    return NULL;
#endif
}
