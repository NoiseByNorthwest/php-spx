#include <time.h>
#include <sys/time.h>

#if defined(__APPLE__) && defined(__MACH__) && (__MAC_OS_X_VERSION_MIN_REQUIRED < 101200)
int clock_gettime(int clk_id, struct timespec *res)
{
  struct timeval tv;
  int ret = 0;
  ret = gettimeofday(&tv, NULL);
  if (ret == 0) {
    res->tv_sec = tv.tv_sec;
    res->tv_nsec = tv.tv_usec * 1000;
  }
  return ret;
}
#endif
