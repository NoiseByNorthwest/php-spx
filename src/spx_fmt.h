/* SPX - A simple profiler for PHP
 * Copyright (C) 2017-2022 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


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
