#ifndef _ASSUAN_CONFIG_H_INCLUDED
#define _ASSUAN_CONFIG_H_INCLUDED
#define _GNU_SOURCE 1
#define PACKAGE "libassuan"
#define PACKAGE_NAME "libassuan"
#define PACKAGE_VERSION "3.0.2"
#define VERSION "3.0.2"
#define BUILD_COMMITID "0"
#define BUILD_TIMESTAMP "2025-01-01T00:00+0000"
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_UIO_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LOCALE_H 1
#define HAVE_FCNTL_H 1
#define HAVE_DLFCN_H 1
#define HAVE_UINTPTR_T 1
#if defined(__linux__)
#define HAVE_SO_PEERCRED 1
#define HAVE_FOPENCOOKIE 1
#elif defined(__APPLE__)
#define HAVE_FUNOPEN 1
#define HAVE_SYS_UCRED_H 1
#endif
#define HAVE_GETADDRINFO 1
#define HAVE_GETRLIMIT 1
#define HAVE_INET_PTON 1
#if defined(__linux__)
#define HAVE_MEMRCHR 1
#endif
#define HAVE_NANOSLEEP 1
#define HAVE_SETENV 1
#define HAVE_STAT 1
#define HAVE_STPCPY 1
#define USE_DESCRIPTOR_PASSING 1
#define GPG_ERR_SOURCE_DEFAULT GPG_ERR_SOURCE_ASSUAN
#endif
