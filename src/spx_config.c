#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "spx_config.h"
#include "spx_php.h"
#include "spx_utils.h"

typedef struct {
    const char * enabled_str;
    const char * key_str;

    const char * builtins_str;
    const char * depth_str;
    const char * metrics_str;

    const char * output_str;
    const char * output_file;

    const char * fp_focus_str;
    const char * fp_inc_str;
    const char * fp_rel_str;
    const char * fp_limit_str;
    const char * fp_live_str;

    const char * trace_safe_str;
} source_data_t;

typedef const char * (*source_handler_t) (const char * parameter);

static void init_config(spx_config_t * config);
static void fix_config(spx_config_t * config);
static void source_data_get(source_data_t * source_data, source_handler_t handler);
static void source_data_to_config(const source_data_t * source_data, spx_config_t * config);
static const char * source_handler_env(const char * parameter);
static const char * source_handler_http_header(const char * parameter);
static const char * source_handler_http_query_string(const char * parameter);

void spx_config_read(spx_config_t * config, ...)
{
    init_config(config);

    source_data_t source_data;

    va_list ap;
    va_start(ap, config);

    while (1) {
        spx_config_source_t source = va_arg(ap, spx_config_source_t);
        source_handler_t source_handler = NULL;
        switch (source) {
            case SPX_CONFIG_SOURCE_ENV:
                source_handler = source_handler_env;
                break;

            case SPX_CONFIG_SOURCE_HTTP_HEADER:
                source_handler = source_handler_http_header;
                break;

            case SPX_CONFIG_SOURCE_HTTP_QUERY_STRING:
                source_handler = source_handler_http_query_string;
                break;

            default:
                ;
        }

        if (!source_handler) {
            break;
        }

        source_data_get(&source_data, source_handler);
        source_data_to_config(&source_data, config);
    }

    va_end(ap);

    fix_config(config);
}

static void init_config(spx_config_t * config)
{
    config->enabled = 0;
    config->key = NULL;

    config->builtins = 0;
    config->max_depth = 0;

    SPX_METRIC_FOREACH(i, {
        config->enabled_metrics[i] = 0;
    });

    config->enabled_metrics[SPX_METRIC_WALL_TIME] = 1;
    config->enabled_metrics[SPX_METRIC_ZE_MEMORY] = 1;

    config->output = SPX_CONFIG_OUTPUT_FLAT_PROFILE;
    config->output_file = NULL;

    config->fp_focus = SPX_METRIC_WALL_TIME;
    config->fp_inc = 0;
    config->fp_rel = 0;
    config->fp_limit = 10;
    config->fp_live = 0;

    config->trace_safe = 0;
}

static void fix_config(spx_config_t * config)
{
    if (config->output == SPX_CONFIG_OUTPUT_FLAT_PROFILE) {
        config->enabled_metrics[config->fp_focus] = 1;
    }
}

static void source_data_get(source_data_t * source_data, source_handler_t handler)
{
    source_data->enabled_str     = handler("SPX_ENABLED");
    source_data->key_str         = handler("SPX_KEY");
    source_data->builtins_str    = handler("SPX_BUILTINS");
    source_data->depth_str       = handler("SPX_DEPTH");
    source_data->metrics_str     = handler("SPX_METRICS");
    source_data->output_str      = handler("SPX_OUTPUT");
    source_data->output_file     = handler("SPX_OUTPUT_FILE");
    source_data->fp_focus_str    = handler("SPX_FP_FOCUS");
    source_data->fp_inc_str      = handler("SPX_FP_INC");
    source_data->fp_rel_str      = handler("SPX_FP_REL");
    source_data->fp_limit_str    = handler("SPX_FP_LIMIT");
    source_data->fp_live_str     = handler("SPX_FP_LIVE");
    source_data->trace_safe_str  = handler("SPX_TRACE_SAFE");
}

static void source_data_to_config(const source_data_t * source_data, spx_config_t * config)
{
    if (source_data->enabled_str) {
        config->enabled = *source_data->enabled_str == '1' ? 1 : 0;
    }

    if (source_data->key_str) {
        config->key = source_data->key_str;
    }

    if (source_data->builtins_str) {
        config->builtins = *source_data->builtins_str == '1' ? 1 : 0;
    }

    if (source_data->depth_str) {
        config->max_depth = atoi(source_data->depth_str);
    }

    if (source_data->metrics_str) {
        SPX_METRIC_FOREACH(i, {
            config->enabled_metrics[i] = 0;
        });

        SPX_UTILS_TOKENIZE_STRING(source_data->metrics_str, ',', token, 32, {
            spx_metric_t metric = spx_metric_get_by_short_name(token);
            if (metric != SPX_METRIC_NONE) {
                config->enabled_metrics[metric] = 1;
            }
        });
    }

    if (source_data->output_str) {
        if (0 == strcmp(source_data->output_str, "fp")) {
            config->output = SPX_CONFIG_OUTPUT_FLAT_PROFILE;
        } else if (0 == strcmp(source_data->output_str, "cg")) {
            config->output = SPX_CONFIG_OUTPUT_CALLGRIND;
        } else if (0 == strcmp(source_data->output_str, "gte")) {
            config->output = SPX_CONFIG_OUTPUT_GOOGLE_TRACE_EVENT;
        } else if (0 == strcmp(source_data->output_str, "trace")) {
            config->output = SPX_CONFIG_OUTPUT_TRACE;
        }
    }

    if (source_data->output_file) {
        config->output_file = source_data->output_file;
    }

    if (source_data->fp_focus_str) {
        spx_metric_t focus = spx_metric_get_by_short_name(source_data->fp_focus_str);
        if (focus != SPX_METRIC_NONE) {
            config->fp_focus = focus;
        }
    }

    if (source_data->fp_inc_str) {
        config->fp_inc = *source_data->fp_inc_str == '1' ? 1 : 0;
    }

    if (source_data->fp_rel_str) {
        config->fp_rel = *source_data->fp_rel_str == '1' ? 1 : 0;
    }

    if (source_data->fp_limit_str) {
        config->fp_limit = atoi(source_data->fp_limit_str);
    }

    if (source_data->fp_live_str) {
        config->fp_live = *source_data->fp_live_str == '1' ? 1 : 0;
    }

    if (source_data->trace_safe_str) {
        config->trace_safe = *source_data->trace_safe_str == '1' ? 1 : 0;
    }
}

static const char * source_handler_env(const char * parameter)
{
    return getenv(parameter);
}

static const char * source_handler_http_header(const char * parameter)
{
    char key[128] = "HTTP_";

    return spx_php_global_array_get("_SERVER", strncat(
        key,
        parameter,
        sizeof(key) - strlen(key) - 1
    ));
}

static const char * source_handler_http_query_string(const char * parameter)
{
    return spx_php_global_array_get("_GET", parameter);
}
