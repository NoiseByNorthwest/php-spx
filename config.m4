PHP_ARG_ENABLE(SPX, whether to enable SPX extension,
[ --enable-spx   Enable SPX extension])

PHP_ARG_ENABLE(SPX-DEV, whether to enable SPX developer build flags,
[  --enable-spx-dev   Compile SPX with debugging symbols])

if test -z "$PHP_ZLIB_DIR"; then
PHP_ARG_WITH(zlib-dir, for ZLIB,
[  --with-zlib-dir[=DIR]   Set the path to ZLIB install prefix.], no)
fi

PHP_ARG_WITH(spx-assets-dir, for assets path,
[  --with-spx-assets-dir[=DIR]   Set the installation path of assets.], $prefix/share/misc/php-spx/assets)

if test "$PHP_SPX" = "yes"; then
    AC_DEFINE(HAVE_SPX, 1, [spx])
    AC_MSG_CHECKING([for assets directory])
    AC_MSG_RESULT([ $PHP_SPX_ASSETS_DIR ])
    AC_DEFINE_UNQUOTED([SPX_HTTP_UI_ASSETS_DIR], [ "$PHP_SPX_ASSETS_DIR/web-ui" ], [path of web-ui assets directory])
    PHP_SUBST([PHP_SPX_ASSETS_DIR])

    CFLAGS="-Werror -Wall -O3 -pthread -std=gnu90"

    if test "$(uname -s 2>/dev/null)" = "Darwin"
    then
        # see discussion here https://github.com/NoiseByNorthwest/php-spx/pull/270
        CFLAGS="$CFLAGS -Wno-typedef-redefinition"
    fi

    if test "$PHP_SPX_DEV" = "yes"
    then
        CFLAGS="$CFLAGS -g"
    fi

    AC_MSG_CHECKING([for zlib header])
    if test "$PHP_ZLIB_DIR" != "no" && test "$PHP_ZLIB_DIR" != "yes"; then
        if test -f "$PHP_ZLIB_DIR/include/zlib/zlib.h"; then
            PHP_ZLIB_DIR="$PHP_ZLIB_DIR"
            PHP_ZLIB_INCDIR="$PHP_ZLIB_DIR/include/zlib"
        elif test -f "$PHP_ZLIB_DIR/include/zlib.h"; then
            PHP_ZLIB_DIR="$PHP_ZLIB_DIR"
            PHP_ZLIB_INCDIR="$PHP_ZLIB_DIR/include"
        else
            AC_MSG_ERROR([Can't find ZLIB headers under "$PHP_ZLIB_DIR"])
        fi
    else
        for i in /usr/local /usr /opt/local; do
            if test -f "$i/include/zlib/zlib.h"; then
                PHP_ZLIB_DIR="$i"
                PHP_ZLIB_INCDIR="$i/include/zlib"
            elif test -f "$i/include/zlib.h"; then
                PHP_ZLIB_DIR="$i"
                PHP_ZLIB_INCDIR="$i/include"
            fi
        done
    fi

    AC_MSG_CHECKING([for zlib binary])
    if test "$PHP_ZLIB_DIR" != "no" && test "$PHP_ZLIB_DIR" != "yes"; then
        AC_MSG_RESULT([$PHP_ZLIB_DIR])
        PHP_ADD_LIBRARY_WITH_PATH(z, $PHP_ZLIB_DIR/$PHP_LIBDIR, SPX_SHARED_LIBADD)
        PHP_ADD_INCLUDE($PHP_ZLIB_INCDIR)
    else
        AC_MSG_ERROR([spx support requires ZLIB. Use --with-zlib-dir=<DIR> to specify the prefix where ZLIB headers and library are located])
    fi

    AC_MSG_CHECKING([for Zstandard include file])
    zstd_header_file=$(find /usr/ -type f | grep '/zstd.h$')
    if test "$zstd_header_file" == ""; then
        AC_MSG_WARN([Zstandard header not found])
    else
        AC_MSG_RESULT([$zstd_header_file])
    fi

    AC_MSG_CHECKING([for Zstandard binary])
    zstd_binary_file=$(find /usr/ -type f | grep '/libzstd.a$')
    if test -z "$zstd_binary_file"; then
        AC_MSG_ERROR([Zstandard binary not found])
    else
        AC_MSG_RESULT([$zstd_binary_file])
        LDFLAGS=-lzstd
    fi

    PHP_NEW_EXTENSION(spx,
        src/php_spx.c               \
        src/spx_profiler.c          \
        src/spx_profiler_tracer.c   \
        src/spx_profiler_sampler.c  \
        src/spx_reporter_full.c     \
        src/spx_reporter_fp.c       \
        src/spx_reporter_trace.c    \
        src/spx_metric.c            \
        src/spx_resource_stats.c    \
        src/spx_hmap.c              \
        src/spx_str_builder.c       \
        src/spx_output_stream.c     \
        src/spx_php.c               \
        src/spx_stdio.c             \
        src/spx_config.c            \
        src/spx_utils.c             \
        src/spx_fmt.c,
        $ext_shared)

    PHP_ADD_MAKEFILE_FRAGMENT
fi
