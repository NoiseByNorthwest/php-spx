/* SPX - A simple profiler for PHP
 * Copyright (C) 2017-2025 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
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

#ifdef HAVE_ZSTD
#   include <zstd.h>
#endif

#ifdef HAVE_LZ4
#   include <lz4frame.h>
#endif

#include "spx_output_stream.h"


typedef struct {
    void * (*open)     (const char * file_name);
    void * (*dopen)    (int fileno);
    int    (*close)    (void * file, int fd_owned);
    int    (*flush)    (void * file);
    int    (*print)    (void * file, const char * str);
    int    (*vprintf)  (void * file, const char * fmt, va_list ap);
    int    (*write)    (void * file, const void * buf, size_t len);
} file_driver_t;

struct spx_output_stream_t {
    const file_driver_t * file_driver;
    void * file;
    int fd_owned;
};


static file_driver_t * resolve_file_driver(spx_output_stream_compression_type_t compression_type);
static spx_output_stream_t * create_output_stream(const file_driver_t * file_driver, void * file, int fd_owned);

static void * stdio_file_driver_open(const char * file_name);
static void * stdio_file_driver_dopen(int fileno);
static int stdio_file_driver_close(void * file, int fd_owned);
static int stdio_file_driver_flush(void * file);
static int stdio_file_driver_print(void * file, const char * str);
static int stdio_file_driver_vprintf(void * file, const char * fmt, va_list ap);
static int stdio_file_driver_write(void * file, const void * buf, size_t len);

static void * gz_file_driver_open(const char * file_name);
static void * gz_file_driver_dopen(int fileno);
static int gz_file_driver_close(void * file, int fd_owned);
static int gz_file_driver_flush(void * file);
static int gz_file_driver_print(void * file, const char * str);
static int gz_file_driver_vprintf(void * file, const char * fmt, va_list ap);
static int gz_file_driver_write(void * file, const void * buf, size_t len);

#ifdef HAVE_ZSTD

typedef struct {
    FILE * file;
    size_t buffer_capacity;
    size_t buffer_size;
    void * buffer;
    size_t compressed_buffer_capacity;
    void * compressed_buffer;
} zstd_file_t;

static void * zstd_file_driver_open(const char * file_name);
static void * zstd_file_driver_dopen(int fileno);
static void * zstd_file_driver_create(FILE * file, int fd_owned);
static int zstd_file_driver_close(void * file, int fd_owned);
static int zstd_file_driver_flush(void * file);
static int zstd_file_driver_print(void * file, const char * str);
static int zstd_file_driver_vprintf(void * file, const char * fmt, va_list ap);
static int zstd_file_driver_write(void * file, const void * buf, size_t len);
static int zstd_file_driver_write_to_buffer(zstd_file_t * zstd_file, const void * buf, size_t len);
static int zstd_file_driver_flush_buffer(zstd_file_t * zstd_file, int flush_zstd_buffer);

#endif

#ifdef HAVE_LZ4

typedef struct {
    FILE * file;
    LZ4F_compressionContext_t compression_ctx;
    size_t buffer_capacity;
    size_t buffer_size;
    void * buffer;
    size_t compressed_buffer_capacity;
    void * compressed_buffer;
} lz4_file_t;

static void * lz4_file_driver_open(const char * file_name);
static void * lz4_file_driver_dopen(int fileno);
static void * lz4_file_driver_create(FILE * file, int fd_owned);
static int lz4_file_driver_close(void * file, int fd_owned);
static int lz4_file_driver_flush(void * file);
static int lz4_file_driver_print(void * file, const char * str);
static int lz4_file_driver_vprintf(void * file, const char * fmt, va_list ap);
static int lz4_file_driver_write(void * file, const void * buf, size_t len);
static int lz4_file_driver_write_to_buffer(lz4_file_t * lz4_file, const void * buf, size_t len);
static int lz4_file_driver_flush_buffer(lz4_file_t * lz4_file, int flush_lz4_buffer);

#endif

static file_driver_t stdio_file_driver = {
    stdio_file_driver_open,
    stdio_file_driver_dopen,
    stdio_file_driver_close,
    stdio_file_driver_flush,
    stdio_file_driver_print,
    stdio_file_driver_vprintf,
    stdio_file_driver_write
};

static file_driver_t gz_file_driver = {
    gz_file_driver_open,
    gz_file_driver_dopen,
    gz_file_driver_close,
    gz_file_driver_flush,
    gz_file_driver_print,
    gz_file_driver_vprintf,
    gz_file_driver_write
};

#ifdef HAVE_ZSTD
static file_driver_t zstd_file_driver = {
    zstd_file_driver_open,
    zstd_file_driver_dopen,
    zstd_file_driver_close,
    zstd_file_driver_flush,
    zstd_file_driver_print,
    zstd_file_driver_vprintf,
    zstd_file_driver_write
};
#endif

#ifdef HAVE_LZ4
static file_driver_t lz4_file_driver = {
    lz4_file_driver_open,
    lz4_file_driver_dopen,
    lz4_file_driver_close,
    lz4_file_driver_flush,
    lz4_file_driver_print,
    lz4_file_driver_vprintf,
    lz4_file_driver_write
};
#endif


const char * spx_output_stream_compression_format_ext(spx_output_stream_compression_type_t compression_type)
{
    switch(compression_type) {
        case SPX_OUTPUT_STREAM_COMPRESSION_GZIP:
            return "gz";

#ifdef HAVE_ZSTD
        case SPX_OUTPUT_STREAM_COMPRESSION_ZSTD:
            return "zst";
#endif

#ifdef HAVE_LZ4
        case SPX_OUTPUT_STREAM_COMPRESSION_LZ4:
            return "lz4";
#endif

        default:
            return "";
    }
}

spx_output_stream_t * spx_output_stream_open(const char * file_name, spx_output_stream_compression_type_t compression_type)
{
    const file_driver_t * file_driver = resolve_file_driver(compression_type);

    return create_output_stream(
        file_driver,
        file_driver->open(file_name),
        1
    );
}

spx_output_stream_t * spx_output_stream_dopen(int fileno, spx_output_stream_compression_type_t compression_type)
{
    const file_driver_t * file_driver = resolve_file_driver(compression_type);

    return create_output_stream(
        file_driver,
        file_driver->dopen(fileno),
        0
    );
}

int spx_output_stream_close(spx_output_stream_t * output)
{
    const int ret = output->file_driver->close(output->file, output->fd_owned);
    free(output);

    return ret;
}

int spx_output_stream_print(spx_output_stream_t * output, const char * str)
{
    return output->file_driver->print(output->file, str);
}

int spx_output_stream_printf(spx_output_stream_t * output, const char * format, ...)
{
    va_list argp;
    va_start(argp, format);
    const int ret = output->file_driver->vprintf(output->file, format, argp);
    va_end(argp);

    return ret;
}

int spx_output_stream_write(spx_output_stream_t * output, const void * buf, size_t len)
{
    return output->file_driver->write(output->file, buf, len);
}

int spx_output_stream_flush(spx_output_stream_t * output)
{
    return output->file_driver->flush(output->file);
}

static file_driver_t * resolve_file_driver(spx_output_stream_compression_type_t compression_type)
{
    switch(compression_type) {
        case SPX_OUTPUT_STREAM_COMPRESSION_GZIP:
            return &gz_file_driver;

#ifdef HAVE_ZSTD
        case SPX_OUTPUT_STREAM_COMPRESSION_ZSTD:
            return &zstd_file_driver;
#endif

#ifdef HAVE_LZ4
        case SPX_OUTPUT_STREAM_COMPRESSION_LZ4:
            return &lz4_file_driver;
#endif

        default:
            return &stdio_file_driver;
    }
}

static spx_output_stream_t * create_output_stream(const file_driver_t * file_driver, void * file, int fd_owned)
{
    if (!file) {
        return NULL;
    }

    spx_output_stream_t * output = malloc(sizeof(*output));
    if (!output) {
        file_driver->close(file, fd_owned);

        return NULL;
    }

    output->file_driver = file_driver;
    output->file = file;
    output->fd_owned = fd_owned;

    return output;
}

static void * stdio_file_driver_open(const char * file_name)
{
    return fopen(file_name, "w");
}

static void * stdio_file_driver_dopen(int fileno)
{
    return fdopen(fileno, "w");
}

static int stdio_file_driver_close(void * file, int fd_owned)
{
    if (fd_owned) {
        return fclose(file);
    }

    return fflush(file);
}

static int stdio_file_driver_flush(void * file)
{
    return fflush(file);
}

static int stdio_file_driver_print(void * file, const char * str)
{
    return fputs(str, file);
}

static int stdio_file_driver_vprintf(void * file, const char * fmt, va_list ap)
{
    return vfprintf(file, fmt, ap);
}

static int stdio_file_driver_write(void * file, const void * buf, size_t len)
{
    return fwrite(buf, 1, len, file);
}

static void * gz_file_driver_open(const char * file_name)
{
    // FIXME the level should be configurable
    return gzopen(file_name, "w1");
}

static void * gz_file_driver_dopen(int fileno)
{
    // FIXME the level should be configurable
    return gzdopen(fileno, "w1");
}

static int gz_file_driver_close(void * file, int fd_owned)
{
    if (fd_owned) {
        return gzclose(file);
    }

    return gz_file_driver_flush(file);
}

static int gz_file_driver_flush(void * file)
{
    return gzflush(file, Z_SYNC_FLUSH);
}

static int gz_file_driver_print(void * file, const char * str)
{
    return gzputs(file, str);
}

static int gz_file_driver_vprintf(void * file, const char * fmt, va_list ap)
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

static int gz_file_driver_write(void * file, const void * buf, size_t len)
{
    return gzwrite(file, buf, len);
}

#ifdef HAVE_ZSTD

static void * zstd_file_driver_open(const char * file_name)
{
    return zstd_file_driver_create(fopen(file_name, "wb"), 1);
}

static void * zstd_file_driver_dopen(int fileno)
{
    return zstd_file_driver_create(fdopen(fileno, "wb"), 0);
}

static void * zstd_file_driver_create(FILE * file, int fd_owned)
{
    zstd_file_t * zstd_file = NULL;
    
    if (!file) {
        goto error;
    }

    zstd_file = malloc(sizeof(*zstd_file));
    if (!zstd_file) {
        goto error;
    }

    zstd_file->file = file;
    zstd_file->buffer_capacity = 512 * 1024;
    zstd_file->buffer_size = 0;
    zstd_file->buffer = NULL;
    zstd_file->compressed_buffer_capacity = 0;
    zstd_file->compressed_buffer = NULL;

    zstd_file->buffer = malloc(zstd_file->buffer_capacity);
    if (!zstd_file->buffer) {
        goto error;
    }

    zstd_file->compressed_buffer_capacity = ZSTD_compressBound(zstd_file->buffer_capacity);
    zstd_file->compressed_buffer = malloc(zstd_file->compressed_buffer_capacity);
    if (!zstd_file->compressed_buffer) {
        goto error;
    }

    return zstd_file;

error:
    if (zstd_file) {
        if (zstd_file->buffer) {
            free(zstd_file->buffer);
        }

        if (zstd_file->compressed_buffer) {
            free(zstd_file->compressed_buffer);
        }

        free(zstd_file);
    }


    if (file && fd_owned) {
        fclose(file);
    }

    return NULL;
}

static int zstd_file_driver_close(void * file, int fd_owned)
{
    zstd_file_t * zstd_file = file;

    int status = 0;

    if (zstd_file_driver_flush(file) != 0) {
        status = EOF;
    }

    if (fd_owned) {
        int fclose_status = fclose(zstd_file->file);
        if (fclose_status != 0) {
            status = fclose_status;
        }
    }

    free(zstd_file->buffer);
    free(zstd_file->compressed_buffer);
    free(zstd_file);

    return status;
}

static int zstd_file_driver_flush(void * file)
{
    zstd_file_t * zstd_file = file;

    if (zstd_file_driver_flush_buffer(zstd_file, 1) != 0) {
        return EOF;
    }
    
    return fflush(zstd_file->file);
}

static int zstd_file_driver_print(void * file, const char * str)
{
    zstd_file_t * zstd_file = file;

    return zstd_file_driver_write_to_buffer(zstd_file, str, strlen(str));
}

static int zstd_file_driver_vprintf(void * file, const char * fmt, va_list ap)
{
    zstd_file_t * zstd_file = file;

    char * buf;
    int printed = vasprintf(&buf, fmt, ap);
    if (printed < 0) {
        return printed;
    }

    printed = zstd_file_driver_write_to_buffer(zstd_file, buf, strlen(buf));
    free(buf);

    return printed;
}

static int zstd_file_driver_write(void * file, const void * buf, size_t len)
{
    return zstd_file_driver_write_to_buffer(file, buf, len);
}

static int zstd_file_driver_write_to_buffer(zstd_file_t * zstd_file, const void * buf, size_t len)
{
    size_t written_byte_count = 0;

    while (len > 0) {
        if (
            zstd_file->buffer_capacity == zstd_file->buffer_size &&
            zstd_file_driver_flush_buffer(zstd_file, 0) != 0
        ) {
            return written_byte_count;
        }

        const size_t remaining_buffer_len = zstd_file->buffer_capacity - zstd_file->buffer_size;
        const size_t chunk_size = len > remaining_buffer_len ? remaining_buffer_len : len;
        
        memcpy(zstd_file->buffer + zstd_file->buffer_size, buf, chunk_size);
        
        len -= chunk_size;
        buf += chunk_size;
        zstd_file->buffer_size += chunk_size;
        written_byte_count += chunk_size;
    }

    return written_byte_count;
}

static int zstd_file_driver_flush_buffer(zstd_file_t * zstd_file, int flush_zstd_buffer)
{
    if (zstd_file->buffer_size > 0) {
        size_t compressed_size = ZSTD_compress(
            zstd_file->compressed_buffer,
            zstd_file->compressed_buffer_capacity,
            zstd_file->buffer,
            zstd_file->buffer_size,
            1 // compression level, FIXME should be configurable
        );

        if (ZSTD_isError(compressed_size)) {
            return 1;
        }

        if (
            fwrite(
                zstd_file->compressed_buffer,
                1,
                compressed_size,
                zstd_file->file
            ) != compressed_size
        ) {
            return 1;
        }

        zstd_file->buffer_size = 0;
    }

    return 0;
}

#endif

#ifdef HAVE_LZ4

static void * lz4_file_driver_open(const char * file_name)
{
    return lz4_file_driver_create(fopen(file_name, "wb"), 1);
}

static void * lz4_file_driver_dopen(int fileno)
{
    return lz4_file_driver_create(fdopen(fileno, "wb"), 0);
}

static void * lz4_file_driver_create(FILE * file, int fd_owned)
{
    lz4_file_t * lz4_file = NULL;
    
    if (!file) {
        goto error;
    }

    lz4_file = malloc(sizeof(*lz4_file));
    if (!lz4_file) {
        goto error;
    }

    lz4_file->file = file;
    lz4_file->buffer_capacity = 4 * 1024;
    lz4_file->buffer_size = 0;
    lz4_file->buffer = NULL;
    lz4_file->compressed_buffer_capacity = 0;
    lz4_file->compressed_buffer = NULL;

    int compression_ctx_initialized = 0;

    if (
        LZ4F_isError(
            LZ4F_createCompressionContext(&lz4_file->compression_ctx, LZ4F_VERSION)
        )
    ) {
        goto error;
    }

    compression_ctx_initialized = 1;

    LZ4F_preferences_t prefs = {0};
    // FIXME should be configurable
    prefs.compressionLevel = 0;

    char header_buf[LZ4F_HEADER_SIZE_MAX];

    size_t header_size = LZ4F_compressBegin(lz4_file->compression_ctx, header_buf, sizeof header_buf, &prefs);
    if (fwrite(header_buf, 1, header_size, lz4_file->file) != header_size) {
        goto error;
    }

    lz4_file->buffer = malloc(lz4_file->buffer_capacity);
    if (!lz4_file->buffer) {
        goto error;
    }

    lz4_file->compressed_buffer_capacity = LZ4F_compressBound(lz4_file->buffer_capacity, &prefs);
    lz4_file->compressed_buffer = malloc(lz4_file->compressed_buffer_capacity);
    if (!lz4_file->compressed_buffer) {
        goto error;
    }

    return lz4_file;

error:
    if (lz4_file) {
        if (compression_ctx_initialized) {
            LZ4F_freeCompressionContext(lz4_file->compression_ctx);
        }

        if (lz4_file->buffer) {
            free(lz4_file->buffer);
        }

        if (lz4_file->compressed_buffer) {
            free(lz4_file->compressed_buffer);
        }

        free(lz4_file);
    }


    if (file && fd_owned) {
        fclose(file);
    }

    return NULL;
}

static int lz4_file_driver_close(void * file, int fd_owned)
{
    lz4_file_t * lz4_file = file;

    int status = 0;

    if (lz4_file_driver_flush(file) != 0) {
        status = EOF;

        goto end;
    }

    const size_t compressed_size = LZ4F_compressEnd(
        lz4_file->compression_ctx,
        lz4_file->compressed_buffer,
        lz4_file->compressed_buffer_capacity,
        NULL
    );

    if (LZ4F_isError(compressed_size)) {
        status = EOF;

        goto end;
    }

    if (
        fwrite(
            lz4_file->compressed_buffer,
            1,
            compressed_size,
            lz4_file->file
        ) != compressed_size
    ) {
        status = EOF;

        goto end;
    }

end:
    if (fd_owned) {
        int fclose_status = fclose(lz4_file->file);
        if (fclose_status != 0) {
            status = fclose_status;
        }
    }

    LZ4F_freeCompressionContext(lz4_file->compression_ctx);
    free(lz4_file->buffer);
    free(lz4_file->compressed_buffer);
    free(lz4_file);

    return status;
}

static int lz4_file_driver_flush(void * file)
{
    lz4_file_t * lz4_file = file;

    if (lz4_file_driver_flush_buffer(lz4_file, 1) != 0) {
        return EOF;
    }
    
    return fflush(lz4_file->file);
}

static int lz4_file_driver_print(void * file, const char * str)
{
    lz4_file_t * lz4_file = file;

    return lz4_file_driver_write_to_buffer(lz4_file, str, strlen(str));
}

static int lz4_file_driver_vprintf(void * file, const char * fmt, va_list ap)
{
    lz4_file_t * lz4_file = file;

    char * buf;
    int printed = vasprintf(&buf, fmt, ap);
    if (printed < 0) {
        return printed;
    }

    printed = lz4_file_driver_write_to_buffer(lz4_file, buf, strlen(buf));
    free(buf);

    return printed;
}

static int lz4_file_driver_write(void * file, const void * buf, size_t len)
{
    return lz4_file_driver_write_to_buffer(file, buf, len);
}

static int lz4_file_driver_write_to_buffer(lz4_file_t * lz4_file, const void * buf, size_t len)
{
    size_t written_byte_count = 0;

    while (len > 0) {
        if (
            lz4_file->buffer_capacity == lz4_file->buffer_size &&
            lz4_file_driver_flush_buffer(lz4_file, 0) != 0
        ) {
            return written_byte_count;
        }

        const size_t remaining_buffer_len = lz4_file->buffer_capacity - lz4_file->buffer_size;
        const size_t chunk_size = len > remaining_buffer_len ? remaining_buffer_len : len;
        
        memcpy(lz4_file->buffer + lz4_file->buffer_size, buf, chunk_size);
        
        len -= chunk_size;
        buf += chunk_size;
        lz4_file->buffer_size += chunk_size;
        written_byte_count += chunk_size;
    }

    return written_byte_count;
}

static int lz4_file_driver_flush_buffer(lz4_file_t * lz4_file, int flush_lz4_buffer)
{
    if (lz4_file->buffer_size > 0) {
        const size_t compressed_size = LZ4F_compressUpdate(
            lz4_file->compression_ctx,
            lz4_file->compressed_buffer,
            lz4_file->compressed_buffer_capacity,
            lz4_file->buffer,
            lz4_file->buffer_size,
            NULL
        );

        if (LZ4F_isError(compressed_size)) {
            return 1;
        }

        if (
            fwrite(
                lz4_file->compressed_buffer,
                1,
                compressed_size,
                lz4_file->file
            ) != compressed_size
        ) {
            return 1;
        }

        lz4_file->buffer_size = 0;
    }

    if (flush_lz4_buffer) {
        const size_t compressed_size = LZ4F_flush(
            lz4_file->compression_ctx,
            lz4_file->compressed_buffer,
            lz4_file->compressed_buffer_capacity,
            NULL
        );

        if (LZ4F_isError(compressed_size)) {
            return 1;
        }

        if (
            fwrite(
                lz4_file->compressed_buffer,
                1,
                compressed_size,
                lz4_file->file
            ) != compressed_size
        ) {
            return 1;
        }
    }

    return 0;
}

#endif
