PHP_ARG_ENABLE(SPX, whether to enable SPX extension,
[ --enable-spx   Enable SPX extension])

if test "$PHP_SPX" = "yes"; then
    if test "$PHP_THREAD_SAFETY" != "no" -a "$CONTINUOUS_INTEGRATION" != "true"
    then
        AC_MSG_ERROR([SPX does not work with ZTS PHP build])
    fi

    AC_DEFINE(HAVE_SPX, 1, [spx])

    CFLAGS="-Werror -Wall -O3 -pthread"
    if test "$CONTINUOUS_INTEGRATION" = "true"
    then
        CFLAGS="$CFLAGS -DCONTINUOUS_INTEGRATION"
    fi

    AC_MSG_CHECKING([for zlib header])

    for dir in /usr/local /usr
    do
        for incdir in $dir/include/zlib $dir/include
        do
            if test -f "$incdir/zlib.h"
            then
                SPX_ZLIB_DIR="$dir"
                SPX_ZLIB_INCDIR="$incdir"

                break
            fi
        done

        if test "$SPX_ZLIB_INCDIR" != ""
        then
            break
        fi
    done

    if test "$SPX_ZLIB_INCDIR" != ""
    then
        AC_MSG_RESULT([yes])
        PHP_ADD_INCLUDE($SPX_ZLIB_INCDIR)
        PHP_ADD_LIBRARY_WITH_PATH(z, $SPX_ZLIB_DIR/$PHP_LIBDIR, SPX_SHARED_LIBADD)
    else
        AC_MSG_ERROR([Cannot find zlib.h])
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
