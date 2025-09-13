/* SPX - A simple profiler for PHP
 * Copyright (C) 2017-2025 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
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

#include <stdint.h>

#include "main/php.h"


#define SPX_PHP_STACK_CAPACITY 16384

typedef struct {
    uint64_t hash_code;

    const char * func_name;
    const char * class_name;
    const char * file_name;
    uint32_t line;
    uint16_t depth;
} spx_php_function_t;

int spx_php_is_cli_sapi(void);
int spx_php_are_ansi_sequences_supported(void);

void spx_php_print_stack(void);
size_t spx_php_current_depth(void);
void spx_php_current_function(spx_php_function_t * function);
int spx_php_previous_function(const spx_php_function_t * current, spx_php_function_t * previous);
void spx_php_function_at(size_t depth, spx_php_function_t * function);
uint8_t spx_php_is_internal_function(const spx_php_function_t * function);
size_t spx_php_function_call_site_line(const spx_php_function_t * function);

const char * spx_php_ini_get_string(const char * name);
double spx_php_ini_get_double(const char * name);
const char * spx_php_global_array_get(const char * name, const char * key);
char * spx_php_build_command_line(void);

size_t spx_php_zend_memory_usage(void);
size_t spx_php_zend_memory_peak_usage(void);
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

void spx_php_global_hooks_init(void);
void spx_php_global_hooks_set(int use_observer_api);
void spx_php_global_hooks_unset(void);
void spx_php_global_hooks_disable(void);

void spx_php_execution_init(int use_observer_api);
void spx_php_execution_shutdown(void);

void spx_php_execution_disable(void);
void spx_php_execution_hook(void (*before)(void), void (*after)(void), int internal_functions);
int spx_php_execution_hook_are_internal_functions_traced();
void spx_php_execution_finalize(void);

void spx_php_output_add_header_line(const char * header_line);
void spx_php_output_add_header_linef(const char * fmt, ...);
void spx_php_output_send_headers(void);

size_t spx_php_output_direct_write(const void * ptr, size_t len);
size_t spx_php_output_direct_print(const char * str);
int spx_php_output_direct_printf(const char * fmt, ...);

void spx_php_log_notice(const char * fmt, ...);

#endif /* SPX_PHP_H_DEFINED */
