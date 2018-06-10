#include <stdio.h>
#include <unistd.h>

#ifndef ZTS
#   define USE_SIGNAL
#endif

#ifdef USE_SIGNAL
#   include <signal.h>
#endif

#include "php_spx.h"
#include "ext/standard/info.h"

#include "spx_thread.h"
#include "spx_config.h"
#include "spx_php.h"
#include "spx_utils.h"
#include "spx_metric.h"
#include "spx_resource_stats.h"
#include "spx_profiler_tracer.h"
#include "spx_profiler_sampler.h"
#include "spx_reporter_fp.h"
#include "spx_reporter_full.h"
#include "spx_reporter_trace.h"

typedef struct {
    void (*init) (void);
    void (*shutdown) (void);
} execution_handler_t;

static SPX_THREAD_TLS struct {
    int cli_sapi;
    spx_config_t config;

    execution_handler_t * execution_handler;

    struct {
#ifdef USE_SIGNAL
        struct {
            int handler_set;
            struct {
                struct sigaction sigint;
                struct sigaction sigterm;
            } prev_handler;

            volatile sig_atomic_t handler_called;
            volatile sig_atomic_t probing;
            volatile sig_atomic_t stop;
            int signo;
        } sig_handling;
#endif

        spx_profiler_reporter_t * reporter;
        spx_profiler_t * profiler;
    } profiling_handler;
} context;

ZEND_BEGIN_MODULE_GLOBALS(spx)
    const char * data_dir;
    zend_bool http_enabled;
    const char * http_key;
    const char * http_ip_var;
    const char * http_ip_whitelist;
    const char * http_ui_assets_dir;
    const char * http_ui_uri_prefix;
ZEND_END_MODULE_GLOBALS(spx)

ZEND_DECLARE_MODULE_GLOBALS(spx)

#ifdef ZTS
#   define SPX_G(v) TSRMG(spx_globals_id, zend_spx_globals *, v)
#else
#   define SPX_G(v) (spx_globals.v)
#endif

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY(
        "spx.data_dir", "/tmp/spx", PHP_INI_SYSTEM,
        OnUpdateString, data_dir, zend_spx_globals, spx_globals
    )
    STD_PHP_INI_ENTRY(
        "spx.http_enabled", "0", PHP_INI_SYSTEM,
        OnUpdateBool, http_enabled, zend_spx_globals, spx_globals
    )
    STD_PHP_INI_ENTRY(
        "spx.http_key", "", PHP_INI_SYSTEM,
        OnUpdateString, http_key, zend_spx_globals, spx_globals
    )
    STD_PHP_INI_ENTRY(
        "spx.http_ip_var", "REMOTE_ADDR", PHP_INI_SYSTEM,
        OnUpdateString, http_ip_var, zend_spx_globals, spx_globals
    )
    STD_PHP_INI_ENTRY(
        "spx.http_ip_whitelist", "", PHP_INI_SYSTEM,
        OnUpdateString, http_ip_whitelist, zend_spx_globals, spx_globals
    )
    STD_PHP_INI_ENTRY(
        "spx.http_ui_assets_dir", SPX_HTTP_UI_ASSETS_DIR, PHP_INI_SYSTEM,
        OnUpdateString, http_ui_assets_dir, zend_spx_globals, spx_globals
    )
    STD_PHP_INI_ENTRY(
        "spx.http_ui_uri_prefix", "/_spx", PHP_INI_SYSTEM,
        OnUpdateString, http_ui_uri_prefix, zend_spx_globals, spx_globals
    )
PHP_INI_END()

static PHP_MINIT_FUNCTION(spx);
static PHP_MSHUTDOWN_FUNCTION(spx);
static PHP_RINIT_FUNCTION(spx);
static PHP_RSHUTDOWN_FUNCTION(spx);
static PHP_MINFO_FUNCTION(spx);

static int check_access(void);

