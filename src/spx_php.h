/* SPX - A simple profiler for PHP
 * Copyright (C) 2017-2022 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
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


#ifndef SPX_PHP_H_DEFINED
#define SPX_PHP_H_DEFINED

/*
	TSRMLS_* macros, which were deprecated since PHP7, are removed in PHP8.
	More details here:
	https://github.com/php/php-src/blob/PHP-8.0/UPGRADING.INTERNALS#L50
*/
#if PHP_API_VERSION >= 20200930
#define TSRMLS_CC
#define TSRMLS_C
#define TSRMLS_DC
#define TSRMLS_D
#define TSRMLS_FETCH()
#endif

typedef struct {
    unsigned long hash_code;

    const char * func_name;
    const char * class_name;
} spx_php_function_t;

int spx_php_is_cli_sapi(void);

void spx_php_current_function(spx_php_function_t * function);

const char * spx_php_ini_get_string(const char * name);
double spx_php_ini_get_double(const char * name);
const char * spx_php_global_array_get(const char * name, const char * key);
char * spx_php_build_command_line(void);

size_t spx_php_zend_memory_usage(void);
size_t spx_php_zend_memory_alloc_count(void);
size_t spx_php_zend_memory_alloc_bytes(void);
size_t spx_php_zend_memory_free_count(void);
size_t spx_php_zend_memory_free_bytes(void);
size_t spx_php_zend_gc_run_count(void);
size_t spx_php_zend_gc_root_buffer_length(void);
size_t spx_php_zend_gc_collected_count(void);
size_t spx_php_zend_included_file_count(void);
size_t spx_php_zend_included_line_count(void);
size_t spx_php_zend_class_count(void);
size_t spx_php_zend_function_count(void);
size_t spx_php_zend_opcode_count(void);
size_t spx_php_zend_object_count(void);
size_t spx_php_zend_error_count(void);

void spx_php_global_hooks_set(void);
void spx_php_global_hooks_unset(void);
void spx_php_global_hooks_disable(void);

void spx_php_execution_init(void);
void spx_php_execution_shutdown(void);

void spx_php_execution_disable(void);
void spx_php_execution_hook(void (*before)(void), void (*after)(void), int internal);
void spx_php_execution_finalize(void);

void spx_php_output_add_header_line(const char * header_line);
void spx_php_output_add_header_linef(const char * fmt, ...);
void spx_php_output_send_headers(void);

size_t spx_php_output_direct_write(const void * ptr, size_t len);
size_t spx_php_output_direct_print(const char * str);
int spx_php_output_direct_printf(const char * fmt, ...);

void spx_php_log_notice(const char * fmt, ...);

#endif /* SPX_PHP_H_DEFINED */
