PHP_ARG_ENABLE(spx, whether to enable SPX extension,
[ --enable-spx   Enable SPX extension])

PHP_ARG_ENABLE(spx-dev, whether to enable SPX developer build flags,
[  --enable-spx-dev   Compile SPX with debugging symbols],, no)

if test -z "$PHP_ZLIB_DIR"; then
PHP_ARG_WITH(zlib-dir, for ZLIB,
[  --with-zlib-dir[=DIR]   Set the path to ZLIB install prefix.], no, no)
fi

PHP_ARG_WITH(spx-zstd, for Zstandard support,
[  --with-spx-zstd[=DIR]   Set the path to Zstandard install prefix, or disable it with --without-spx-zstd.], yes, no)

PHP_ARG_WITH(spx-assets-dir, for assets path,
[  --with-spx-assets-dir[=DIR]   Set the installation path of assets.], $prefix/share/misc/php-spx/assets, no)

PHP_ARG_ENABLE(spx-symlink-assets-dir, whether to symlink the SPX assets directory,
[  --enable-spx-symlink-assets-dir   Symlink SPX assets directory to the source folder during installation], no, no)

if test "$PHP_SPX" = "yes"; then
    AC_DEFINE(HAVE_SPX, 1, [spx])

    if test "$PHP_SPX_ASSETS_DIR" = "yes" || test "$PHP_SPX_ASSETS_DIR" = "no"; then
        AC_MSG_ERROR([--with-spx-assets-dir requires a directory (--with-spx-assets-dir=DIR)])
    fi

    AC_MSG_CHECKING([for assets directory])
    AC_MSG_RESULT([$PHP_SPX_ASSETS_DIR])
    AC_DEFINE_UNQUOTED([SPX_HTTP_UI_ASSETS_DIR], ["$PHP_SPX_ASSETS_DIR/web-ui"], [path of web-ui assets directory])
    PHP_SUBST([PHP_SPX_ASSETS_DIR])
    PHP_SUBST([PHP_SPX_SYMLINK_ASSETS_DIR])

    CFLAGS="$CFLAGS -Werror -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-attributes -O3 -pthread"

    if test "$(uname -s)" = "Linux"; then
        CFLAGS="$CFLAGS -Wextra"
    fi

    if test "$CI" != "true"; then
        CFLAGS="$CFLAGS -march=native"
        AC_MSG_NOTICE([Enabling -march=native (not in CI)])
    else
        AC_MSG_NOTICE([Skipping -march=native in CI])
    fi

    php_ver_num=$(echo $PHP_VERSION | awk -F. '{printf("%d%02d%02d",$1,$2,$3)}')
    if test "$php_ver_num" -ge 80200; then
        CFLAGS="$CFLAGS -std=c11"
        AC_MSG_NOTICE([Adding -std=c11 because PHP >= 8.2])
    else
        CFLAGS="$CFLAGS -std=gnu90"
        AC_MSG_NOTICE([Adding -std=gnu90 because PHP < 8.2])
    fi

    # Disabling typedef-redefinition is required for:
    #   - macOS, see https://github.com/NoiseByNorthwest/php-spx/pull/270
    #   - clang, see https://github.com/NoiseByNorthwest/php-spx/pull/305
    # Furthermore, typedef-redefinition is also allowed with c11 and above.
    CFLAGS="$CFLAGS -Wno-typedef-redefinition"

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

    if test -n "$PHP_ZLIB_INCDIR"; then
        AC_MSG_RESULT([$PHP_ZLIB_INCDIR])
    else
        AC_MSG_RESULT([not found])
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
    if test "$PHP_SPX_ZSTD" = "no"; then
        zstd_search_dirs=""
    elif test "$PHP_SPX_ZSTD" = "yes"; then
        zstd_search_dirs="/usr/local /usr /opt/local /opt/homebrew"
    else
        zstd_search_dirs="$PHP_SPX_ZSTD"
    fi

    for i in $zstd_search_dirs; do
        if test -f "$i/include/zstd.h"; then
            PHP_ZSTD_INCDIR="$i/include"
            PHP_ZSTD_LIBDIR="$i/$PHP_LIBDIR"
            zstd_found=yes
            break
        fi
    done

    if test "$zstd_found" = "yes"; then
        AC_MSG_RESULT([$PHP_ZSTD_INCDIR])
        PHP_ADD_INCLUDE([$PHP_ZSTD_INCDIR])
        PHP_ADD_LIBRARY_WITH_PATH(zstd, [$PHP_ZSTD_LIBDIR], SPX_SHARED_LIBADD)

        CPPFLAGS="$CPPFLAGS -DHAVE_ZSTD=1"
    elif test "$PHP_SPX_ZSTD" = "no"; then
        AC_MSG_RESULT([disabled])
    elif test "$PHP_SPX_ZSTD" != "yes"; then
        AC_MSG_ERROR([Can't find Zstandard headers under "$PHP_SPX_ZSTD"])
    else
        AC_MSG_RESULT([not found])
        AC_MSG_WARN([Zstandard not found, continuing without it])
    fi

    PHP_SUBST([SPX_SHARED_LIBADD])

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
