#ifndef SPX_RESOURCE_STATS_H_DEFINED
#define SPX_RESOURCE_STATS_H_DEFINED

void spx_resource_stats_init(void);

void spx_resource_stats_shutdown(void);

size_t spx_resource_stats_wall_time(void);
size_t spx_resource_stats_cpu_time(void);

void spx_resource_stats_io(size_t * in, size_t * out);

#endif /* SPX_RESOURCE_STATS_H_DEFINED */
