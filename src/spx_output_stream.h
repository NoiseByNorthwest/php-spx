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


#ifndef SPX_OUTPUT_STREAM_H_DEFINED
#define SPX_OUTPUT_STREAM_H_DEFINED

typedef struct spx_output_stream_t spx_output_stream_t;

spx_output_stream_t * spx_output_stream_open(const char * file_name, int compressed);
spx_output_stream_t * spx_output_stream_dopen(int fileno, int compressed);

void spx_output_stream_close(spx_output_stream_t * output);

void spx_output_stream_print(spx_output_stream_t * output, const char * str);
void spx_output_stream_printf(spx_output_stream_t * output, const char * format, ...);

void spx_output_stream_flush(spx_output_stream_t * output);

#endif /* SPX_OUTPUT_STREAM_H_DEFINED */
