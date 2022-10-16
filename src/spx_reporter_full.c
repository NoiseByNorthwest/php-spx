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
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef linux
#   include <sys/syscall.h>
#endif

#include "spx_reporter_full.h"
#include "spx_php.h"
#include "spx_output_stream.h"
#include "spx_str_builder.h"
#include "spx_utils.h"

#define BUFFER_CAPACITY 16384

typedef struct {
    size_t function_idx;
    int start;
    spx_profiler_metric_values_t metric_values;
} buffer_entry_t;

typedef struct {
    char * key;
    size_t exec_ts;
    char * hostname;
    pid_t process_pid;
    pid_t process_tid;
    char * process_pwd;
    int cli;
    char * cli_command_line;
    char * http_request_uri;
    char * http_method;
    char * http_host;
    char * custom_metadata_str;
    size_t wall_time_ms;
    size_t peak_memory_usage;
    size_t called_function_count;
    size_t call_count;
    size_t recorded_call_count;
    int enabled_metrics[SPX_METRIC_COUNT];
} metadata_t;

typedef struct {
    spx_profiler_reporter_t base;

    char metadata_file_name[512];
    metadata_t * metadata;
    spx_output_stream_t * output;

    size_t buffer_size;
    buffer_entry_t buffer[BUFFER_CAPACITY];

    spx_str_builder_t * str_builder;
} full_reporter_t;

static spx_profiler_reporter_cost_t full_notify(
    spx_profiler_reporter_t * reporter,
    const spx_profiler_event_t * event
);

static void full_destroy(spx_profiler_reporter_t * reporter);
static void flush_buffer(full_reporter_t * reporter, const int * enabled_metrics);
static void finalize(full_reporter_t * reporter, const spx_profiler_event_t * event);

static metadata_t * metadata_create(void);
static void metadata_destroy(metadata_t * metadata);
static int metadata_save(const metadata_t * metadata, const char * file_name);

size_t spx_reporter_full_metadata_list_files(
    const char * data_dir,
    void (*callback) (const char *, size_t)
) {
    DIR * dir = opendir(data_dir);
    if (!dir) {
        return 0;
    }

    char file_path[512];
    size_t count = 0;
    const struct dirent * entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!spx_utils_str_ends_with(entry->d_name, ".json")) {
            continue;
        }

        snprintf(
            file_path,
            sizeof(file_path),
            "%s/%s",
            data_dir,
            entry->d_name
        );

        callback(file_path, count);
        count++;
    }

    closedir(dir);

    return count;
}

int spx_reporter_full_build_metadata_file_name(
    const char * data_dir,
    const char * key,
    char * file_name,
    size_t size
) {
    return snprintf(
        file_name,
        size,
        "%s/%s.json",
        data_dir,
        key
    );
}

int spx_reporter_full_build_file_name(
    const char * data_dir,
    const char * key,
    char * file_name,
    size_t size
) {
    return snprintf(
        file_name,
        size,
        "%s/%s.txt.gz",
        data_dir,
        key
    );
}

spx_profiler_reporter_t * spx_reporter_full_create(const char * data_dir)
{
    full_reporter_t * reporter = malloc(sizeof(*reporter));
    if (!reporter) {
        return NULL;
    }

    reporter->base.notify = full_notify;
    reporter->base.destroy = full_destroy;

    reporter->metadata = NULL;
    reporter->output = NULL;
    reporter->str_builder = NULL;

    reporter->metadata = metadata_create();
    if (!reporter->metadata) {
        goto error;
    }

    char file_name[512];
    spx_reporter_full_build_file_name(
        data_dir,
        reporter->metadata->key,
        file_name,
        sizeof(file_name)
    );

    spx_reporter_full_build_metadata_file_name(
        data_dir,
        reporter->metadata->key,
        reporter->metadata_file_name,
        sizeof(reporter->metadata_file_name)
    );

    (void) mkdir(data_dir, 0777);
    reporter->output = spx_output_stream_open(file_name, 1);
    if (!reporter->output) {
        goto error;
    }

    reporter->str_builder = spx_str_builder_create(8 * 1024);
    if (!reporter->str_builder) {
        goto error;
    }

    reporter->buffer_size = 0;

    spx_output_stream_print(reporter->output, "[events]\n");

    return (spx_profiler_reporter_t *) reporter;

error:
    spx_profiler_reporter_destroy((spx_profiler_reporter_t *)reporter);

    return NULL;
}

