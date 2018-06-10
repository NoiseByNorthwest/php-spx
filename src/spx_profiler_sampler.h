#ifndef SPX_PROFILER_SAMPLER_H_DEFINED
#define SPX_PROFILER_SAMPLER_H_DEFINED

#include <stddef.h>

#include "spx_profiler.h"

spx_profiler_t * spx_profiler_sampler_create(
    spx_profiler_t * sampled_profiler,
    size_t sampling_period_us
);

#endif /* SPX_PROFILER_SAMPLER_H_DEFINED */
