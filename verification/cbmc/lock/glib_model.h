/* GLib / libdnf model for the CBMC harnesses in this directory.
 *
 * Every function here re-implements the documented GLib contract on top of
 * malloc/free so that CBMC checks the CALLER's use of it: NULL arguments that
 * GLib would answer with a g_return_if_fail CRITICAL are asserted instead,
 * allocations are exact-fit (no slack, so a one-element overrun is an
 * out-of-bounds), and a live-allocation counter backs the leak check (CBMC's
 * own --memory-leak-check only tracks the most recent allocation).
 * See TRUSTED.md for what this model assumes.
 */
#ifndef GLIB_MODEL_H
#define GLIB_MODEL_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

typedef char gchar;
typedef int gint;
typedef unsigned int guint;
typedef int gboolean;
typedef void *gpointer;
typedef const void *gconstpointer;
typedef gchar **GStrv;
typedef struct _GError GError;
#define TRUE 1
#define FALSE 0
#define G_BEGIN_DECLS
#define G_END_DECLS
#define G_MAXINT INT_MAX

extern unsigned model_live; /* live g_malloc'd blocks */
unsigned model_live = 0;

/* memcpy as a byte loop: CBMC's library memcpy with a symbolic size is encoded as one huge array
 * update (GBs of SAT memory); the loop is bounded by --unwind and still bounds-checks every byte. */
static void *model_memcpy (void *d, const void *s, size_t n)
{
  unsigned char *dd = d;
  const unsigned char *ss = s;
  for (size_t i = 0; i < n; i++)
    dd[i] = ss[i];
  return d;
}
#ifdef MODEL_RELAXED
#define memcpy model_memcpy
#endif

#define MODEL_ASSERT(c, msg) __CPROVER_assert((c), msg)

static gpointer g_malloc (size_t n)
{
  gpointer p = malloc (n ? n : 1);
  __CPROVER_assume (p != NULL);
  model_live++;
  return p;
}

/* Allocation of a model-created string / array whose size is symbolic. Default: exact fit. With
 * -DMODEL_RELAXED the object always has `cap` bytes (n <= cap is asserted): CBMC encodes an object of
 * symbolic size so expensively that nested splits exceed 6 GiB. Only the harnesses that need it
 * set MODEL_RELAXED (see TRUSTED.md); allocations made by the SHIPPED code (g_new0) stay exact. */
#ifndef MODEL_STR_CAP
#define MODEL_STR_CAP 64
#endif
static gpointer model_alloc (size_t n, size_t cap)
{
#ifdef MODEL_RELAXED
  MODEL_ASSERT (n <= cap, "model allocation cap too small");
  return g_malloc (cap);
#else
  return g_malloc (n);
#endif
}

static gpointer g_malloc0_n (size_t n, size_t sz)
{
  /* GLib aborts when n * sz overflows. */
  __CPROVER_assume (sz == 0 || n <= SIZE_MAX / sz);
  gpointer p = calloc (n ? n : 1, sz ? sz : 1);
  __CPROVER_assume (p != NULL);
  model_live++;
  return p;
}
#define g_new0(T, n) ((T *)g_malloc0_n ((size_t)(n), sizeof (T)))

static void g_free (gpointer p)
{
  if (p)
    {
      MODEL_ASSERT (model_live > 0, "g_free of more blocks than allocated");
      model_live--;
      free (p);
    }
}

static gchar *g_strdup (const gchar *s)
{
  if (!s)
    return NULL;
  size_t n = strlen (s);
  gchar *r = model_alloc (n + 1, MODEL_STR_CAP);
  memcpy (r, s, n + 1);
  return r;
}

static gchar *g_strndup (const gchar *s, size_t n)
{
  gchar *r = model_alloc (n + 1, MODEL_STR_CAP);
  memcpy (r, s, n);
  r[n] = '\0';
  return r;
}

static gboolean g_str_equal (gconstpointer a, gconstpointer b)
{
  return strcmp ((const char *)a, (const char *)b) == 0;
}

static int g_strcmp0 (const char *a, const char *b)
{
  if (!a)
    return -(a != b);
  if (!b)
    return a != b;
  return strcmp (a, b);
}

static gboolean g_str_has_prefix (const gchar *s, const gchar *p)
{
  MODEL_ASSERT (s != NULL && p != NULL, "g_str_has_prefix: NULL argument");
  return strncmp (s, p, strlen (p)) == 0;
}

static gboolean g_str_has_suffix (const gchar *s, const gchar *x)
{
  MODEL_ASSERT (s != NULL && x != NULL, "g_str_has_suffix: NULL argument");
  size_t sl = strlen (s), xl = strlen (x);
  return sl >= xl && strcmp (s + (sl - xl), x) == 0;
}