void spx_reporter_full_set_custom_metadata_str(
    const spx_profiler_reporter_t * base_reporter,
    const char * custom_metadata_str
) {
    const full_reporter_t * reporter = (const full_reporter_t *) base_reporter;

    reporter->metadata->custom_metadata_str = strdup(custom_metadata_str);
}

const char * spx_reporter_full_get_key(const spx_profiler_reporter_t * base_reporter)
{
    const full_reporter_t * reporter = (const full_reporter_t *) base_reporter;

    return reporter->metadata->key;
}

static spx_profiler_reporter_cost_t full_notify(
    spx_profiler_reporter_t * base_reporter,
    const spx_profiler_event_t * event
) {
    full_reporter_t * reporter = (full_reporter_t *) base_reporter;

    if (event->type == SPX_PROFILER_EVENT_CALL_END) {
        reporter->metadata->call_count++;
    }

    if (event->type != SPX_PROFILER_EVENT_FINALIZE) {
        if (event->type == SPX_PROFILER_EVENT_CALL_END) {
            reporter->metadata->recorded_call_count++;
        }

        buffer_entry_t * current = &reporter->buffer[reporter->buffer_size];

        current->function_idx  = event->callee->idx;
        current->start         = event->type == SPX_PROFILER_EVENT_CALL_START;
        current->metric_values = *event->cum;

        reporter->buffer_size++;

        if (reporter->buffer_size < BUFFER_CAPACITY) {
            return SPX_PROFILER_REPORTER_COST_LIGHT;
        }
    }

    flush_buffer(reporter, event->enabled_metrics);

    if (event->type == SPX_PROFILER_EVENT_FINALIZE) {
        finalize(reporter, event);
    }

    return SPX_PROFILER_REPORTER_COST_HEAVY;
}

static void full_destroy(spx_profiler_reporter_t * base_reporter)
{
    full_reporter_t * reporter = (full_reporter_t *) base_reporter;

    if (reporter->metadata) {
        metadata_destroy(reporter->metadata);
    }

    if (reporter->output) {
        spx_output_stream_close(reporter->output);
    }

    if (reporter->str_builder) {
        spx_str_builder_destroy(reporter->str_builder);
    }
}

static void flush_buffer(full_reporter_t * reporter, const int * enabled_metrics)
{
    spx_str_builder_reset(reporter->str_builder);

    size_t i;
    for (i = 0; i < reporter->buffer_size; i++) {
        const buffer_entry_t * current = &reporter->buffer[i];

        spx_str_builder_append_long(reporter->str_builder, current->function_idx);
        spx_str_builder_append_char(reporter->str_builder, ' ');
        spx_str_builder_append_char(reporter->str_builder, current->start ? '1' : '0');

        SPX_METRIC_FOREACH(i, {
            if (!enabled_metrics[i]) {
                continue;
            }

            spx_str_builder_append_char(reporter->str_builder, ' ');
            spx_str_builder_append_double(reporter->str_builder, current->metric_values.values[i], 4);
        });

        spx_str_builder_append_str(reporter->str_builder, "\n");

        if (spx_str_builder_remaining(reporter->str_builder) < 128) {
            spx_output_stream_print(reporter->output, spx_str_builder_str(reporter->str_builder));
            spx_str_builder_reset(reporter->str_builder);
        }
    }

    if (spx_str_builder_size(reporter->str_builder) > 0) {
        spx_output_stream_print(reporter->output, spx_str_builder_str(reporter->str_builder));
    }

    reporter->buffer_size = 0;
}

