#ifndef SPX_OUTPUT_STREAM_H_DEFINED
#define SPX_OUTPUT_STREAM_H_DEFINED

typedef struct spx_output_stream_t spx_output_stream_t;

spx_output_stream_t * spx_output_stream_open(const char * file_name, int compressed);
spx_output_stream_t * spx_output_stream_dopen(int fileno, int compressed);

void spx_output_stream_close(spx_output_stream_t * output);

void spx_output_stream_print(spx_output_stream_t * output, const char * str);
void spx_output_stream_printf(spx_output_stream_t * output, const char * format, ...);

void spx_output_stream_flush(spx_output_stream_t * output);

#endif /* SPX_OUTPUT_STREAM_H_DEFINED */
