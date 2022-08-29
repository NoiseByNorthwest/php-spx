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


/* _GNU_SOURCE is implicitly defined since PHP 8.2 https://github.com/php/php-src/pull/8807 */
#ifndef _GNU_SOURCE
#   define _GNU_SOURCE /* vasprintf */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <zlib.h>

#include "spx_output_stream.h"

typedef struct {
    void * (*open)     (const char * file_name);
    void * (*dopen)    (int fileno);
    void   (*close)    (void * file);
    void   (*flush)    (void * file);
    int    (*print)    (void * file, const char * str);
    int    (*vprintf)  (void * file, const char * fmt, va_list ap);
} file_handler_t;

struct spx_output_stream_t {
    const file_handler_t * file_handler;
    void * file;
    int owned;
};

static void * stdio_file_handler_open(const char * file_name);
static void * stdio_file_handler_dopen(int fileno);
static void stdio_file_handler_close(void * file);
static void stdio_file_handler_flush(void * file);
static int stdio_file_handler_print(void * file, const char * str);
static int stdio_file_handler_vprintf(void * file, const char * fmt, va_list ap);

static void * gz_file_handler_open(const char * file_name);
static void * gz_file_handler_dopen(int fileno);
static void gz_file_handler_close(void * file);
static void gz_file_handler_flush(void * file);
static int gz_file_handler_print(void * file, const char * str);
static int gz_file_handler_vprintf(void * file, const char * fmt, va_list ap);

static file_handler_t stdio_file_handler = {
    stdio_file_handler_open,
    stdio_file_handler_dopen,
    stdio_file_handler_close,
    stdio_file_handler_flush,
    stdio_file_handler_print,
    stdio_file_handler_vprintf
};

static file_handler_t gz_file_handler = {
    gz_file_handler_open,
    gz_file_handler_dopen,
    gz_file_handler_close,
    gz_file_handler_flush,
    gz_file_handler_print,
    gz_file_handler_vprintf
};

static spx_output_stream_t * create_output_stream(const file_handler_t * file_handler, void * file, int owned)
{
    if (!file) {
        return NULL;
    }

    spx_output_stream_t * output = malloc(sizeof(*output));
    if (!output) {
        if (owned) {
            file_handler->close(file);
        }

        return NULL;
    }

    output->file_handler = file_handler;
    output->file = file;
    output->owned = owned;

    return output;
}

spx_output_stream_t * spx_output_stream_open(const char * file_name, int compressed)
{
    const file_handler_t * file_handler = compressed ? &gz_file_handler : &stdio_file_handler;

    return create_output_stream(file_handler, file_handler->open(file_name), 1);
}

spx_output_stream_t * spx_output_stream_dopen(int fileno, int compressed)
{
    const file_handler_t * file_handler = compressed ? &gz_file_handler : &stdio_file_handler;

    return create_output_stream(file_handler, file_handler->dopen(fileno), 0);
}

void spx_output_stream_close(spx_output_stream_t * output)
{
    if (output->owned) {
        output->file_handler->close(output->file);
    } else {
        output->file_handler->flush(output->file);
    }

    free(output);
}

void spx_output_stream_print(spx_output_stream_t * output, const char * str)
{
    output->file_handler->print(output->file, str);
}

void spx_output_stream_printf(spx_output_stream_t * output, const char * format, ...)
{
    va_list argp;
    va_start(argp, format);
    output->file_handler->vprintf(output->file, format, argp);
    va_end(argp);
}

void spx_output_stream_flush(spx_output_stream_t * output)
{
    output->file_handler->flush(output->file);
}

static void * stdio_file_handler_open(const char * file_name)
{
    return fopen(file_name, "w");
}

static void * stdio_file_handler_dopen(int fileno)
{
    return fdopen(fileno, "w");
}

static void stdio_file_handler_close(void * file)
{
    fclose(file);
}

static void stdio_file_handler_flush(void * file)
{
    fflush(file);
}

static int stdio_file_handler_print(void * file, const char * str)
{
    return fputs(str, file);
}

static int stdio_file_handler_vprintf(void * file, const char * fmt, va_list ap)
{
    return vfprintf(file, fmt, ap);
}

static void * gz_file_handler_open(const char * file_name)
{
    return gzopen(file_name, "w1");
}

static void * gz_file_handler_dopen(int fileno)
{
    return gzdopen(fileno, "w1");
}

static void gz_file_handler_close(void * file)
{
    gzclose(file);
}

static void gz_file_handler_flush(void * file)
{
    gzflush(file, Z_SYNC_FLUSH);
}

static int gz_file_handler_print(void * file, const char * str)
{
    return gzputs(file, str);
}

static int gz_file_handler_vprintf(void * file, const char * fmt, va_list ap)
{
    char * buf;
    int printed = vasprintf(&buf, fmt, ap);
    if (printed < 0) {
        return printed;
    }

    printed = gzputs(file, buf);
    free(buf);

    return printed;
}
