#ifndef LIB_XDGPP_HELPER_H
#define LIB_XDGPP_HELPER_H

#if defined _WIN32 || defined __CYGWIN__
# ifdef BUILDING_LIBXDGPP
#  define API_PUBLIC __declspec(dllexport)
# else
#  define API_PUBLIC __declspec(dllimport)
# endif
#elif defined __OS2__
# ifdef BUILDING_LIBXDGPP
#  define API_PUBLIC __declspec(dllexport)
# else
#  define API_PUBLIC
# endif
#else
# ifdef BUILDING_LIBXDGPP
#  define API_PUBLIC __attribute__((visibility("default")))
# else
#  define API_PUBLIC
# endif
#endif

#endif