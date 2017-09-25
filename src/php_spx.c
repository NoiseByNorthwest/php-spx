#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#ifndef ZTS
#   include <signal.h>
#endif

#include "main/SAPI.h"
#include "ext/standard/info.h"

#include "php_spx.h"
#include "spx_thread.h"
#include "spx_config.h"
#include "spx_stdio.h"
#include "spx_php.h"
#include "spx_utils.h"
#include "spx_resource_stats.h"
#include "spx_profiler.h"
#include "spx_reporter_fp.h"
#include "spx_reporter_cg.h"
#include "spx_reporter_gte.h"
#include "spx_reporter_trace.h"

static SPX_THREAD_TLS struct {
    struct {
        volatile sig_atomic_t probing;
        volatile sig_atomic_t stop;
        volatile sig_atomic_t finish_called;
        int signo;
    } sig_handling;

    int cli_sapi;

    struct {
        int stdout;
        int stderr;
    } fd_backup;

    spx_config_t config;
    spx_profiler_t * profiler;

    char output_file[512];
} context;

/*
 *  PHP way of managing global state: currently only used for INI entries.
 */
ZEND_BEGIN_MODULE_GLOBALS(spx)
    zend_bool http_enabled;
    const char * http_key;
    const char * http_ip_var;
    const char * http_ip_whitelist;
ZEND_END_MODULE_GLOBALS(spx)

ZEND_DECLARE_MODULE_GLOBALS(spx)

#ifdef ZTS
#   define SPX_G(v) TSRMG(spx_globals_id, zend_spx_globals *, v)
#else
#   define SPX_G(v) (spx_globals.v)
#endif

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("spx.http_enabled", "0", PHP_INI_SYSTEM, OnUpdateBool, http_enabled, zend_spx_globals, spx_globals)
    STD_PHP_INI_ENTRY("spx.http_key", "", PHP_INI_SYSTEM, OnUpdateString, http_key, zend_spx_globals, spx_globals)
    STD_PHP_INI_ENTRY("spx.http_ip_var", "REMOTE_ADDR", PHP_INI_SYSTEM, OnUpdateString, http_ip_var, zend_spx_globals, spx_globals)
    STD_PHP_INI_ENTRY("spx.http_ip_whitelist", "", PHP_INI_SYSTEM, OnUpdateString, http_ip_whitelist, zend_spx_globals, spx_globals)
PHP_INI_END()

PHP_MINIT_FUNCTION(spx);
PHP_MSHUTDOWN_FUNCTION(spx);
PHP_RINIT_FUNCTION(spx);
PHP_RSHUTDOWN_FUNCTION(spx);
PHP_MINFO_FUNCTION(spx);

static void ex_hook_before(void);
static void ex_hook_after(void);
static void terminate(void);

#ifndef ZTS
static void terminate_handler(int signo);
static void setup_terminate_handler(void);
#endif

static int check_access(void);
static void init(void);
static void finish(void);
static void read_file_content(const char * file, size_t (*reader) (const void * ptr, size_t len));
static void generate_output_file_name(char * str, size_t max, spx_config_output_t output_type);

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