static void profiling_handler_init(void);
static void profiling_handler_shutdown(void);
static void profiling_handler_ex_set_context(void);
static void profiling_handler_ex_unset_context(void);
static void profiling_handler_ex_hook_before(void);
static void profiling_handler_ex_hook_after(void);
#ifdef USE_SIGNAL
static void profiling_handler_sig_terminate(void);
static void profiling_handler_sig_handler(int signo);
static void profiling_handler_sig_set_handler(void);
static void profiling_handler_sig_unset_handler(void);
#endif

static void http_ui_handler_init(void);
static void http_ui_handler_shutdown(void);
static int  http_ui_handler_data(const char * data_dir, const char *relative_path);
static void http_ui_handler_list_metadata_files_callback(const char * file_name, size_t count);
static int  http_ui_handler_output_file(const char * file_name);

static void read_stream_content(FILE * stream, size_t (*callback) (const void * ptr, size_t len));

static execution_handler_t profiling_handler = {
    profiling_handler_init,
    profiling_handler_shutdown
};

static execution_handler_t http_ui_handler = {
    http_ui_handler_init,
    http_ui_handler_shutdown
};

static zend_function_entry spx_functions[] = {
    /* empty */
    {NULL, NULL, NULL, 0, 0}
};

zend_module_entry spx_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_SPX_EXTNAME,
    spx_functions,
    PHP_MINIT(spx),
    PHP_MSHUTDOWN(spx),
    PHP_RINIT(spx),
    PHP_RSHUTDOWN(spx),
    PHP_MINFO(spx),
    PHP_SPX_VERSION,
    PHP_MODULE_GLOBALS(spx),
    NULL,
    NULL,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_SPX
ZEND_GET_MODULE(spx)
#endif