static void g_strfreev (GStrv v)
{
  if (!v)
    return;
  for (gchar **p = v; *p; p++)
    g_free (*p);
  g_free (v);
}

#ifdef MODEL_RELAXED
/* Shared by g_strsplit / g_strsplit_set: ONE scan records the token boundaries in small fixed
 * arrays, then the result is built with at most MODEL_MAXTOK constant-bound copies. (Nested
 * count/fill passes over symbolic-size strings made CBMC unroll tokens x characters x characters
 * and cost tens of GB.) Contract: "" -> {NULL}; otherwise the string is cut at each separator (a
 * delimiter string, or any one delimiter character when is_set) at most max_tokens-1 times;
 * empty tokens are kept. MODEL_MAXTOK is a harness precondition (>= strlen+1 of any input); the
 * model ASSERTS it, so a too small value fails loudly instead of silently dropping behaviour. */
#ifndef MODEL_MAXTOK
#define MODEL_MAXTOK 8
#endif
static GStrv model_split (const gchar *string, const gchar *delims, int is_set, gint max_tokens)
{
  if (max_tokens < 1)
    max_tokens = G_MAXINT;
  size_t dl = strlen (delims), n = 0;
  size_t st[MODEL_MAXTOK + 1], en[MODEL_MAXTOK + 1];
  if (*string)
    {
      size_t start = 0, i = 0;
      gint left = max_tokens;
      while (string[i])
        {
          size_t sl = 0;
          if (left > 1)
            {
              if (is_set)
                {
                  for (const gchar *d = delims; *d; d++)
                    if (string[i] == *d)
                      sl = 1;
                }
              else
                {
                  size_t k = 0;
                  while (k < dl && string[i + k] == delims[k])
                    k++;
                  if (k == dl)
                    sl = dl;
                }
            }
          if (sl)
            {
              MODEL_ASSERT (n < MODEL_MAXTOK, "model bound MODEL_MAXTOK too small");
              st[n] = start;
              en[n] = i;
              n++;
              i += sl;
              start = i;
              left--;
            }
          else
            i++;
        }
      MODEL_ASSERT (n < MODEL_MAXTOK, "model bound MODEL_MAXTOK too small");
      st[n] = start;
      en[n] = i;
      n++;
    }
  /* exact fit even in relaxed mode: a read of r[n+1] must stay an out-of-bounds */
  GStrv r = g_malloc ((n + 1) * sizeof (gchar *));
  for (size_t t = 0; t < MODEL_MAXTOK; t++)
    {
      if (t >= n)
        break;
      r[t] = g_strndup (string + st[t], en[t] - st[t]);
    }
  r[n] = NULL;
  return r;
}

/* g_strsplit: documented behaviour incl. "" -> {NULL}, "a=" -> {"a","",NULL}. */
static GStrv g_strsplit (const gchar *string, const gchar *delimiter, gint max_tokens)
{
  MODEL_ASSERT (string != NULL && delimiter != NULL && *delimiter != '\0',
                "g_strsplit: NULL string/delimiter");
  return model_split (string, delimiter, 0, max_tokens);
}

/* g_strsplit_set: every delimiter character separates; empty tokens kept. */
static GStrv g_strsplit_set (const gchar *string, const gchar *delims, gint max_tokens)
{
  MODEL_ASSERT (string != NULL && delims != NULL, "g_strsplit_set: NULL string/delimiters");
  return model_split (string, delims, 1, max_tokens);
}

#else
/* Exact-fit reference model (used by the harnesses that are cheap enough for it). */
/* g_strsplit: documented behaviour incl. "" -> {NULL}, "a=" -> {"a","",NULL}. */
static GStrv g_strsplit (const gchar *string, const gchar *delimiter, gint max_tokens)
{
  MODEL_ASSERT (string != NULL && delimiter != NULL && *delimiter != '\0',
                "g_strsplit: NULL string/delimiter");
  if (max_tokens < 1)
    max_tokens = G_MAXINT;
  size_t dlen = strlen (delimiter), slen = strlen (string);
  gchar **tmp = malloc ((slen + 2) * sizeof (gchar *));
  __CPROVER_assume (tmp != NULL);
  size_t n = 0;
  const gchar *rem = string, *s = strstr (rem, delimiter);
  while (--max_tokens && s)
    {
      tmp[n++] = g_strndup (rem, (size_t)(s - rem));
      rem = s + dlen;
      s = strstr (rem, delimiter);
    }
  if (*string)
    tmp[n++] = g_strdup (rem);
  GStrv r = g_malloc ((n + 1) * sizeof (gchar *));
  for (size_t i = 0; i < n; i++)
    r[i] = tmp[i];
  r[n] = NULL;
  free (tmp);
  return r;
}

