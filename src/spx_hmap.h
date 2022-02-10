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


#ifndef SPX_HMAP_H_DEFINED
#define SPX_HMAP_H_DEFINED

typedef struct spx_hmap_t spx_hmap_t;
typedef struct spx_hmap_entry_t spx_hmap_entry_t;

typedef unsigned long (*spx_hmap_hash_key_func_t) (const void *);
typedef int (*spx_hmap_cmp_key_func_t) (const void *, const void *);

spx_hmap_t * spx_hmap_create(
    size_t size,
    spx_hmap_hash_key_func_t hash,
    spx_hmap_cmp_key_func_t cmp
);

void spx_hmap_reset(spx_hmap_t * hmap);
void spx_hmap_destroy(spx_hmap_t * hmap);

spx_hmap_entry_t * spx_hmap_ensure_entry(spx_hmap_t * hmap, const void * key, int * new);
void * spx_hmap_get_value(spx_hmap_t * hmap, const void * key);
int spx_hmap_set_entry_key(spx_hmap_t * hmap, spx_hmap_entry_t * entry, const void * key);

void spx_hmap_entry_set_value(spx_hmap_entry_t * entry, void * value);
void * spx_hmap_entry_get_value(const spx_hmap_entry_t * entry);

#endif /* SPX_HMAP_H_DEFINED */
