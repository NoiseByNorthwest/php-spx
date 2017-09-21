#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "main/php.h"
#include "Zend/zend_extensions.h"

/* linux 2.6+ */
#ifndef linux
#   error "Only Linux based OS are supported"
#endif

#ifndef __x86_64__
#   error "Only x86-64 architecture is supported"
#endif

#if ZEND_EXTENSION_API_NO < 220131226 || ZEND_EXTENSION_API_NO > 320160303
#   error "Only the following PHP versions are supported: 5.6 to 7.1"
#endif

#ifdef ZTS
#   error "ZTS is not yet supported"
#endif

#define PHP_SPX_VERSION "0.1"
#define PHP_SPX_EXTNAME "SPX"

extern zend_module_entry spx_module_entry;
