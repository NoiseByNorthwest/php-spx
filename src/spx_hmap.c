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


#include <stdio.h>
#include <stdlib.h>

#include "spx_hmap.h"

#define HSET_BUCKET_SIZE 4

struct spx_hmap_entry_t {
    const void * key;
    void * value;
    int free;
};

typedef struct hmap_bucket_t {
    spx_hmap_entry_t entries[HSET_BUCKET_SIZE];
    struct hmap_bucket_t * next;
} hmap_bucket_t;

struct spx_hmap_t {
    spx_hmap_hash_key_func_t hash;
    spx_hmap_cmp_key_func_t cmp;
    size_t size;
    hmap_bucket_t * buckets;
};

static void bucket_init(hmap_bucket_t * bucket)
{
    bucket->next = NULL;
    size_t i;
    for (i = 0; i < HSET_BUCKET_SIZE; i++) {
        bucket->entries[i].free = 1;
    }
}

static void bucket_release_chain(hmap_bucket_t * bucket)
{
    if (!bucket->next) {
        return;
    }

    bucket_release_chain(bucket->next);
    free(bucket->next);
}

static spx_hmap_entry_t * bucket_get_entry(
    hmap_bucket_t * bucket,
    spx_hmap_cmp_key_func_t cmp,
    const void * key,
    int existing,
    int * new
) {
    size_t i;
    for (i = 0; i < HSET_BUCKET_SIZE; i++) {
        spx_hmap_entry_t * entry = &bucket->entries[i];
        if (entry->free) {
            if (existing) {
                return NULL;
            }

            entry->free = 0;
            entry->key = key;
            if (new) {
                *new = 1;
            }

            return entry;
        }

        if (0 == cmp(key, entry->key)) {
            if (new) {
                *new = 0;
            }

            return entry;
        }
    }

    if (!bucket->next) {
        if (existing) {
            return NULL;
        }

        bucket->next = malloc(sizeof(*bucket->next));
        if (!bucket->next) {
            return NULL;
        }

        bucket_init(bucket->next);
    }

    return bucket_get_entry(
        bucket->next,
        cmp,
        key,
        existing,
        new
    );
}

spx_hmap_t * spx_hmap_create(
    size_t size,
    spx_hmap_hash_key_func_t hash,
    spx_hmap_cmp_key_func_t cmp
) {
    spx_hmap_t * hmap = malloc(sizeof(*hmap));
    if (!hmap) {
        goto error;
    }

    hmap->hash = hash;
    hmap->cmp = cmp;
    hmap->size = size;
    hmap->buckets = malloc(hmap->size * sizeof(*hmap->buckets));
    if (!hmap->buckets) {
        goto error;
    }

    size_t i;
    for (i = 0; i < hmap->size; i++) {
        bucket_init(&hmap->buckets[i]);
    }

    return hmap;

error:
    free(hmap);

    return NULL;
}

void spx_hmap_reset(spx_hmap_t * hmap)
{
    size_t i;
    for (i = 0; i < hmap->size; i++) {
        bucket_release_chain(&hmap->buckets[i]);
        bucket_init(&hmap->buckets[i]);
    }
}

void spx_hmap_destroy(spx_hmap_t * hmap)
{
    size_t i;
    for (i = 0; i < hmap->size; i++) {
        bucket_release_chain(&hmap->buckets[i]);
    }

    free(hmap->buckets);
    free(hmap);
}

spx_hmap_entry_t * spx_hmap_ensure_entry(spx_hmap_t * hmap, const void * key, int * new) {
    return bucket_get_entry(
        &hmap->buckets[hmap->hash(key) % hmap->size],
        hmap->cmp,
        key,
        0,
        new
    );
}

void * spx_hmap_get_value(spx_hmap_t * hmap, const void * key)
{
    const spx_hmap_entry_t * entry = bucket_get_entry(
        &hmap->buckets[hmap->hash(key) % hmap->size],
        hmap->cmp,
        key,
        1,
        NULL
    );

    if (!entry) {
        return NULL;
    }

    return entry->value;
}

int spx_hmap_set_entry_key(spx_hmap_t * hmap, spx_hmap_entry_t * entry, const void * key)
{
    if (0 != hmap->cmp(entry->key, key)) {
        return 0;
    }

    entry->key = key;

    return 1;
}

void spx_hmap_entry_set_value(spx_hmap_entry_t * entry, void * value)
{
    entry->value = value;
}

void * spx_hmap_entry_get_value(const spx_hmap_entry_t * entry)
{
    return entry->value;
}
