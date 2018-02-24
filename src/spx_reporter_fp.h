#ifndef SPX_REPORTER_FP_H_DEFINED
#define SPX_REPORTER_FP_H_DEFINED

#include "spx_profiler.h"

spx_profiler_reporter_t * spx_reporter_fp_create(
    spx_metric_t focus,
    int inc,
    int rel,
    size_t limit,
    int live
);

#endif /* SPX_REPORTER_FP_H_DEFINED */
