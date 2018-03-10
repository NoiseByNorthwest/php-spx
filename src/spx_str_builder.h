#ifndef SPX_STR_BUILDER_H_DEFINED
#define SPX_STR_BUILDER_H_DEFINED

typedef struct spx_str_builder_t spx_str_builder_t;

spx_str_builder_t * spx_str_builder_create(size_t capacity);
void spx_str_builder_destroy(spx_str_builder_t * str_builder);

void spx_str_builder_reset(spx_str_builder_t * str_builder);
size_t spx_str_builder_capacity(const spx_str_builder_t * str_builder);
size_t spx_str_builder_size(const spx_str_builder_t * str_builder);
size_t spx_str_builder_remaining(const spx_str_builder_t * str_builder);
const char * spx_str_builder_str(const spx_str_builder_t * str_builder);

size_t spx_str_builder_append_double(spx_str_builder_t * str_builder, double d, size_t nb_dec);
size_t spx_str_builder_append_long(spx_str_builder_t * str_builder, long l);
size_t spx_str_builder_append_str(spx_str_builder_t * str_builder, const char * str);
size_t spx_str_builder_append_char(spx_str_builder_t * str_builder, char c);

#endif /* SPX_STR_BUILDER_H_DEFINED */
