PHP_ARG_ENABLE(SPX, whether to enable SPX extension,
[ --enable-spx   Enable SPX extension])

PHP_ARG_ENABLE(SPX-DEV, whether to enable SPX developer build flags,
[  --enable-spx-dev   Compile SPX with debugging symbols])

if test -z "$PHP_ZLIB_DIR"; then
PHP_ARG_WITH(zlib-dir, for ZLIB,
[  --with-zlib-dir[=DIR]   Set the path to ZLIB install prefix.], no)
fi

if test "$PHP_SPX" = "yes"; then
    if test "$PHP_THREAD_SAFETY" != "no" -a "$CONTINUOUS_INTEGRATION" != "true"
    then
        AC_MSG_ERROR([SPX does not work with ZTS PHP build])
    fi

    AC_DEFINE(HAVE_SPX, 1, [spx])

    CFLAGS="-Werror -Wall -O3 -pthread -std=gnu90"
    if test "$CONTINUOUS_INTEGRATION" = "true"
    then
        CFLAGS="$CFLAGS -DCONTINUOUS_INTEGRATION"
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
        for i in /usr/local /usr; do
            if test -f "$i/include/zlib/zlib.h"; then
                PHP_ZLIB_DIR="$i"
                PHP_ZLIB_INCDIR="$i/include/zlib"
            elif test -f "$i/include/zlib.h"; then
                PHP_ZLIB_DIR="$i"
                PHP_ZLIB_INCDIR="$i/include"
            fi
        done
    fi

    AC_MSG_CHECKING([for zlib location])
    if test "$PHP_ZLIB_DIR" != "no" && test "$PHP_ZLIB_DIR" != "yes"; then
        AC_MSG_RESULT([$PHP_ZLIB_DIR])
        PHP_ADD_LIBRARY_WITH_PATH(z, $PHP_ZLIB_DIR/$PHP_LIBDIR, SPX_SHARED_LIBADD)
        PHP_ADD_INCLUDE($PHP_ZLIB_INCDIR)
    else
        AC_MSG_ERROR([spx support requires ZLIB. Use --with-zlib-dir=<DIR> to specify the prefix where ZLIB headers and library are located])
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
