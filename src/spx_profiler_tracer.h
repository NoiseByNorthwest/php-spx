#ifndef SPX_PROFILER_TRACER_H_DEFINED
#define SPX_PROFILER_TRACER_H_DEFINED

#include <stddef.h>

#include "spx_profiler.h"

spx_profiler_t * spx_profiler_tracer_create(
    size_t max_depth,
    const int * enabled_metrics,
    spx_profiler_reporter_t * reporter
);

#endif /* SPX_PROFILER_TRACER_H_DEFINED */
