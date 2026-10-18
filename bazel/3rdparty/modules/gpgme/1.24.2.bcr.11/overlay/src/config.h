#ifndef GPGME_CONFIG_H
#define GPGME_CONFIG_H
#define _GNU_SOURCE 1
#define _REENTRANT 1
#define PACKAGE "gpgme"
#define PACKAGE_NAME "gpgme"
#define PACKAGE_VERSION "1.24.2"
#define VERSION "1.24.2"
#define BUILD_COMMITID "0"
#define BUILD_REVISION "0"
#define BUILD_TIMESTAMP "2025-01-01T00:00+0000"
#define GPG_ERR_SOURCE_DEFAULT GPG_ERR_SOURCE_GPGME
#define ENABLE_UISERVER 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_UIO_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LOCALE_H 1
#define HAVE_POLL_H 1
#define HAVE_DLFCN_H 1
#if defined(__linux__)
#define HAVE_ERROR_T 1
#endif
#define HAVE_FSEEKO 1
#define HAVE_GETEGID 1
#define HAVE_GETGID 1
#define HAVE_NANOSLEEP 1
#define HAVE_POSIXDECL_TTYNAME_R 1
#define HAVE_DECL_TTYNAME_R 1
#define HAVE_TTYNAME_R 1
#define HAVE_SETENV 1
#define HAVE_SETLOCALE 1
#define HAVE_STPCPY 1
#if defined(__linux__)
#define HAVE_SYS_GETTID 1
#endif
#define HAVE_THREAD_SAFE_GETENV 1
#define HAVE_TIMEGM 1
#define HAVE_TLS 1
#define HAVE_UINTPTR_T 1
#define SIZEOF_UNSIGNED_INT 4
#define USE_DESCRIPTOR_PASSING 1
#if defined(__linux__)
#define USE_LINUX_GETDENTS 1
#endif
#define PATHSEP_C ':'
#define DIRSEP_C '/'
#define DIRSEP_S "/"
#define GPGME_GCC_A_PURE __attribute__ ((__pure__))
#define GPG_ERR_ENABLE_GETTEXT_MACROS 1
#define CRIGHTBLURB "Copyright (C) 2000 Werner Koch\nCopyright (C) 2001--2025 g10 Code GmbH\n"
#define FLEXIBLE_ARRAY_MEMBER /**/
#endif
