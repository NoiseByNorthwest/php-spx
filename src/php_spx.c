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

#define STACK_CAPACITY 2048

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

        char full_report_key[512];
        spx_profiler_reporter_t * reporter;
        spx_profiler_t * profiler;
        spx_php_function_t stack[STACK_CAPACITY];
        size_t depth;
        size_t span_depth;
    } profiling_handler;
} context;

ZEND_BEGIN_MODULE_GLOBALS(spx)
    zend_bool debug;
    const char * data_dir;
    zend_bool http_enabled;
    const char * http_key;
    const char * http_ip_var;
    const char * http_trusted_proxies;
    const char * http_ip_whitelist;
    const char * http_ui_assets_dir;
    const char * http_profiling_enabled;
    const char * http_profiling_auto_start;
    const char * http_profiling_builtins;
    const char * http_profiling_sampling_period;
    const char * http_profiling_depth;
    const char * http_profiling_metrics;
ZEND_END_MODULE_GLOBALS(spx)

ZEND_DECLARE_MODULE_GLOBALS(spx)

#ifdef ZTS
#   define SPX_G(v) TSRMG(spx_globals_id, zend_spx_globals *, v)
#else
#   define SPX_G(v) (spx_globals.v)
#endif

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY(
        "spx.debug", "0", PHP_INI_SYSTEM,
        OnUpdateBool, debug, zend_spx_globals, spx_globals
    )
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
        "spx.http_trusted_proxies", "127.0.0.1", PHP_INI_SYSTEM,
        OnUpdateString, http_trusted_proxies, zend_spx_globals, spx_globals
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
        "spx.http_profiling_enabled", NULL, PHP_INI_SYSTEM,
        OnUpdateString, http_profiling_enabled, zend_spx_globals, spx_globals
    )
    STD_PHP_INI_ENTRY(
        "spx.http_profiling_auto_start", NULL, PHP_INI_SYSTEM,
        OnUpdateString, http_profiling_auto_start, zend_spx_globals, spx_globals
    )
    STD_PHP_INI_ENTRY(
        "spx.http_profiling_builtins", NULL, PHP_INI_SYSTEM,
        OnUpdateString, http_profiling_builtins, zend_spx_globals, spx_globals
    )
    STD_PHP_INI_ENTRY(
        "spx.http_profiling_sampling_period", NULL, PHP_INI_SYSTEM,
        OnUpdateString, http_profiling_sampling_period, zend_spx_globals, spx_globals
    )
    STD_PHP_INI_ENTRY(
        "spx.http_profiling_depth", NULL, PHP_INI_SYSTEM,
        OnUpdateString, http_profiling_depth, zend_spx_globals, spx_globals
    )
    STD_PHP_INI_ENTRY(
        "spx.http_profiling_metrics", NULL, PHP_INI_SYSTEM,
        OnUpdateString, http_profiling_metrics, zend_spx_globals, spx_globals
    )
PHP_INI_END()

static PHP_MINIT_FUNCTION(spx);
static PHP_MSHUTDOWN_FUNCTION(spx);
static PHP_RINIT_FUNCTION(spx);
static PHP_RSHUTDOWN_FUNCTION(spx);
static PHP_MINFO_FUNCTION(spx);
static PHP_FUNCTION(spx_profiler_start);
static PHP_FUNCTION(spx_profiler_stop);
static PHP_FUNCTION(spx_profiler_full_report_set_custom_metadata_str);

static int check_access(void);

static void profiling_handler_init(void);
static void profiling_handler_shutdown(void);
static void profiling_handler_start(void);
static void profiling_handler_stop(void);
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

ZEND_BEGIN_ARG_INFO_EX(arginfo_spx_profiler_start, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_spx_profiler_stop, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_spx_profiler_full_report_set_custom_metadata_str, 0, 0, 1)
#if PHP_API_VERSION >= 20151012
    ZEND_ARG_TYPE_INFO(0, customMetadataStr, IS_STRING, 0)
#else
    ZEND_ARG_INFO(0, customMetadataStr)
#endif
ZEND_END_ARG_INFO()

