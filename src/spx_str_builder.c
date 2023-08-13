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


#include <stdlib.h>

#include "spx_str_builder.h"

struct spx_str_builder_t {
    size_t capacity;
    size_t size;
    char * buffer;
};

#define REMAINING_CHARS(s) ((s)->capacity - (s)->size)

spx_str_builder_t * spx_str_builder_create(size_t capacity)
{
    spx_str_builder_t * str_builder = malloc(sizeof(*str_builder));
    if (!str_builder) {
        return NULL;
    }

    str_builder->buffer = malloc(capacity + 1);
    if (!str_builder->buffer) {
        free(str_builder);

        return NULL;
    }

    str_builder->capacity = capacity;
    spx_str_builder_reset(str_builder);

    return str_builder;
}

void spx_str_builder_destroy(spx_str_builder_t * str_builder)
{
    free(str_builder->buffer);
    free(str_builder);
}

void spx_str_builder_reset(spx_str_builder_t * str_builder)
{
    str_builder->size = 0;
    str_builder->buffer[0] = 0;
}

size_t spx_str_builder_capacity(const spx_str_builder_t * str_builder)
{
    return str_builder->capacity;
}

size_t spx_str_builder_size(const spx_str_builder_t * str_builder)
{
    return str_builder->size;
}

size_t spx_str_builder_remaining(const spx_str_builder_t * str_builder)
{
    return REMAINING_CHARS(str_builder);
}

const char * spx_str_builder_str(const spx_str_builder_t * str_builder)
{
    return str_builder->buffer;
}

size_t spx_str_builder_append_double(spx_str_builder_t * str_builder, double d, size_t nb_dec)
{
    const size_t remaining = REMAINING_CHARS(str_builder);
    if (remaining == 0) {
        return 0;
    }

    char * p = str_builder->buffer + str_builder->size;

    if (d == 0) {
        return spx_str_builder_append_char(str_builder, '0');
    }

    size_t dec_factor = 1;
    size_t i = nb_dec;
    while (i--) {
        dec_factor *= 10;
    }

    long v = d * dec_factor + 0.5;
    int neg = v < 0;    
    if (neg) {
        v *= -1;
    }

    size_t c = 0, n = 0;
    while (v != 0) {
        n++;

        if (c + 1 > remaining) {
            str_builder->buffer[str_builder->size] = 0;

            return 0;
        }

        const char digit = (v % 10) + '0';

        if (digit == '0' && c == 0 && n <= nb_dec) {
            v /= 10;

            continue;
        }

        p[c++] = digit;

        v /= 10;

        if (n == nb_dec) {
            if (c + 1 > remaining) {
                str_builder->buffer[str_builder->size] = 0;

                return 0;
            }

            p[c++] = '.';
        }
    }

    if (neg) {
        if (c + 1 > remaining) {
            str_builder->buffer[str_builder->size] = 0;

            return 0;
        }

        p[c++] = '-';
    }

    p[c] = 0;

    for (i = 0; i < c / 2; i++) {
        const size_t opp = c - i - 1;

        p[i]   ^= p[opp];
        p[opp] ^= p[i];
        p[i]   ^= p[opp];
    }

    str_builder->size += c;

    return c;
}

size_t spx_str_builder_append_long(spx_str_builder_t * str_builder, long l)
{
    const size_t remaining = REMAINING_CHARS(str_builder);
    if (remaining == 0) {
        return 0;
    }

    char * p = str_builder->buffer + str_builder->size;

    if (l == 0) {
        return spx_str_builder_append_char(str_builder, '0');
    }

    long v = l;
    int neg = v < 0;    
    if (neg) {
        v *= -1;
    }

    size_t c = 0;
    while (v != 0) {
        if (c + 1 > remaining) {
            str_builder->buffer[str_builder->size] = 0;

            return 0;
        }

        p[c++] = (v % 10) + '0';
        v /= 10;
    }

    if (neg) {
        if (c + 1 > remaining) {
            str_builder->buffer[str_builder->size] = 0;

            return 0;
        }

        p[c++] = '-';
    }

    p[c] = 0;

    size_t i;
    for (i = 0; i < c / 2; i++) {
        const size_t opp = c - i - 1;

        p[i]   ^= p[opp];
        p[opp] ^= p[i];
        p[i]   ^= p[opp];
    }

    str_builder->size += c;

    return c;
}

size_t spx_str_builder_append_str(spx_str_builder_t * str_builder, const char * str)
{
    char * p = str_builder->buffer + str_builder->size;
    size_t c = 0;
    while (*str) {
        if (REMAINING_CHARS(str_builder) == 0) {
            str_builder->buffer[str_builder->size] = 0;

            return 0;
        }

        p[c++] = *str;
        str++;
        str_builder->size++;
    }

    p[c] = 0;

    return c;
}

size_t spx_str_builder_append_char(spx_str_builder_t * str_builder, char c)
{
    if (REMAINING_CHARS(str_builder) == 0) {
        return 0;
    }

    str_builder->buffer[str_builder->size++] = c;
    str_builder->buffer[str_builder->size] = 0;

    return 1;
}
