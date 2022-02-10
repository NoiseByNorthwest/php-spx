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


#ifndef SPX_UTILS_H_DEFINED
#define SPX_UTILS_H_DEFINED

#include <stddef.h>

#define SPX_UTILS_TOKENIZE_STRING(str, delim, token, size, block) \
do {                                                              \
    const char * c_ = str;                                        \
    const char delim_ = delim;                                    \
    size_t i_ = 0;                                                \
    char token[size] = {0};                                       \
    while (1) {                                                   \
        if (*c_ == 0 || *c_ == delim_) {                          \
            token[i_] = 0;                                        \
                                                                  \
            do {                                                  \
                block                                             \
            } while(0);                                           \
                                                                  \
            if (*c_ == 0) {                                       \
                break;                                            \
            }                                                     \
                                                                  \
            i_ = 0;                                               \
        } else if (i_ < sizeof(token) - 1) {                      \
            token[i_] = *c_;                                      \
            i_++;                                                 \
        }                                                         \
                                                                  \
        c_++;                                                     \
    }                                                             \
} while (0)

char * spx_utils_json_escape(char * dst, const char * src, size_t limit);
int spx_utils_str_starts_with(const char * str, const char * prefix);
int spx_utils_str_ends_with(const char * str, const char * suffix);

#define spx_utils_die(msg) spx_utils_die_(msg, __FILE__, __LINE__)
void spx_utils_die_(const char * msg, const char * file, size_t line);

#endif /* SPX_UTILS_H_DEFINED */
