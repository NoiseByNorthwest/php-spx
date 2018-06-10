#ifndef SPX_REPORTER_FULL_H_DEFINED
#define SPX_REPORTER_FULL_H_DEFINED

#include "spx_profiler.h"

size_t spx_reporter_full_metadata_list_files(
    const char * data_dir,
    void (*callback) (const char *, size_t)
);

int spx_reporter_full_metadata_get_file_name(
    const char * data_dir,
    const char * key,
    char * file_name,
    size_t size
);

int spx_reporter_full_get_file_name(
    const char * data_dir,
    const char * key,
    char * file_name,
    size_t size
);

spx_profiler_reporter_t * spx_reporter_full_create(const char * data_dir);

#endif /* SPX_REPORTER_FULL_H_DEFINED */
