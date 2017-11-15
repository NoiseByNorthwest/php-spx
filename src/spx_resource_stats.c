#ifdef linux
#   include "spx_resource_stats-linux.c"
#elif defined(__APPLE__) && defined(__MACH__)
#   include "spx_resource_stats-macos.c"
#else
#   error "Your platform is not supported. Please open an issue."
#endif
