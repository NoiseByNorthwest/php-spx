#ifndef SPX_CONFIG_H_DEFINED
#define SPX_CONFIG_H_DEFINED

#include <stddef.h>
#include "spx_metric.h"

typedef enum {
    SPX_CONFIG_OUTPUT_FLAT_PROFILE,
    SPX_CONFIG_OUTPUT_CALLGRIND,
    SPX_CONFIG_OUTPUT_GOOGLE_TRACE_EVENT,
    SPX_CONFIG_OUTPUT_TRACE,
} spx_config_output_t;

typedef struct {
    int enabled;
    const char * key;

    int builtins;
    size_t max_depth;
    int enabled_metrics[SPX_METRIC_COUNT];

    spx_config_output_t output;
    const char * output_file;

    spx_metric_t fp_focus;
    int fp_inc;
    int fp_rel;
    size_t fp_limit;
    int fp_live;

    int trace_safe;
} spx_config_t;

typedef enum {
    SPX_CONFIG_SOURCE_ENV,
    SPX_CONFIG_SOURCE_HTTP_HEADER,
    SPX_CONFIG_SOURCE_HTTP_QUERY_STRING,
} spx_config_source_t;

void spx_config_init(spx_config_t * config);
void spx_config_read(spx_config_t * config, spx_config_source_t source);

#endif /* SPX_CONFIG_H_DEFINED */
