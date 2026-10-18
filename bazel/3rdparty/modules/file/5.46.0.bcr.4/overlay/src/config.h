#ifndef FILE_CONFIG_H
#define FILE_CONFIG_H
#define _GNU_SOURCE 1
#define PACKAGE "file"
#define PACKAGE_NAME "file"
#define PACKAGE_VERSION "5.46"
#define VERSION "5.46"
#define BUILTIN_ELF 1
#define ELFCORE 1
#define ZLIBSUPPORT 1
#define BZLIBSUPPORT 1
#define XZLIBSUPPORT 1
#define ZSTDLIBSUPPORT 1
#define HAVE_ZLIB_H 1
#define HAVE_BZLIB_H 1
#define HAVE_LZMA_H 1
#define HAVE_ZSTD_H 1
#define HAVE_ASCTIME_R 1
#define HAVE_ASPRINTF 1
#if defined(__linux__)
#define HAVE_BYTESWAP_H 1
#if defined(__APPLE__)
#define HAVE_STRLCPY 1
#define HAVE_STRLCAT 1
#endif
#endif
#define HAVE_CTIME_R 1
#define HAVE_DPRINTF 1
#define HAVE_ERR_H 1
#define HAVE_FORK 1
#define HAVE_FREELOCALE 1
#define HAVE_GETLINE 1
#define HAVE_GETOPT_H 1
#define HAVE_GETOPT_LONG 1
#define HAVE_GMTIME_R 1
#define HAVE_INTTYPES_H 1
#define HAVE_LOCALTIME_R 1
#define HAVE_MAJOR 1
#define HAVE_MBRTOWC 1
#define HAVE_MEMMEM 1
#define HAVE_MKSTEMP 1
#define HAVE_MMAP 1
#define HAVE_NEWLOCALE 1
#if defined(__linux__)
#define HAVE_PIPE2 1
#if defined(__APPLE__)
#define HAVE_STRLCPY 1
#define HAVE_STRLCAT 1
#endif
#endif
#define HAVE_POSIX_SPAWNP 1
#define HAVE_PREAD 1
#define HAVE_SIG_T 1
#define HAVE_SPAWN_H 1
#define HAVE_STDINT_H 1
#define HAVE_STRCASESTR 1
#define HAVE_STRNDUP 1
#define HAVE_STRTOF 1
#define HAVE_STRUCT_OPTION 1
#define HAVE_STRUCT_STAT_ST_RDEV 1
#define HAVE_STRUCT_TM_TM_GMTOFF 1
#define HAVE_STRUCT_TM_TM_ZONE 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_MMAN_H 1
#if defined(__linux__)
#define HAVE_SYS_SYSMACROS_H 1
#if defined(__APPLE__)
#define HAVE_STRLCPY 1
#define HAVE_STRLCAT 1
#endif
#endif
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_UNISTD_H 1
#define HAVE_USELOCALE 1
#define HAVE_UTIME 1
#define HAVE_UTIMES 1
#define HAVE_UTIME_H 1
#define HAVE_VASPRINTF 1
#define HAVE_VISIBILITY 1
#define HAVE_WCHAR_H 1
#define HAVE_WCTYPE_H 1
#define HAVE_WCWIDTH 1
#define MAGIC "/usr/share/misc/magic"
#if defined(__APPLE__)
#define HAVE_STRLCPY 1
#define HAVE_STRLCAT 1
#endif
#endif