static PHP_MINIT_FUNCTION(spx)
{
#ifdef ZTS
    spx_php_hooks_init();
#endif

    REGISTER_INI_ENTRIES();

    return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(spx)
{
#ifdef ZTS
    spx_php_hooks_shutdown();
#endif

    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

static PHP_RINIT_FUNCTION(spx)
{
    context.execution_handler = NULL;
    context.cli_sapi = spx_php_is_cli_sapi();

    if (context.cli_sapi) {
        spx_config_get(&context.config, context.cli_sapi, SPX_CONFIG_SOURCE_ENV, -1);
    } else {
        spx_config_get(
            &context.config,
            context.cli_sapi,
            SPX_CONFIG_SOURCE_HTTP_COOKIE,
            SPX_CONFIG_SOURCE_HTTP_HEADER,
            SPX_CONFIG_SOURCE_HTTP_QUERY_STRING,
            -1
        );
    }

    int web_ui_url = 0;
    if (!context.cli_sapi) {
        const char * request_uri = spx_php_global_array_get("_SERVER", "REQUEST_URI");
        if (request_uri) {
            web_ui_url = spx_utils_str_starts_with(request_uri, SPX_G(http_ui_uri_prefix));
        }
    }

    if (!web_ui_url && !context.config.enabled) {
        return SUCCESS;
    }

    if (!check_access()) {
        return SUCCESS;
    }

    if (web_ui_url) {
        context.execution_handler = &http_ui_handler;
    } else if (context.config.enabled) {
        context.execution_handler = &profiling_handler;
    }

    if (context.execution_handler) {
        context.execution_handler->init();
    }

    return SUCCESS;
}

static PHP_RSHUTDOWN_FUNCTION(spx)
{
    if (context.execution_handler) {
        context.execution_handler->shutdown();
    }

    return SUCCESS;
}

static PHP_MINFO_FUNCTION(spx)
{
    php_info_print_table_start();

    php_info_print_table_row(2, PHP_SPX_EXTNAME " Support", "enabled");
    php_info_print_table_row(2, PHP_SPX_EXTNAME " Version", PHP_SPX_VERSION);

    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}

static int check_access(void)
{
    TSRMLS_FETCH();

    if (context.cli_sapi) {
        /* CLI SAPI -> granted */
        return 1;
    }

    if (!SPX_G(http_enabled)) {
        /* HTTP profiling explicitly turned off -> not granted */

        return 0;
    }

    if (!SPX_G(http_key) || SPX_G(http_key)[0] == 0) {
        /* empty spx.http_key (server config) -> not granted */
        spx_php_log_notice("access not granted: http_key is empty");

        return 0;
    }

    if (!context.config.key || context.config.key[0] == 0) {
        /* empty SPX_KEY (client config) -> not granted */
        spx_php_log_notice("access not granted: client key is empty");

        return 0;
    }

    if (0 != strcmp(SPX_G(http_key), context.config.key)) {
        /* server / client key mismatch -> not granted */
        spx_php_log_notice(
            "access not granted: server (\"%s\") & client (\"%s\") key mismatch",
            SPX_G(http_key),
            context.config.key
        );

        return 0;
    }

    if (!SPX_G(http_ip_var) || SPX_G(http_ip_var)[0] == 0) {
        /* empty client ip server var name -> not granted */
        spx_php_log_notice("access not granted: http_ip_var is empty");

        return 0;
    }

    const char * ip_str = spx_php_global_array_get("_SERVER", SPX_G(http_ip_var));
    if (!ip_str || ip_str[0] == 0) {
        /* empty client ip -> not granted */
        spx_php_log_notice(
            "access not granted: $_SERVER[\"%s\"] is empty",
            SPX_G(http_ip_var)
        );

        return 0;
    }

    const char * authorized_ips_str = SPX_G(http_ip_whitelist);
    if (!authorized_ips_str || authorized_ips_str[0] == 0) {
        /* empty ip white list -> not granted */
        spx_php_log_notice("access not granted: IP white list is empty");

        return 0;
    }

    SPX_UTILS_TOKENIZE_STRING(authorized_ips_str, ',', authorized_ip_str, 32, {
        if (0 == strcmp(ip_str, authorized_ip_str)) {
            /* ip authorized (OK, as well as all previous checks) -> granted */
            spx_php_log_notice(
                "access granted: \"%s\" IP with \"%s\" key",
                ip_str,
                context.config.key
            );

            return 1;
        }
    });

    spx_php_log_notice(
        "access not granted: \"%s\" IP is not in white list (\"%s\")",
        ip_str,
        authorized_ips_str
    );

    /* no matching ip in white list -> not granted */
    return 0;
}

static void profiling_handler_init(void)
{
    TSRMLS_FETCH();

#ifdef USE_SIGNAL
    context.profiling_handler.sig_handling.handler_set = 0;
    context.profiling_handler.sig_handling.probing = 0;
    context.profiling_handler.sig_handling.stop = 0;
    context.profiling_handler.sig_handling.handler_called = 0;
    context.profiling_handler.sig_handling.signo = -1;
#endif

    profiling_handler_ex_set_context();

    context.profiling_handler.reporter = NULL;
    context.profiling_handler.profiler = NULL;

    switch (context.config.report) {
        default:
        case SPX_CONFIG_REPORT_FULL:
            context.profiling_handler.reporter = spx_reporter_full_create(SPX_G(data_dir));

            break;

        case SPX_CONFIG_REPORT_FLAT_PROFILE:
            context.profiling_handler.reporter = spx_reporter_fp_create(
                context.config.fp_focus,
                context.config.fp_inc,
                context.config.fp_rel,
                context.config.fp_limit,
                context.config.fp_live
            );

            break;

        case SPX_CONFIG_REPORT_TRACE:
            context.profiling_handler.reporter = spx_reporter_trace_create(
                context.config.trace_file,
                context.config.trace_safe
            );

            break;
    }

    if (!context.profiling_handler.reporter) {
        goto error;
    }

    context.profiling_handler.profiler = spx_profiler_tracer_create(
        context.config.max_depth,
        context.config.enabled_metrics,
        context.profiling_handler.reporter
    );

    if (!context.profiling_handler.profiler) {
        goto error;
    }

    if (context.config.sampling_period > 0) {
        spx_profiler_t * sampling_profiler = spx_profiler_sampler_create(
            context.profiling_handler.profiler,
            context.config.sampling_period
        );

        if (!sampling_profiler) {
            goto error;
        }

        context.profiling_handler.profiler = sampling_profiler;
    }

    return;

error:
    profiling_handler_shutdown();
}

static void profiling_handler_shutdown(void)
{
    spx_php_hooks_finalize();

    if (context.profiling_handler.profiler) {
        context.profiling_handler.profiler->finalize(context.profiling_handler.profiler);
        context.profiling_handler.profiler->destroy(context.profiling_handler.profiler);
        context.profiling_handler.profiler = NULL;
    }

    if (context.profiling_handler.reporter) {
        spx_profiler_reporter_destroy(context.profiling_handler.reporter);
        context.profiling_handler.reporter = NULL;
    }

    profiling_handler_ex_unset_context();
}

static void profiling_handler_ex_set_context(void)
{
#ifndef ZTS
    spx_php_hooks_init();
#endif

    spx_php_execution_init();
    spx_resource_stats_init();

    spx_php_execution_hook(profiling_handler_ex_hook_before, profiling_handler_ex_hook_after, 0);
    if (context.config.builtins) {
        spx_php_execution_hook(profiling_handler_ex_hook_before, profiling_handler_ex_hook_after, 1);
    }

#ifdef USE_SIGNAL
    if (context.cli_sapi) {
        profiling_handler_sig_set_handler();
    }
#endif
}

static void profiling_handler_ex_unset_context(void)
{
#ifdef USE_SIGNAL
    if (context.cli_sapi) {
        profiling_handler_sig_unset_handler();
    }
#endif

    spx_resource_stats_shutdown();
    spx_php_execution_shutdown();

#ifndef ZTS
    spx_php_hooks_shutdown();
#endif
}

static void profiling_handler_ex_hook_before(void)
{
#ifdef USE_SIGNAL
    context.profiling_handler.sig_handling.probing = 1;
#endif

    spx_php_function_t function;
    spx_php_current_function(&function);

    context.profiling_handler.profiler->call_start(context.profiling_handler.profiler, &function);

#ifdef USE_SIGNAL
    context.profiling_handler.sig_handling.probing = 0;
    if (context.profiling_handler.sig_handling.stop) {
        profiling_handler_sig_terminate();
    }
#endif
}

static void profiling_handler_ex_hook_after(void)
{
#ifdef USE_SIGNAL
    context.profiling_handler.sig_handling.probing = 1;
#endif

    context.profiling_handler.profiler->call_end(context.profiling_handler.profiler);

#ifdef USE_SIGNAL
    context.profiling_handler.sig_handling.probing = 0;
    if (context.profiling_handler.sig_handling.stop) {
        profiling_handler_sig_terminate();
    }
#endif
}

#ifdef USE_SIGNAL
static void profiling_handler_sig_terminate(void)
{
    profiling_handler_shutdown();

    _exit(
        context.profiling_handler.sig_handling.signo < 0 ?
            EXIT_SUCCESS : 128 + context.profiling_handler.sig_handling.signo
    );
}

static void profiling_handler_sig_handler(int signo)
{
    context.profiling_handler.sig_handling.handler_called++;
    if (context.profiling_handler.sig_handling.handler_called > 1) {
        return;
    }

    context.profiling_handler.sig_handling.signo = signo;

    if (context.profiling_handler.sig_handling.probing) {
        context.profiling_handler.sig_handling.stop = 1;

        return;
    }

    profiling_handler_sig_terminate();
}

static void profiling_handler_sig_set_handler(void)
{
    struct sigaction act;

    act.sa_handler = profiling_handler_sig_handler;
    act.sa_flags = 0;

    sigaction(SIGINT, &act, &context.profiling_handler.sig_handling.prev_handler.sigint);
    sigaction(SIGTERM, &act, &context.profiling_handler.sig_handling.prev_handler.sigterm);

    context.profiling_handler.sig_handling.handler_set = 1;
}

static void profiling_handler_sig_unset_handler(void)
{
    if (!context.profiling_handler.sig_handling.handler_set) {
        return;
    }

    sigaction(SIGINT, &context.profiling_handler.sig_handling.prev_handler.sigint, NULL);
    sigaction(SIGTERM, &context.profiling_handler.sig_handling.prev_handler.sigterm, NULL);

    context.profiling_handler.sig_handling.handler_set = 0;
}
#endif /* defined(USE_SIGNAL) */

static void http_ui_handler_init(void)
{
#ifndef ZTS
    spx_php_hooks_init();
#endif

    spx_php_execution_init();
    spx_php_execution_disable();
}

static void http_ui_handler_shutdown(void)
{
    TSRMLS_FETCH();
    spx_php_execution_shutdown();

    const char * request_uri = spx_php_global_array_get("_SERVER", "REQUEST_URI");
    if (!request_uri) {
        goto error_404;
    }

    const char * prefix_pos = strstr(request_uri, SPX_G(http_ui_uri_prefix));
    if (prefix_pos != request_uri) {
        goto error_404;
    }

    char relative_path[512];
    strncpy(relative_path, request_uri + strlen(SPX_G(http_ui_uri_prefix)), sizeof(relative_path));

    char * query_string = strchr(relative_path, '?');
    if (relative_path[0] != '/') {
        spx_php_output_add_header_line("HTTP/1.1 301 Moved Permanently");
        spx_php_output_add_header_linef(
            "Location: %s/index.html%s",
            SPX_G(http_ui_uri_prefix),
            query_string ? query_string : ""
        );

        spx_php_output_send_headers();

        goto finish;
    }

    if (query_string) {
        *query_string = 0;
    }

    if (0 == strcmp(relative_path, "/")) {
        strncpy(relative_path, "/index.html", sizeof(relative_path));
    }

    if (0 == http_ui_handler_data(SPX_G(data_dir), relative_path)) {
        goto finish;
    }

    char local_file_name[512];
    snprintf(
        local_file_name,
        sizeof(local_file_name),
        "%s%s",
        SPX_G(http_ui_assets_dir),
        relative_path
    );

    if (0 == http_ui_handler_output_file(local_file_name)) {
        goto finish;
    }

error_404:
    spx_php_output_add_header_line("HTTP/1.1 404 Not Found");
    spx_php_output_add_header_line("Content-Type: text/plain");
    spx_php_output_send_headers();

    spx_php_output_direct_print("File not found.\n");

finish:
#ifndef ZTS
    spx_php_hooks_shutdown();
#endif
    ;
}

static int http_ui_handler_data(const char * data_dir, const char *relative_path)
{
    if (0 == strcmp(relative_path, "/data/metrics")) {
        spx_php_output_add_header_line("HTTP/1.1 200 OK");
        spx_php_output_add_header_line("Content-Type: application/json");
        spx_php_output_send_headers();

        spx_php_output_direct_print("{\"results\": [\n");

        SPX_METRIC_FOREACH(i, {
            if (i > 0) {
                spx_php_output_direct_print(",");
            }

            spx_php_output_direct_print("{");

            spx_php_output_direct_printf("\"key\": \"%s\",", spx_metrics_info[i].key);
            spx_php_output_direct_printf("\"short_name\": \"%s\",", spx_metrics_info[i].short_name);
            spx_php_output_direct_printf("\"name\": \"%s\",", spx_metrics_info[i].name);

            spx_php_output_direct_print("\"type\": \"");
            switch (spx_metrics_info[i].type) {
                case SPX_FMT_TIME:
                    spx_php_output_direct_print("time");
                    break;

                case SPX_FMT_MEMORY:
                    spx_php_output_direct_print("memory");
                    break;

                case SPX_FMT_QUANTITY:
                    spx_php_output_direct_print("quantity");
                    break;

                default:
                    ;
            }

            spx_php_output_direct_print("\",");

            spx_php_output_direct_printf("\"releasable\": %d", spx_metrics_info[i].releasable);

            spx_php_output_direct_print("}\n");
        });

        spx_php_output_direct_print("]}\n");

        return 0;
    }

    if (0 == strcmp(relative_path, "/data/reports/metadata")) {
        spx_php_output_add_header_line("HTTP/1.1 200 OK");
        spx_php_output_add_header_line("Content-Type: application/json");
        spx_php_output_send_headers();

        spx_php_output_direct_print("{\"results\": [\n");

        spx_reporter_full_metadata_list_files(
            data_dir,
            http_ui_handler_list_metadata_files_callback
        );

        spx_php_output_direct_print("]}\n");

        return 0;
    }

    const char * get_report_metadata_uri = "/data/reports/metadata/";
    if (spx_utils_str_starts_with(relative_path, get_report_metadata_uri)) {
        char file_name[512];
        spx_reporter_full_metadata_get_file_name(
            data_dir,
            relative_path + strlen(get_report_metadata_uri),
            file_name,
            sizeof(file_name)
        );

        return http_ui_handler_output_file(file_name);
    }

    const char * get_report_uri = "/data/reports/get/";
    if (spx_utils_str_starts_with(relative_path, get_report_uri)) {
        char file_name[512];
        spx_reporter_full_get_file_name(
            data_dir,
            relative_path + strlen(get_report_uri),
            file_name,
            sizeof(file_name)
        );

        return http_ui_handler_output_file(file_name);
    }

    return -1;
}

static void http_ui_handler_list_metadata_files_callback(const char * file_name, size_t count)
{
    if (count > 0) {
        spx_php_output_direct_print(",");
    }

    FILE * fp = fopen(file_name, "r");
    if (!fp) {
        return;
    }

    read_stream_content(fp, spx_php_output_direct_write);
    fclose(fp);
}

static int http_ui_handler_output_file(const char * file_name)
{
    FILE * fp = fopen(file_name, "rb");
    if (!fp) {
        return -1;
    }

    char suffix[32];
    int suffix_offset = strlen(file_name) - (sizeof(suffix) - 1);
    strncpy(
        suffix,
        file_name + (suffix_offset < 0 ? 0 : suffix_offset),
        sizeof(suffix)
    );

    suffix[sizeof(suffix) - 1] = 0;

    const int compressed = spx_utils_str_ends_with(suffix, ".gz");
    if (compressed) {
        *strrchr(suffix, '.') = 0;
    }

    const char * content_type = "application/octet-stream";
    if (spx_utils_str_ends_with(suffix, ".html")) {
        content_type = "text/html; charset=utf-8";
    } else if (spx_utils_str_ends_with(suffix, ".css")) {
        content_type = "text/css";
    } else if (spx_utils_str_ends_with(suffix, ".js")) {
        content_type = "application/javascript";
    } else if (spx_utils_str_ends_with(suffix, ".json")) {
        content_type = "application/json";
    }

    spx_php_output_add_header_line("HTTP/1.1 200 OK");
    spx_php_output_add_header_linef("Content-Type: %s", content_type);
    if (compressed) {
        spx_php_output_add_header_line("Content-Encoding: gzip");
    }

    fseek(fp, 0L, SEEK_END);
    spx_php_output_add_header_linef("Content-Length: %ld", ftell(fp));
    rewind(fp);

    spx_php_output_send_headers();

    read_stream_content(fp, spx_php_output_direct_write);
    fclose(fp);

    return 0;
}

static void read_stream_content(FILE * stream, size_t (*callback) (const void * ptr, size_t len))
{
    char buf[8 * 1024];
    while (1) {
        size_t read = fread(buf, 1, sizeof(buf), stream);
        callback(buf, read);

        if (read < sizeof(buf)) {
            break;
        }
    }
}
