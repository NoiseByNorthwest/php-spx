#ifndef __unix__
#   error "Your platform is not supported"
#endif

#include <stdio.h>
#include <unistd.h>

#include "spx_thread.h"

static SPX_THREAD_TLS FILE * null_output;
static SPX_THREAD_TLS int null_output_initialized;

int spx_stdio_disable(int fd)
{
    if (!null_output_initialized) {
        null_output_initialized = 1;
        null_output = fopen("/dev/null", "w");
    }

    if (!null_output) {
        return -1;
    }

    int copy = dup(fd);
    if (copy == -1) {
        return -1;
    }

    if (dup2(fileno(null_output), fd) == -1) {
        close(copy);

        return -1;
    }

    return copy;
}

int spx_stdio_restore(int fd, int copy)
{
    if (dup2(copy, fd) == -1) {
        return -1;
    }

    close(copy);

    return fd;
}