static void finalize(full_reporter_t * reporter, const spx_profiler_event_t * event)
{
    spx_output_stream_print(reporter->output, "[functions]\n");

    size_t i;
    for (i = 0; i < event->func_table.size; i++) {
        const spx_profiler_func_table_entry_t * entry = &event->func_table.entries[i];

        spx_output_stream_printf(
            reporter->output,
            "%s%s%s\n",
            entry->function.class_name,
            entry->function.class_name[0] ? "::" : "",
            entry->function.func_name
        );
    }

    reporter->metadata->peak_memory_usage = spx_php_zend_memory_usage();
    reporter->metadata->wall_time_ms = event->cum->values[SPX_METRIC_WALL_TIME] / 1000;

    reporter->metadata->called_function_count = event->func_table.size;
    SPX_METRIC_FOREACH(i, {
        reporter->metadata->enabled_metrics[i] = event->enabled_metrics[i];
    });

    metadata_save(reporter->metadata, reporter->metadata_file_name);
}

static metadata_t * metadata_create(void)
{
    metadata_t * metadata = malloc(sizeof(*metadata));
    if (!metadata) {
        return NULL;
    }

    metadata->key = NULL;
    metadata->hostname = NULL;
    metadata->process_pwd = NULL;

    metadata->cli_command_line = NULL;
    metadata->http_request_uri = NULL;
    metadata->http_method = NULL;
    metadata->http_host = NULL;
    metadata->custom_metadata_str = NULL;

    metadata->exec_ts = time(NULL);

    char hostname[256];
    if (0 == gethostname(hostname, sizeof(hostname))) {
        hostname[sizeof(hostname) - 1] = 0;
        metadata->hostname = strdup(hostname);
    } else {
        metadata->hostname = strdup("n/a");
    }

    if (!metadata->hostname) {
        goto error;
    }

    metadata->process_pid = getpid();
#ifdef linux
    metadata->process_tid = syscall(SYS_gettid);
#else
    metadata->process_tid = 0;
#endif

    time_t timer;
    time(&timer);

    char date[32];
    strftime(
        date,
        sizeof(date),
        "%Y%m%d_%H%M%S",
        localtime(&timer)
    );

    char key[512];
    snprintf(
        key,
        sizeof(key),
        "spx-full-%s-%s-%d-%d",
        date,
        metadata->hostname,
        metadata->process_pid,
        rand()
    );

    metadata->key = strdup(key);
    if (!metadata->key) {
        goto error;
    }

    char pwd[8 * 1024];
    metadata->process_pwd = strdup(getcwd(pwd, sizeof(pwd)) ? pwd : "n/a");
    if (!metadata->process_pwd) {
        goto error;
    }

    metadata->cli = spx_php_is_cli_sapi();
    metadata->cli_command_line = spx_php_build_command_line();
    if (!metadata->cli_command_line) {
        metadata->cli_command_line = strdup("n/a");
    }

    if (!metadata->cli_command_line) {
        goto error;
    }

    const char * http_request_uri = spx_php_global_array_get("_SERVER", "REQUEST_URI");
    metadata->http_request_uri = strdup(http_request_uri ? http_request_uri : "n/a");
    if (!metadata->http_request_uri) {
        goto error;
    }

    const char * http_method = spx_php_global_array_get("_SERVER", "REQUEST_METHOD");
    metadata->http_method = strdup(http_method ? http_method : "n/a");
    if (!metadata->http_method) {
        goto error;
    }

    const char * http_host = spx_php_global_array_get("_SERVER", "HTTP_HOST");
    metadata->http_host = strdup(http_host ? http_host : "n/a");
    if (!metadata->http_host) {
        goto error;
    }

    metadata->call_count = 0;
    metadata->recorded_call_count = 0;

    return metadata;

error:
    metadata_destroy(metadata);

    return NULL;
}

