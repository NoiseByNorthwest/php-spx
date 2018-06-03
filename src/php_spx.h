#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "main/php.h"

/* linux 2.6+ or OSX */
#if !defined(linux) && !(defined(__APPLE__) && defined(__MACH__))
#   error "Only Linux-based OS or Apple MacOS are supported"
#endif

#ifndef __x86_64__
#   error "Only x86-64 architecture is supported"
#endif

#if ZEND_MODULE_API_NO < 20131226 || ZEND_MODULE_API_NO > 20170718
#   error "Only the following PHP versions are supported: 5.6 to 7.2"
#endif

#if defined(ZTS) && !defined(CONTINUOUS_INTEGRATION)
#   error "ZTS is not yet supported"
#endif

#define PHP_SPX_EXTNAME "SPX"
#define PHP_SPX_VERSION "0.3.0"

extern zend_module_entry spx_module_entry;