PHP_MINIT_FUNCTION(spx)
{
#ifdef ZTS
    spx_php_hooks_init();
#endif

    REGISTER_INI_ENTRIES();

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(spx)
{
#ifdef ZTS
    spx_php_hooks_shutdown();
#endif

    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

PHP_RINIT_FUNCTION(spx)
{
    init();

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(spx)
{
    finish();

    return SUCCESS;
}

PHP_MINFO_FUNCTION(spx)
{
    php_info_print_table_start();

    php_info_print_table_row(2, "SPX Support", "enabled");
    php_info_print_table_row(2, "SPX Version", PHP_SPX_VERSION);

    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}

static void ex_hook_before(void)
{
    context.sig_handling.probing = 1;
    spx_php_function_t function;
    spx_php_current_function(&function);

    spx_profiler_call_start(context.profiler, &function);
    context.sig_handling.probing = 0;
    if (context.sig_handling.stop) {
        terminate();
    }
}

static void ex_hook_after(void)
{
    context.sig_handling.probing = 1;
    spx_profiler_call_end(context.profiler);
    context.sig_handling.probing = 0;
    if (context.sig_handling.stop) {
        terminate();
    }
}

static void terminate(void)
{
    finish();

    exit(context.sig_handling.signo < 0 ? EXIT_SUCCESS : 128 + context.sig_handling.signo);
}

#ifndef ZTS
static void terminate_handler(int signo)
{
    if (context.sig_handling.finish_called > 0) {
        return;
    }

    context.sig_handling.signo = signo;

    if (context.sig_handling.probing) {
        context.sig_handling.stop = 1;

        return;
    }

    terminate();
}

static void setup_terminate_handler(void)
{
    struct sigaction act;

    act.sa_handler = terminate_handler;
    act.sa_flags = 0;

    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}
#endif

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

    /* empty spx.http_key (server config) -> not granted */
    if (!SPX_G(http_key) || SPX_G(http_key)[0] == 0) {
        return 0;
    }

    /* empty SPX_KEY (client config) -> not granted */
    if (!context.config.key || context.config.key[0] == 0) {
        return 0;
    }

    /* server / client key mismatch -> not granted */
    if (0 != strcmp(SPX_G(http_key), context.config.key)) {
        return 0;
    }

    /* empty client ip server var name -> not granted */
    if (!SPX_G(http_ip_var) || SPX_G(http_ip_var)[0] == 0) {
        return 0;
    }

    /* empty client ip -> not granted */
    const char * ip_str = spx_php_global_array_get("_SERVER", SPX_G(http_ip_var));
    if (!ip_str || ip_str[0] == 0) {
        return 0;
    }

    /* empty ip white list -> not granted */
    const char * authorized_ips_str = SPX_G(http_ip_whitelist);
    if (!authorized_ips_str || authorized_ips_str[0] == 0) {
        return 0;
    }

    SPX_UTILS_TOKENIZE_STRING(authorized_ips_str, ',', authorized_ip_str, 32, {
        if (0 == strcmp(ip_str, authorized_ip_str)) {
            /* ip authorized (OK, as well as all previous checks) -> granted */
            return 1;
        }
    });

    /* no matching ip in white list -> not granted */
    return 0;
}

static void init(void)
{
    context.sig_handling.probing = 0;
    context.sig_handling.stop = 0;
    context.sig_handling.finish_called = 0;
    context.sig_handling.signo = -1;

    context.cli_sapi = 0 == strcmp(sapi_module.name, "cli");

    context.fd_backup.stdout = -1;
    context.fd_backup.stderr = -1;
    
    context.profiler = NULL;
    context.output_file[0] = 0;

    spx_config_init(&context.config);
    if (context.cli_sapi) {
        spx_config_read(&context.config, SPX_CONFIG_SOURCE_ENV);
    } else {
        spx_config_read(&context.config, SPX_CONFIG_SOURCE_HTTP_HEADER);
        spx_config_read(&context.config, SPX_CONFIG_SOURCE_HTTP_QUERY_STRING);
    }

    if (!context.config.enabled) {
        return;
    }

    if (!check_access()) {
        return;
    }

    if (context.config.output_file) {
        strcpy(context.output_file, context.config.output_file);
    }

    if (!context.output_file[0]) {
        generate_output_file_name(
            context.output_file,
            sizeof(context.output_file),
            context.config.output
        );
    }

    spx_output_stream_t * output = NULL;
    int fp_live = 0;
    if (
        context.cli_sapi &&
        context.config.output == SPX_CONFIG_OUTPUT_FLAT_PROFILE &&
        !context.config.output_file
    ) {
        fp_live = context.config.fp_live && isatty(STDOUT_FILENO);
        if (fp_live) {
            context.fd_backup.stdout = spx_stdio_disable(STDOUT_FILENO);
            context.fd_backup.stderr = spx_stdio_disable(STDERR_FILENO);
            output = spx_output_stream_dopen(context.fd_backup.stdout, 0);
        } else {
            output = spx_output_stream_dopen(STDOUT_FILENO, 0);
        }
    } else {
        int compressed = 0;
        size_t name_len = strlen(context.output_file);
        if (name_len > 3 && strcmp(context.output_file + name_len - 3, ".gz") == 0) {
            compressed = 1;
        }

        output = spx_output_stream_open(context.output_file, compressed);
    }

    if (!output) {
        return;
    }

    spx_profiler_reporter_t * reporter = NULL;
    switch (context.config.output) {
        default:
        case SPX_CONFIG_OUTPUT_FLAT_PROFILE:
            reporter = spx_reporter_fp_create(
                output,
                context.config.fp_focus,
                context.config.fp_inc,
                context.config.fp_rel,
                context.config.fp_limit,
                fp_live
            );

            break;

        case SPX_CONFIG_OUTPUT_CALLGRIND:
            reporter = spx_reporter_cg_create(output);

            break;

        case SPX_CONFIG_OUTPUT_GOOGLE_TRACE_EVENT:
            reporter = spx_reporter_gte_create(output);

            break;

        case SPX_CONFIG_OUTPUT_TRACE:
            reporter = spx_reporter_trace_create(output, context.config.trace_safe);

            break;
    }

    if (!reporter) {
        return;
    }

    spx_resource_stats_init();

    context.profiler = spx_profiler_create(
        context.config.max_depth,
        context.config.enabled_metrics,
        reporter
    );

    if (!context.profiler) {
        spx_resource_stats_shutdown();

        return;
    }

#ifndef ZTS
    spx_php_hooks_init();
#endif

    spx_php_context_init();

    spx_php_execution_hook(ex_hook_before, ex_hook_after, 0);
    if (context.config.builtins) {
        spx_php_execution_hook(ex_hook_before, ex_hook_after, 1);
    }

    if (context.cli_sapi) {
#ifndef ZTS
        setup_terminate_handler();
#endif
    } else {
        spx_php_output_disable();
    }
}

static void finish(void)
{
    context.sig_handling.finish_called++;

    if (context.sig_handling.finish_called != 1) {
        return;
    }

    if (!context.profiler) {
        return;
    }

    spx_profiler_finalize(context.profiler);
    spx_profiler_destroy(context.profiler);
    context.profiler = NULL;

    if (context.fd_backup.stdout != -1) {
        spx_stdio_restore(STDOUT_FILENO, context.fd_backup.stdout);
    }

    if (context.fd_backup.stderr != -1) {
        spx_stdio_restore(STDERR_FILENO, context.fd_backup.stderr);
    }

    spx_resource_stats_shutdown();

    spx_php_output_restore();

    spx_php_execution_hook(NULL, NULL, 0);
    spx_php_execution_hook(NULL, NULL, 1);

#ifndef ZTS
    spx_php_hooks_shutdown();
#endif

    if (
        context.cli_sapi &&
        !context.config.output_file &&
        context.config.output != SPX_CONFIG_OUTPUT_FLAT_PROFILE
    ) {
        fprintf(stderr, "\nSPX output file: %s\n", context.output_file);
    }

    if (context.cli_sapi) {
        return;
    }

    spx_php_output_direct_print("HTTP/1.1 200 OK\r\n");

    switch (context.config.output) {
        default:
        case SPX_CONFIG_OUTPUT_FLAT_PROFILE:
        case SPX_CONFIG_OUTPUT_TRACE:
            spx_php_output_direct_print("Content-Type: text/plain\r\n");

            break;

        case SPX_CONFIG_OUTPUT_CALLGRIND:
            spx_php_output_direct_print("Content-Type: application/octet-stream\r\n");

            break;

        case SPX_CONFIG_OUTPUT_GOOGLE_TRACE_EVENT:
            spx_php_output_direct_print("Content-Type: application/json\r\n");

            break;
    }


    if (context.config.output != SPX_CONFIG_OUTPUT_FLAT_PROFILE) {
        spx_php_output_direct_print("Content-Encoding: gzip\r\n");
        
        spx_php_output_direct_print("Content-Disposition: attachment; filename=\"");

        const char * basename = strrchr(context.output_file, '/') + 1;
        spx_php_output_direct_write(basename, strrchr(basename, '.') - basename);

        spx_php_output_direct_print("\"\r\n");
    }

    spx_php_output_direct_print("\r\n");

    read_file_content(context.output_file, spx_php_output_direct_write);

    remove(context.output_file);

    spx_php_ouput_finalize();
}

static void read_file_content(const char * file, size_t (*reader) (const void * ptr, size_t len))
{
    FILE * fp = fopen(file, "rb");
    if (!fp) {
        return;
    }

    char buf[512];
    while (1) {
        size_t read = fread(buf, 1, sizeof(buf), fp);
        reader(buf, read);

        if (read < sizeof(buf)) {
            break;
        }
    }

    fclose(fp);
}

static void generate_output_file_name(char * str, size_t max, spx_config_output_t output_type)
{
    const char * prefix;
    const char * extension;
    int compressed;

    switch (output_type) {
        default:
        case SPX_CONFIG_OUTPUT_FLAT_PROFILE:
            prefix = "spx.flat_profile";
            extension = "txt";
            compressed = 0;

            break;

        case SPX_CONFIG_OUTPUT_CALLGRIND:
            prefix = "callgrind.out";
            extension = "dat";
            compressed = 1;

            break;

        case SPX_CONFIG_OUTPUT_GOOGLE_TRACE_EVENT:
            prefix = "spx.google_trace_event";
            extension = "json";
            compressed = 1;

            break;

        case SPX_CONFIG_OUTPUT_TRACE:
            prefix = "spx.trace";
            extension = "txt";
            compressed = 1;
            
            break;
    }

    time_t timer;
    time(&timer);

    char date[64];
    strftime(
        date,
        sizeof(date),
        "%Y-%m-%d_%H:%M:%S",
        localtime(&timer)
    );

    snprintf(
        str,
        max,
        "/tmp/%s.%s.%d.%s%s",
        prefix,
        date,
        rand(),
        extension,
        compressed ? ".gz" : ""
    );
}
