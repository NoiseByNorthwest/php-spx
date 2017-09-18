#ifndef SPX_HSET_H_DEFINED
#define SPX_HSET_H_DEFINED

typedef struct spx_hset_t spx_hset_t;
typedef struct spx_hset_entry_t spx_hset_entry_t;

typedef unsigned long (*spx_hset_hash_func_t) (const void *);
typedef int (*spx_hset_cmp_func_t) (const void *, const void *);

spx_hset_t * spx_hset_create(
    size_t size,
    spx_hset_hash_func_t hash,
    spx_hset_cmp_func_t cmp
);

void spx_hset_destroy(spx_hset_t * hset);

spx_hset_entry_t * spx_hset_get_entry(
    spx_hset_t * hset,
    void * value,
    int * new
);

spx_hset_entry_t * spx_hset_get_existing_entry(
    spx_hset_t * hset,
    void * value
);

int spx_hset_entry_set_value(
    const spx_hset_t * hset,
    spx_hset_entry_t * entry,
    void * value
);

void * spx_hset_entry_get_value(const spx_hset_entry_t * hset);

#endif /* SPX_HSET_H_DEFINED */
