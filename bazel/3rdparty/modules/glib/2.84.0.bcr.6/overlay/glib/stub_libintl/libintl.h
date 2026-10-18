#ifndef BCR_GLIB_OVERLAY_STUB_LIBINTL_H
#define BCR_GLIB_OVERLAY_STUB_LIBINTL_H

#include <stddef.h>

#ifndef gettext
static inline char *gettext(const char *m) {
	return (char *) m;
}
#endif

#ifndef dgettext
static inline char *dgettext(const char *d, const char *m) {
	(void) d;
	return (char *) m;
}
#endif

#ifndef dcgettext
static inline char *dcgettext(const char *d, const char *m, int c) {
	(void) d;
	(void) c;
	return (char *) m;
}
#endif

#ifndef ngettext
static inline char *ngettext(const char *m1, const char *m2, unsigned long n) {
	return (char *) ((n == 1) ? m1 : m2);
}
#endif

#ifndef dngettext
static inline char *dngettext(const char *d, const char *m1, const char *m2, unsigned long n) {
	(void) d;
	return (char *) ((n == 1) ? m1 : m2);
}
#endif

#ifndef dcngettext
static inline char *dcngettext(const char *d, const char *m1, const char *m2, unsigned long n, int c) {
	(void) d;
	(void) c;
	return (char *) ((n == 1) ? m1 : m2);
}
#endif

#ifndef textdomain
static inline char *textdomain(const char *d) {
	(void) d;
	return NULL;
}
#endif

#ifndef bindtextdomain
static inline char *bindtextdomain(const char *d, const char *dir) {
	(void) d;
	(void) dir;
	return (char *) "/";
}
#endif

#ifndef bind_textdomain_codeset
static inline char *bind_textdomain_codeset(const char *d, const char *c) {
	(void) d;
	return (char *) c;
}
#endif

#endif
