#ifndef SPX_FMT_H_DEFINED
#define SPX_FMT_H_DEFINED

#include <stddef.h>
#include "spx_output_stream.h"

typedef enum {
    SPX_FMT_TIME,
    SPX_FMT_MEMORY,
    SPX_FMT_QUANTITY,
    SPX_FMT_PERCENTAGE,
} spx_fmt_value_type_t;

void spx_fmt_format_value(
    char * str,
    size_t size,
    spx_fmt_value_type_t type,
    double value
);

void spx_fmt_print_value(
    spx_output_stream_t * output,
    spx_fmt_value_type_t type,
    double value
);

typedef struct spx_fmt_row_t spx_fmt_row_t;

spx_fmt_row_t * spx_fmt_row_create(void);
void spx_fmt_row_destroy(spx_fmt_row_t * row);

void spx_fmt_row_add_tcell(
    spx_fmt_row_t * row,
    size_t span,
    const char * text
);

void spx_fmt_row_add_ncell(
    spx_fmt_row_t * row,
    size_t span,
    spx_fmt_value_type_t type,
    double value
);

void spx_fmt_row_add_ncellf(
    spx_fmt_row_t * row,
    size_t span,
    spx_fmt_value_type_t type,
    double value,
    const char * ansi_fmt
);

void spx_fmt_row_print(const spx_fmt_row_t * row, spx_output_stream_t * output);
void spx_fmt_row_print_sep(const spx_fmt_row_t * row, spx_output_stream_t * output);

void spx_fmt_row_reset(spx_fmt_row_t * row);

#endif /* SPX_FMT_H_DEFINED */
