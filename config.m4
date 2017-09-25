PHP_ARG_ENABLE(SPX, whether to enable SPX extension,
[ --enable-spx   Enable SPX extension])

if test "$PHP_SPX" = "yes"; then
    if test "$PHP_THREAD_SAFETY" != "no" -a "$CONTINUOUS_INTEGRATION" != "true"
    then
        AC_MSG_ERROR([SPX does not work with ZTS PHP build])
    fi

    AC_DEFINE(HAVE_SPX, 1, [spx])

    CFLAGS="-Werror -Wfatal-errors -Wall -O2 -g"
    if test "$CONTINUOUS_INTEGRATION" = "true"
    then
        CFLAGS="$CFLAGS -DCONTINUOUS_INTEGRATION"
    fi

    AC_MSG_CHECKING([for zlib header])
    if test -f "/usr/include/zlib.h"
    then
        AC_MSG_RESULT([yes])
    else
        AC_MSG_ERROR([Cannot find zlib.h])
    fi

    PHP_SUBST(CFLAGS)

    PHP_NEW_EXTENSION(spx,
        src/php_spx.c            \
        src/spx_profiler.c       \
        src/spx_reporter_fp.c    \
        src/spx_reporter_trace.c \
        src/spx_reporter_cg.c    \
        src/spx_reporter_gte.c   \
        src/spx_metric.c         \
        src/spx_resource_stats.c \
        src/spx_hset.c           \
        src/spx_output_stream.c  \
        src/spx_php.c            \
        src/spx_stdio.c          \
        src/spx_config.c         \
        src/spx_fmt.c,
        $ext_shared)
fi
