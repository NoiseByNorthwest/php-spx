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


#ifndef SPX_STR_BUILDER_H_DEFINED
#define SPX_STR_BUILDER_H_DEFINED

typedef struct spx_str_builder_t spx_str_builder_t;

spx_str_builder_t * spx_str_builder_create(size_t capacity);
void spx_str_builder_destroy(spx_str_builder_t * str_builder);

void spx_str_builder_reset(spx_str_builder_t * str_builder);
size_t spx_str_builder_capacity(const spx_str_builder_t * str_builder);
size_t spx_str_builder_size(const spx_str_builder_t * str_builder);
size_t spx_str_builder_remaining(const spx_str_builder_t * str_builder);
const char * spx_str_builder_str(const spx_str_builder_t * str_builder);

size_t spx_str_builder_append_double(spx_str_builder_t * str_builder, double d, size_t nb_dec);
size_t spx_str_builder_append_long(spx_str_builder_t * str_builder, long l);
size_t spx_str_builder_append_str(spx_str_builder_t * str_builder, const char * str);
size_t spx_str_builder_append_char(spx_str_builder_t * str_builder, char c);

#endif /* SPX_STR_BUILDER_H_DEFINED */
