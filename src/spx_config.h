#ifndef SPX_CONFIG_H_DEFINED
#define SPX_CONFIG_H_DEFINED

#include <stddef.h>
#include "spx_metric.h"

typedef enum {
    SPX_CONFIG_REPORT_FULL,
    SPX_CONFIG_REPORT_FLAT_PROFILE,
    SPX_CONFIG_REPORT_TRACE,
} spx_config_report_t;

typedef struct {
    int enabled;
    const char * key;

    const char * ui_uri;

    size_t sampling_period;
    int builtins;
    size_t max_depth;
    int enabled_metrics[SPX_METRIC_COUNT];

    spx_config_report_t report;

    spx_metric_t fp_focus;
    int fp_inc;
    int fp_rel;
    size_t fp_limit;
    int fp_live;
    int fp_color;

    const char * trace_file;
    int trace_safe;
} spx_config_t;

typedef enum {
    SPX_CONFIG_SOURCE_ENV,
    SPX_CONFIG_SOURCE_HTTP_COOKIE,
    SPX_CONFIG_SOURCE_HTTP_HEADER,
    SPX_CONFIG_SOURCE_HTTP_QUERY_STRING,
} spx_config_source_t;

void spx_config_get(spx_config_t * config, int cli, ...);

#endif /* SPX_CONFIG_H_DEFINED */
