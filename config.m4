PHP_ARG_ENABLE(SPX, whether to enable SPX extension,
[ --enable-spx   Enable SPX extension])

PHP_ARG_ENABLE(SPX-DEV, whether to enable SPX developer build flags,
[  --enable-spx-dev   Compile SPX with debugging symbols],, no)

if test -z "$PHP_ZLIB_DIR"; then
PHP_ARG_WITH(zlib-dir, for ZLIB,
[  --with-zlib-dir[=DIR]   Set the path to ZLIB install prefix.], no)
fi

PHP_ARG_WITH(spx-assets-dir, for assets path,
[  --with-spx-assets-dir[=DIR]   Set the installation path of assets.], $prefix/share/misc/php-spx/assets)

if test "$PHP_SPX" = "yes"; then
    AC_DEFINE(HAVE_SPX, 1, [spx])
    AC_MSG_CHECKING([for assets directory])
    AC_MSG_RESULT([$PHP_SPX_ASSETS_DIR])
    AC_DEFINE_UNQUOTED([SPX_HTTP_UI_ASSETS_DIR], ["$PHP_SPX_ASSETS_DIR/web-ui"], [path of web-ui assets directory])
    PHP_SUBST([PHP_SPX_ASSETS_DIR])

    CFLAGS="$CFLAGS -Werror -Wall -O3 -pthread -std=gnu90"

    if test "$(uname -s 2>/dev/null)" = "Darwin"; then
        # see discussion here https://github.com/NoiseByNorthwest/php-spx/pull/270
        CFLAGS="$CFLAGS -Wno-typedef-redefinition"
    fi

    if test "$PHP_SPX_DEV" = "yes"; then
        CFLAGS="$CFLAGS -g"
    fi

    AC_MSG_CHECKING([for zlib header])
    if test "$PHP_ZLIB_DIR" != "no" && test "$PHP_ZLIB_DIR" != "yes"; then
        if test -f "$PHP_ZLIB_DIR/include/zlib/zlib.h"; then
            PHP_ZLIB_INCDIR="$PHP_ZLIB_DIR/include/zlib"
        elif test -f "$PHP_ZLIB_DIR/include/zlib.h"; then
            PHP_ZLIB_INCDIR="$PHP_ZLIB_DIR/include"
        else
            AC_MSG_ERROR([Can't find ZLIB headers under "$PHP_ZLIB_DIR"])
        fi
    else
        for i in /usr/local /usr /opt/local /opt/homebrew; do
            if test -f "$i/include/zlib/zlib.h"; then
                PHP_ZLIB_DIR="$i"
                PHP_ZLIB_INCDIR="$i/include/zlib"
                break
            elif test -f "$i/include/zlib.h"; then
                PHP_ZLIB_DIR="$i"
                PHP_ZLIB_INCDIR="$i/include"
                break
            fi
        done
    fi

    AC_MSG_CHECKING([for zlib binary])
    if test "$PHP_ZLIB_DIR" != "no" && test "$PHP_ZLIB_DIR" != "yes"; then
        AC_MSG_RESULT([$PHP_ZLIB_DIR])
        PHP_ADD_LIBRARY_WITH_PATH(z, $PHP_ZLIB_DIR/$PHP_LIBDIR, SPX_SHARED_LIBADD)
        PHP_ADD_INCLUDE($PHP_ZLIB_INCDIR)
    else
        AC_MSG_ERROR([SPX support requires ZLIB. Use --with-zlib-dir=<DIR> to specify it.])
    fi

    AC_MSG_CHECKING([for Zstandard include directory])
    zstd_found=no
    for i in /usr/local /usr /opt/local /opt/homebrew; do
        if test -f "$i/include/zstd.h"; then
            PHP_ZSTD_INCDIR="$i/include"

            PHP_ZSTD_LIBDIR="$i/lib"
            if ! test -f "$PHP_ZSTD_LIBDIR/libzstd.a"; then
                PHP_ZSTD_LIBDIR=""
            fi

            zstd_found=yes
            break
        fi
    done

    if test "$zstd_found" = "yes"; then
        AC_MSG_RESULT([$PHP_ZSTD_INCDIR])
        PHP_ADD_INCLUDE([$PHP_ZSTD_INCDIR])
        if test "$PHP_ZSTD_LIBDIR" != ""; then
            PHP_ADD_LIBRARY_WITH_PATH(zstd, [$PHP_ZSTD_LIBDIR], SPX_SHARED_LIBADD)
        else
            AC_MSG_NOTICE([Using static linking for Zstandard])
            LDFLAGS="$LDFLAGS -lzstd"
        fi

        CPPFLAGS="$CPPFLAGS -DHAVE_ZSTD=1"
    else
        AC_MSG_WARN([Zstandard not found — continuing without it])
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
