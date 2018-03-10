#include <stdio.h>
#include <stdlib.h>

#include "spx_hset.h"

#define HSET_BUCKET_SIZE 4

struct spx_hset_entry_t {
    void * value;
    int free;
};

typedef struct hset_bucket_t {
    spx_hset_entry_t entries[HSET_BUCKET_SIZE];
    struct hset_bucket_t * next;
} hset_bucket_t;

struct spx_hset_t {
    spx_hset_hash_func_t hash;
    spx_hset_cmp_func_t cmp;
    size_t size;
    hset_bucket_t * buckets;
};

static void bucket_init(hset_bucket_t * bucket)
{
    bucket->next = NULL;
    size_t i;
    for (i = 0; i < HSET_BUCKET_SIZE; i++) {
        bucket->entries[i].free = 1;
    }
}

static void bucket_release_chain(hset_bucket_t * bucket)
{
    if (!bucket->next) {
        return;
    }

    bucket_release_chain(bucket->next);
    free(bucket->next);
}

static spx_hset_entry_t * bucket_get_entry(
    hset_bucket_t * bucket,
    spx_hset_cmp_func_t cmp,
    void * value,
    int existing,
    int * new
) {
    size_t i;
    for (i = 0; i < HSET_BUCKET_SIZE; i++) {
        spx_hset_entry_t * entry = &bucket->entries[i];
        if (entry->free) {
            if (existing) {
                return NULL;
            }

            entry->free = 0;
            entry->value = value;
            if (new) {
                *new = 1;
            }

            return entry;
        }

        if (0 == cmp(value, entry->value)) {
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
        value,
        existing,
        new
    );
}

spx_hset_t * spx_hset_create(
    size_t size,
    spx_hset_hash_func_t hash,
    spx_hset_cmp_func_t cmp
) {
    spx_hset_t * hset = malloc(sizeof(*hset));
    if (!hset) {
        goto error;
    }

    hset->hash = hash;
    hset->cmp = cmp;
    hset->size = size;
    hset->buckets = malloc(hset->size * sizeof(*hset->buckets));
    if (!hset->buckets) {
        goto error;
    }

    size_t i;
    for (i = 0; i < hset->size; i++) {
        bucket_init(&hset->buckets[i]);
    }

    return hset;

error:
    free(hset);

    return NULL;
}

void spx_hset_reset(spx_hset_t * hset)
{
    size_t i;
    for (i = 0; i < hset->size; i++) {
        bucket_release_chain(&hset->buckets[i]);
        bucket_init(&hset->buckets[i]);
    }
}

void spx_hset_destroy(spx_hset_t * hset)
{
    size_t i;
    for (i = 0; i < hset->size; i++) {
        bucket_release_chain(&hset->buckets[i]);
    }

    free(hset->buckets);
    free(hset);
}

spx_hset_entry_t * spx_hset_get_entry(
    spx_hset_t * hset,
    void * value,
    int * new
) {
    return bucket_get_entry(
        &hset->buckets[hset->hash(value) % hset->size],
        hset->cmp,
        value,
        0,
        new
    );
}

spx_hset_entry_t * spx_hset_get_existing_entry(
    spx_hset_t * hset,
    void * value
) {
    return bucket_get_entry(
        &hset->buckets[hset->hash(value) % hset->size],
        hset->cmp,
        value,
        1,
        NULL
    );
}

int spx_hset_entry_set_value(
    const spx_hset_t * hset,
    spx_hset_entry_t * entry,
    void * value
) {
    if (0 != hset->cmp(entry->value, value)) {
        return 0;
    }

    entry->value = value;

    return 1;
}

void * spx_hset_entry_get_value(const spx_hset_entry_t * entry)
{
    return entry->value;
}
