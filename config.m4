PHP_ARG_ENABLE(SPX, whether to enable SPX extension,
[ --enable-spx   Enable SPX extension])

if test "$PHP_SPX" = "yes"; then
    if test "$PHP_THREAD_SAFETY" != "no"; then
        AC_MSG_ERROR([SPX does not work with ZTS PHP build])
    fi

    AC_DEFINE(HAVE_SPX, 1, [spx])

    CFLAGS="-Werror -Wfatal-errors -Wall -O2 -g"

    AC_MSG_CHECKING([for zlib header])
    if test -f "/usr/include/zlib.h"; then
        AC_MSG_RESULT([yes])
    else
        AC_MSG_ERROR([Cannot find zlib.h])
    fi

    PHP_SUBST(CFLAGS)

    PHP_NEW_EXTENSION(spx,
        php_spx.c            \
        spx_profiler.c       \
        spx_reporter_fp.c    \
        spx_reporter_trace.c \
        spx_reporter_cg.c    \
        spx_reporter_gte.c   \
        spx_metric.c         \
        spx_resource_stats.c \
        spx_hset.c           \
        spx_output_stream.c  \
        spx_php.c            \
        spx_stdio.c          \
        spx_config.c         \
        spx_fmt.c,
        $ext_shared)
fi
