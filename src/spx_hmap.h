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
