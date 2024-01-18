/* SPX - A simple profiler for PHP
 * Copyright (C) 2017-2024 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spx_utils.h"

char * spx_utils_json_escape(char * dst, const char * src, size_t limit)
{
    size_t i = 0;
    while (*src) {
        if (i >= limit) {
            goto limit_reached;
        }

        char escaped_char = 0;

        switch (*src) {
            case '\\':
            case '"':
            case '/':
                escaped_char = *src;

                break;

            case '\b':
                escaped_char = 'b';

                break;

            case '\f':
                escaped_char = 'f';

                break;

            case '\n':
                escaped_char = 'n';

                break;

            case '\r':
                escaped_char = 'r';

                break;

            case '\t':
                escaped_char = 't';

                break;
        }

        if (escaped_char != 0) {
            dst[i++] = '\\';

            if (i >= limit) {
                goto limit_reached;
            }

            dst[i++] = escaped_char;
        } else {
            dst[i++] = *src;
        }

        src++;
    }

    if (i >= limit) {
        goto limit_reached;
    }

    dst[i] = 0;

    return dst;

limit_reached:
    spx_utils_die("The provided buffer is too small to contain the escaped JSON string");

    /* unreacheable */
    return NULL;
}

int spx_utils_str_starts_with(const char * str, const char * prefix)
{
    return 0 == strncmp(str, prefix, strlen(prefix));
}

int spx_utils_str_ends_with(const char * str, const char * suffix)
{
    const size_t str_len = strlen(str);
    const size_t suffix_len = strlen(suffix);

    if (str_len < suffix_len) {
        return 0;
    }

    if (strcmp(str + str_len - suffix_len, suffix) == 0) {
        return 1;
    }

    return 0;
}

#ifdef ZTS
/* We just cannot kill other threads */
#   error "Fair error handling is required for ZTS"
#endif
void spx_utils_die_(const char * msg, const char * file, size_t line)
{
    fprintf(stderr, "SPX Fatal error at %s:%lu - %s\n", file, line, msg);

    exit(EXIT_FAILURE);
}
