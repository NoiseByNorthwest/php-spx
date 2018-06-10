#include <stdlib.h>

#include "spx_profiler.h"


void spx_profiler_reporter_destroy(spx_profiler_reporter_t * reporter)
{
    if (reporter->destroy) {
        reporter->destroy(reporter);
    }

    free(reporter);
}
