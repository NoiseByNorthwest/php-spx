#ifndef SPX_THREAD_H_DEFINED
#define SPX_THREAD_H_DEFINED

#ifdef ZTS
#   if !defined(__CYGWIN__) && defined(WIN32)
#       define SPX_THREAD_TLS __declspec(thread)
#   else
#       define SPX_THREAD_TLS __thread
#   endif
#else
#   define SPX_THREAD_TLS
#endif

#endif /* SPX_THREAD_H_DEFINED */