static zend_function_entry spx_functions[] = {
    PHP_FE(spx_profiler_start, arginfo_spx_profiler_start)
    PHP_FE(spx_profiler_stop, arginfo_spx_profiler_stop)
    PHP_FE(spx_profiler_full_report_set_custom_metadata_str, arginfo_spx_profiler_full_report_set_custom_metadata_str)
    PHP_FE_END
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
    spx_php_global_hooks_set();
#endif

    REGISTER_INI_ENTRIES();

    return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(spx)
{
#ifdef ZTS
    spx_php_global_hooks_unset();
#endif

    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

static PHP_RINIT_FUNCTION(spx)
{
#ifdef ZTS
    spx_php_global_hooks_disable();
#endif

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

        /*
            The access (multi-factor authentication check) is required as long as the client request the
            access to the web UI or to profile the current script.
        */
        const int access_required = context.config.ui_uri || context.config.enabled;

        if (!access_required || !check_access()) {
            /*
                If the access is not required or not granted, we have to read the config again, from the INI source only.
            */
            spx_config_get(
                &context.config,
                context.cli_sapi,
                SPX_CONFIG_SOURCE_INI,
                -1
            );
        }
    }

    if (context.config.ui_uri) {
        context.execution_handler = &http_ui_handler;
    } else {
        if (context.config.enabled) {
            context.execution_handler = &profiling_handler;
        }

        if (!context.cli_sapi && SPX_G(debug)) {
            spx_php_output_add_header_linef(
                "SPX-Debug-Profiling-Triggered: %d",
                context.config.enabled
            );
        }
    }

    if (!context.execution_handler) {
        return SUCCESS;
    }

    context.execution_handler->init();

    return SUCCESS;
}

static PHP_RSHUTDOWN_FUNCTION(spx)
{
    if (context.execution_handler) {
        context.execution_handler->shutdown();
    }

#ifdef ZTS
    spx_php_global_hooks_disable();
#endif

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

static PHP_FUNCTION(spx_profiler_start)
{
    if (context.execution_handler != &profiling_handler) {
        spx_php_log_notice("spx_profiler_start(): profiling is not enabled");

        return;
    }

    if (context.config.auto_start) {
        spx_php_log_notice("spx_profiler_start(): automatic start is not disabled");

        return;
    }

    context.profiling_handler.span_depth++;
    if (context.profiling_handler.span_depth > 1) {
        /*
            Starting a nested span, nothing to do.
        */
        return;
    }

    if (context.profiling_handler.profiler) {
        return;
    }

    profiling_handler_start();

    if (!context.profiling_handler.profiler) {
        spx_php_log_notice("spx_profiler_start(): failure, nothing will be profiled");

        return;
    }

    size_t i;
    for (i = 0; i < context.profiling_handler.depth; i++) {
        context.profiling_handler.profiler->call_start(
            context.profiling_handler.profiler,
            &context.profiling_handler.stack[i]
        );
    }
}

static PHP_FUNCTION(spx_profiler_stop)
{
    if (context.execution_handler != &profiling_handler) {
        spx_php_log_notice("spx_profiler_stop(): profiling is not enabled");

        return;
    }

    if (context.config.auto_start) {
        spx_php_log_notice("spx_profiler_stop(): automatic start is not disabled");

        return;
    }

    if (context.profiling_handler.span_depth == 0) {
        /*
            No active span, nothing to do.
        */
        return;
    }

    context.profiling_handler.span_depth--;

    if (context.profiling_handler.span_depth > 0) {
        /*
            Leaving a nested span, nothing to do.
        */
        return;
    }

    profiling_handler_stop();

    if (context.profiling_handler.full_report_key[0]) {
#if PHP_API_VERSION >= 20151012
        RETURN_STRING(context.profiling_handler.full_report_key);
#else
        RETURN_STRING(context.profiling_handler.full_report_key, 1);
#endif
    }
}

static PHP_FUNCTION(spx_profiler_full_report_set_custom_metadata_str)
{
    char * custom_metadata_str;
#if PHP_API_VERSION >= 20151012
    size_t custom_metadata_str_len;
#else
    int custom_metadata_str_len;
#endif

#if PHP_API_VERSION >= 20170718
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(custom_metadata_str, custom_metadata_str_len)
    ZEND_PARSE_PARAMETERS_END();
#else
    if (
        zend_parse_parameters(
            ZEND_NUM_ARGS() TSRMLS_CC,
            "s",
            &custom_metadata_str,
            &custom_metadata_str_len
        ) == FAILURE
    ) {
        return;
    }
#endif

    if (context.config.report != SPX_CONFIG_REPORT_FULL) {
        spx_php_log_notice(
            "spx_profiler_full_report_set_custom_metadata_str(): `full` report required"
        );

        return;
    }

    if (custom_metadata_str_len > 4 * 1024) {
        spx_php_log_notice(
            "spx_profiler_full_report_set_custom_metadata_str(): too large $customMetadataStr string, it must not exceed 4KB"
        );

        return;
    }

    spx_reporter_full_set_custom_metadata_str(
        context.profiling_handler.reporter,
        custom_metadata_str
    );
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

    if (0 != strcmp(SPX_G(http_ip_var), "REMOTE_ADDR")) {
        if (!SPX_G(http_trusted_proxies) || SPX_G(http_trusted_proxies)[0] == 0) {
            /* empty client ip server var name -> not granted */
            spx_php_log_notice("access not granted: http_trusted_proxies is empty");

            return 0;
        }

        const char * proxy_ip_str = spx_php_global_array_get("_SERVER", "REMOTE_ADDR");
        int found = 0;

        SPX_UTILS_TOKENIZE_STRING(SPX_G(http_trusted_proxies), ',', trusted_proxy_ip_str, 64, {
            if (0 == strcmp(proxy_ip_str, trusted_proxy_ip_str)) {
                found = 1;
            }
        });

        if (!found) {
            /* empty client ip server var name -> not granted */
            spx_php_log_notice("access not granted: '%s' is not a trusted proxy", proxy_ip_str);

            return 0;
        }
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

    SPX_UTILS_TOKENIZE_STRING(authorized_ips_str, ',', authorized_ip_str, 64, {
        if (0 == strcmp(ip_str, authorized_ip_str)) {
            /* ip authorized (OK, as well as all previous checks) -> granted */

            return 1;
        }
    });

    if (0 == strcmp(authorized_ips_str, "*")) {
        /* all ips authorized */
        return 1;
    }

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
#ifdef USE_SIGNAL
    context.profiling_handler.sig_handling.handler_set = 0;
    context.profiling_handler.sig_handling.probing = 0;
    context.profiling_handler.sig_handling.stop = 0;
    context.profiling_handler.sig_handling.handler_called = 0;
    context.profiling_handler.sig_handling.signo = -1;
#endif

    profiling_handler_ex_set_context();

    context.profiling_handler.full_report_key[0] = 0;
    context.profiling_handler.reporter = NULL;
    context.profiling_handler.profiler = NULL;
    context.profiling_handler.depth = 0;
    context.profiling_handler.span_depth = 0;

    if (context.config.auto_start) {
        profiling_handler_start();
    }
}

static void profiling_handler_shutdown(void)
{
    profiling_handler_stop();
    profiling_handler_ex_unset_context();
}


static void profiling_handler_start(void)
{
    TSRMLS_FETCH();

    if (context.profiling_handler.profiler) {
        return;
    }

    context.profiling_handler.full_report_key[0] = 0;

    switch (context.config.report) {
        default:
        case SPX_CONFIG_REPORT_FULL:
            context.profiling_handler.reporter = spx_reporter_full_create(SPX_G(data_dir));
            if (context.profiling_handler.reporter) {
                snprintf(
                    context.profiling_handler.full_report_key,
                    sizeof(context.profiling_handler.full_report_key),
                    "%s",
                    spx_reporter_full_get_key(context.profiling_handler.reporter)
                );
            }

            break;

        case SPX_CONFIG_REPORT_FLAT_PROFILE:
            context.profiling_handler.reporter = spx_reporter_fp_create(
                context.config.fp_focus,
                context.config.fp_inc,
                context.config.fp_rel,
                context.config.fp_limit,
                context.config.fp_live,
                context.config.fp_color
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
    profiling_handler_stop();
}

static void profiling_handler_stop(void)
{
    spx_php_execution_finalize();

    if (context.profiling_handler.profiler) {
        context.profiling_handler.profiler->finalize(context.profiling_handler.profiler);
        context.profiling_handler.profiler->destroy(context.profiling_handler.profiler);
        context.profiling_handler.profiler = NULL;
    }

    if (context.profiling_handler.reporter) {
        spx_profiler_reporter_destroy(context.profiling_handler.reporter);
        context.profiling_handler.reporter = NULL;
    }
}

static void profiling_handler_ex_set_context(void)
{
#ifndef ZTS
    spx_php_global_hooks_set();
#endif

    spx_php_execution_init();

    spx_php_execution_hook(
        profiling_handler_ex_hook_before,
        profiling_handler_ex_hook_after,
        0
    );

    if (context.config.builtins) {
        spx_php_execution_hook(
            profiling_handler_ex_hook_before,
            profiling_handler_ex_hook_after,
            1
        );
    }

    spx_resource_stats_init();

#ifdef USE_SIGNAL
    if (context.cli_sapi && context.config.auto_start) {
        profiling_handler_sig_set_handler();
    }
#endif
}

static void profiling_handler_ex_unset_context(void)
{
#ifdef USE_SIGNAL
    if (context.cli_sapi && context.config.auto_start) {
        profiling_handler_sig_unset_handler();
    }
#endif

    spx_resource_stats_shutdown();
    spx_php_execution_shutdown();

#ifndef ZTS
    spx_php_global_hooks_unset();
#endif
}

static void profiling_handler_ex_hook_before(void)
{
    /*
        It might appear a bit unfair to resolve & copy the current function
        name in the context.profiling_handler.stack array even for
        non-profiled functions.
        But I've no other choice since I currently don't know how to safely
        & accuratly resolve the current stack (accordingly to SPX_BUILTINS)
        via the Zend Engine.
        The induced overhead will, however, not be noticable in most cases.
    */

    if (context.profiling_handler.depth == STACK_CAPACITY) {
        spx_utils_die("STACK_CAPACITY exceeded");
    }

    spx_php_function_t function;
    spx_php_current_function(&function);

    context.profiling_handler.stack[context.profiling_handler.depth] = function;

    context.profiling_handler.depth++;

    if (!context.profiling_handler.profiler) {
        return;
    }

#ifdef USE_SIGNAL
    context.profiling_handler.sig_handling.probing = 1;
#endif

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
    context.profiling_handler.depth--;

    if (!context.profiling_handler.profiler) {
        return;
    }

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
    spx_php_global_hooks_set();
#endif

    spx_php_execution_init();
    spx_php_execution_disable();
}

static void http_ui_handler_shutdown(void)
{
    TSRMLS_FETCH();

    if (!context.config.ui_uri) {
        goto error_404;
    }

    const char * ui_uri = context.config.ui_uri;
    if (
        ui_uri[0] == 0
        || 0 == strcmp(ui_uri, "/")
    ) {
        ui_uri = "/index.html";
    }

    if (0 == http_ui_handler_data(SPX_G(data_dir), ui_uri)) {
        goto finish;
    }

    char local_file_name[512];
    snprintf(
        local_file_name,
        sizeof(local_file_name),
        "%s%s",
        SPX_G(http_ui_assets_dir),
        ui_uri
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
    spx_php_execution_shutdown();

#ifndef ZTS
    spx_php_global_hooks_unset();
#endif
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

            spx_php_output_direct_printf("\"key\": \"%s\",", spx_metric_info[i].key);
            spx_php_output_direct_printf("\"short_name\": \"%s\",", spx_metric_info[i].short_name);
            spx_php_output_direct_printf("\"name\": \"%s\",", spx_metric_info[i].name);

            spx_php_output_direct_print("\"type\": \"");
            switch (spx_metric_info[i].type) {
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

            spx_php_output_direct_printf("\"releasable\": %d", spx_metric_info[i].releasable);

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
        spx_reporter_full_build_metadata_file_name(
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
        spx_reporter_full_build_file_name(
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
    snprintf(
        suffix,
        sizeof(suffix),
        "%s",
        file_name + (suffix_offset < 0 ? 0 : suffix_offset)
    );

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