/* g_strsplit_set: every delimiter character separates; empty tokens kept. */
static GStrv g_strsplit_set (const gchar *string, const gchar *delims, gint max_tokens)
{
  MODEL_ASSERT (string != NULL && delims != NULL, "g_strsplit_set: NULL string/delimiters");
  if (max_tokens < 1)
    max_tokens = G_MAXINT;
  size_t slen = strlen (string);
  gchar **tmp = malloc ((slen + 2) * sizeof (gchar *));
  __CPROVER_assume (tmp != NULL);
  size_t n = 0;
  if (*string)
    {
      const gchar *start = string, *p = string;
      for (; *p && max_tokens > 1; p++)
        {
          if (strchr (delims, *p) != NULL)
            {
              tmp[n++] = g_strndup (start, (size_t)(p - start));
              start = p + 1;
              max_tokens--;
            }
        }
      tmp[n++] = g_strdup (start);
    }
  GStrv r = g_malloc ((n + 1) * sizeof (gchar *));
  for (size_t i = 0; i < n; i++)
    r[i] = tmp[i];
  r[n] = NULL;
  free (tmp);
  return r;
}

#endif

static int model_isspace (char c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

static gchar *g_strchug (gchar *s)
{
  MODEL_ASSERT (s != NULL, "g_strchug: NULL");
  gchar *st = s;
  while (*st && model_isspace (*st))
    st++;
  memmove (s, st, strlen (st) + 1);
  return s;
}

static gchar *g_strchomp (gchar *s)
{
  MODEL_ASSERT (s != NULL, "g_strchomp: NULL");
  size_t len = strlen (s);
  while (len > 0)
    {
      if (!model_isspace (s[len - 1]))
        break;
      s[len - 1] = '\0';
      len--;
    }
  return s;
}
#define g_strstrip(s) g_strchomp (g_strchug (s))

static gchar *g_strjoinv (const gchar *sep, GStrv v)
{
  MODEL_ASSERT (v != NULL, "g_strjoinv: NULL array");
  size_t sl = sep ? strlen (sep) : 0, total = 1, cnt = 0;
  for (gchar **p = v; *p; p++, cnt++)
    total += strlen (*p) + (cnt ? sl : 0);
  gchar *r = model_alloc (total, MODEL_STR_CAP), *w = r;
  *w = '\0';
  cnt = 0;
  for (gchar **p = v; *p; p++, cnt++)
    {
      if (cnt && sl)
        {
          memcpy (w, sep, sl);
          w += sl;
        }
      size_t l = strlen (*p);
      memcpy (w, *p, l);
      w += l;
      *w = '\0';
    }
  return r;
}

static gchar *g_strconcat (const gchar *a, const gchar *b, const gchar *term)
{
  MODEL_ASSERT (a && b && term == NULL, "g_strconcat: model supports exactly two parts");
  size_t la = strlen (a), lb = strlen (b);
  gchar *r = model_alloc (la + lb + 1, MODEL_STR_CAP);
  memcpy (r, a, la);
  memcpy (r + la, b, lb + 1);
  return r;
}

/* Approximation: "a/b" without GLib's separator collapsing (only validity is checked). */
static gchar *g_build_path (const gchar *sep, const gchar *a, const gchar *b, const gchar *term)
{
  MODEL_ASSERT (sep && a && b && term == NULL, "g_build_path: bad arguments");
  size_t ls = strlen (sep), la = strlen (a), lb = strlen (b);
  gchar *r = model_alloc (la + ls + lb + 1, MODEL_STR_CAP);
  memcpy (r, a, la);
  memcpy (r + la, sep, ls);
  memcpy (r + la + ls, b, lb + 1);
  return r;
}

/* GPtrArray: exact-fit storage. */
typedef struct
{
  gpointer *pdata;
  guint len;
  void (*free_func) (gpointer);
} GPtrArray;

static GPtrArray *g_ptr_array_new_with_free_func (void (*f) (gpointer))
{
  GPtrArray *a = g_malloc (sizeof *a);
  a->pdata = NULL;
  a->len = 0;
  a->free_func = f;
  return a;
}

static void g_ptr_array_add (GPtrArray *a, gpointer d)
{
  MODEL_ASSERT (a != NULL, "g_ptr_array_add: NULL array");
  MODEL_ASSERT (a->len < UINT_MAX, "g_ptr_array_add: length overflow");
  gpointer *np = g_malloc (((size_t)a->len + 1) * sizeof (gpointer));
  for (guint i = 0; i < a->len; i++)
    np[i] = a->pdata[i];
  np[a->len] = d;
  g_free (a->pdata);
  a->pdata = np;
  a->len++;
}

static gboolean g_ptr_array_find_with_equal_func (GPtrArray *a, gconstpointer needle,
                                                  gboolean (*eq) (gconstpointer, gconstpointer),
                                                  guint *idx)
{
  MODEL_ASSERT (a != NULL && eq != NULL, "g_ptr_array_find_with_equal_func: NULL");
  for (guint i = 0; i < a->len; i++)
    if (eq (a->pdata[i], needle))
      {
        if (idx)
          *idx = i;
        return TRUE;
      }
  return FALSE;
}

/* free_segment == FALSE hands the storage to the caller; elements are NOT freed. */
static gpointer g_ptr_array_free (GPtrArray *a, gboolean free_segment)
{
  MODEL_ASSERT (a != NULL, "g_ptr_array_free: NULL");
  gpointer *seg = a->pdata;
  if (free_segment)
    {
      if (a->free_func)
        for (guint i = 0; i < a->len; i++)
          a->free_func (a->pdata[i]);
      g_free (seg);
      seg = NULL;
    }
  g_free (a);
  return seg;
}

static void g_ptr_array_unref (GPtrArray *a)
{
  if (a)
    g_ptr_array_free (a, TRUE);
}

#define g_steal_pointer(pp) ((__typeof__ (*(pp)))g_steal_pointer_impl ((gpointer *)(pp)))
static gpointer g_steal_pointer_impl (gpointer *pp)
{
  gpointer p = *pp;
  *pp = NULL;
  return p;
}

/* g_autofree / g_auto(T) / g_autoptr(T): in GLib these are __attribute__((cleanup)) declarations,
 * which CBMC 6.11 ignores. They expand to the plain type here and extract.py inserts the explicit
 * model_cleanup_<kind> (&var) calls at every scope exit (see extract.py). */
#define g_autofree
#define g_auto(T) glib_auto_##T
#define glib_auto_GStrv GStrv
#define g_autoptr(T) glib_autoptr_##T
#define glib_autoptr_GPtrArray GPtrArray *
static void model_cleanup_autofree (void *p) { g_free (*(void **)p); }
static void model_cleanup_GStrv (GStrv *p) { g_strfreev (*p); }
static void model_cleanup_GPtrArray (GPtrArray **p) { g_ptr_array_unref (*p); }

/* Nondeterministic C string of 0..maxlen non-NUL characters (heap, counted, exact-fit).
 * The length is chosen by a chain of concrete-size allocations rather than one malloc of a
 * symbolic size: same set of behaviours, but CBMC then sees objects of known size, which keeps
 * the solver's memory far lower. */
#ifndef MODEL_ALPHABET
#define MODEL_ALPHABET(c) ((c) != 0)
#endif
static gchar *nondet_str_after (const char *prefix, size_t pl, unsigned maxlen)
{
  unsigned n;
  __CPROVER_assume (n <= maxlen);
  for (unsigned k = 0; k <= maxlen; k++)
    if (n == k)
      {
        gchar *s = g_malloc (pl + (size_t)k + 1);
        for (size_t i = 0; i < pl; i++)
          s[i] = prefix[i];
        for (unsigned i = 0; i < k; i++)
          {
            gchar c;
            __CPROVER_assume (MODEL_ALPHABET (c));
            s[pl + i] = c;
          }
        s[pl + k] = '\0';
        return s;
      }
  __CPROVER_assume (0); /* unreachable: n <= maxlen */
  return NULL;
}

/* Same set of strings in ONE buffer of fixed size (prefix + maxlen + 1): the NUL is placed at a
 * symbolic position. Pointers into it have a single target object, which keeps nested splits cheap.
 * Bytes after the NUL are unconstrained but never read by a correct consumer. */
static gchar *nondet_str_fixed (const char *prefix, size_t pl, unsigned maxlen)
{
  unsigned n;
  __CPROVER_assume (n <= maxlen);
  gchar *s = g_malloc (pl + (size_t)maxlen + 1);
  for (size_t i = 0; i < pl; i++)
    s[i] = prefix[i];
  for (unsigned i = 0; i < maxlen; i++)
    {
      gchar c;
      __CPROVER_assume (i >= n || MODEL_ALPHABET (c));
      s[pl + i] = c;
    }
  s[pl + n] = '\0';
  return s;
}
static gchar *nondet_str (unsigned maxlen) { return nondet_str_after ("", 0, maxlen); }

#endif
