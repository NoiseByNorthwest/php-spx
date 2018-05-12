#ifndef SPX_METRIC_H_DEFINED
#define SPX_METRIC_H_DEFINED

#include "spx_fmt.h"

typedef enum {
    SPX_METRIC_WALL_TIME,
    SPX_METRIC_CPU_TIME,
    SPX_METRIC_IDLE_TIME,

    SPX_METRIC_ZE_MEMORY,
    SPX_METRIC_ZE_GC_RUNS,
    SPX_METRIC_ZE_GC_ROOT_BUFFER,
    SPX_METRIC_ZE_GC_COLLECTED,
    SPX_METRIC_ZE_INCLUDED_FILE_COUNT,
    SPX_METRIC_ZE_CLASS_COUNT,
    SPX_METRIC_ZE_FUNCTION_COUNT,
    SPX_METRIC_ZE_OBJECT_COUNT,
    SPX_METRIC_ZE_ERROR_COUNT,

    SPX_METRIC_IO_BYTES,
    SPX_METRIC_IO_RBYTES,
    SPX_METRIC_IO_WBYTES,

    SPX_METRIC_COUNT,
    SPX_METRIC_NONE,
} spx_metric_t;

#define SPX_METRIC_FOREACH(it, block)            \
do {                                             \
    size_t it;                                   \
    for (it = 0; it < SPX_METRIC_COUNT; it++) {  \
        block                                    \
    }                                            \
} while (0)

typedef struct {
    const char * key;
    const char * short_name;
    const char * name;
    spx_fmt_value_type_t type;
    int releasable;
} spx_metric_info_t;

extern const spx_metric_info_t spx_metrics_info[SPX_METRIC_COUNT];

spx_metric_t spx_metric_get_by_key(const char * key);

typedef struct spx_metric_collector_t spx_metric_collector_t;

spx_metric_collector_t * spx_metric_collector_create(const int * enabled_metrics);
void spx_metric_collector_destroy(spx_metric_collector_t * collector);

void spx_metric_collector_collect(spx_metric_collector_t * collector, double * values);
void spx_metric_collector_noise_barrier(spx_metric_collector_t * collector);
void spx_metric_collector_add_fixed_noise(spx_metric_collector_t * collector, const double * noise);

#endif /* SPX_METRIC_H_DEFINED */