static void metadata_destroy(metadata_t * metadata)
{
    free(metadata->key);
    free(metadata->hostname);
    free(metadata->process_pwd);

    free(metadata->cli_command_line);
    free(metadata->http_request_uri);
    free(metadata->http_method);
    free(metadata->http_host);
    free(metadata->custom_metadata_str);

    free(metadata);
}

static int metadata_save(const metadata_t * metadata, const char * file_name)
{
    FILE * fp = fopen(file_name, "w");
    if (!fp) {
        return -1;
    }

    char buf[8 * 1024];

    fprintf(fp, "{\n");

    fprintf(
        fp,
        "  \"%s\": \"%s\",\n",
        "key",
        spx_utils_json_escape(buf, metadata->key, sizeof(buf))
    );

    fprintf(
        fp,
        "  \"%s\": %lu,\n",
        "exec_ts",
        metadata->exec_ts
    );

    fprintf(
        fp,
        "  \"%s\": \"%s\",\n",
        "host_name",
        spx_utils_json_escape(buf, metadata->hostname, sizeof(buf))
    );

    fprintf(
        fp,
        "  \"%s\": %d,\n",
        "process_pid",
        metadata->process_pid
    );

    fprintf(
        fp,
        "  \"%s\": %d,\n",
        "process_tid",
        metadata->process_tid
    );

    fprintf(
        fp,
        "  \"%s\": \"%s\",\n",
        "process_pwd",
        spx_utils_json_escape(buf, metadata->process_pwd, sizeof(buf))
    );

    fprintf(
        fp,
        "  \"%s\": %d,\n",
        "cli",
        metadata->cli
    );

    fprintf(
        fp,
        "  \"%s\": \"%s\",\n",
        "cli_command_line",
        spx_utils_json_escape(buf, metadata->cli_command_line, sizeof(buf))
    );

    fprintf(
        fp,
        "  \"%s\": \"%s\",\n",
        "http_request_uri",
        spx_utils_json_escape(buf, metadata->http_request_uri, sizeof(buf))
    );

    fprintf(
        fp,
        "  \"%s\": \"%s\",\n",
        "http_method",
        spx_utils_json_escape(buf, metadata->http_method, sizeof(buf))
    );

    fprintf(
        fp,
        "  \"%s\": \"%s\",\n",
        "http_host",
        spx_utils_json_escape(buf, metadata->http_host, sizeof(buf))
    );

    if (metadata->custom_metadata_str) {
        fprintf(
            fp,
            "  \"%s\": \"%s\",\n",
            "custom_metadata_str",
            spx_utils_json_escape(buf, metadata->custom_metadata_str, sizeof(buf))
        );
    } else {
        fprintf(
            fp,
            "  \"%s\": null,\n",
            "custom_metadata_str"
        );
    }

    fprintf(
        fp,
        "  \"%s\": %lu,\n",
        "wall_time_ms",
        metadata->wall_time_ms
    );

    fprintf(
        fp,
        "  \"%s\": %lu,\n",
        "peak_memory_usage",
        metadata->peak_memory_usage
    );

    fprintf(
        fp,
        "  \"%s\": %lu,\n",
        "called_function_count",
        metadata->called_function_count
    );

    fprintf(
        fp,
        "  \"%s\": %lu,\n",
        "call_count",
        metadata->call_count
    );

    fprintf(
        fp,
        "  \"%s\": %lu,\n",
        "recorded_call_count",
        metadata->recorded_call_count
    );

    fprintf(fp, "  \"enabled_metrics\": [\n");

    int first = 1;
    SPX_METRIC_FOREACH(i, {
        if (!metadata->enabled_metrics[i]) {
            continue;
        }

        fprintf(fp, "    ");

        if (!first) {
            fprintf(fp, ",");
        } else {
            first = 0;
        }

        fprintf(
            fp,
            "\"%s\"\n",
            spx_metric_info[i].key
        );
    });

    fprintf(fp, "  ]\n}\n");
    
    fclose(fp);

    return 0;
}
