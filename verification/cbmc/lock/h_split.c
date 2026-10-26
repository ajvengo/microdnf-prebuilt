/* split_option_values (dnf/lock.c) and its use for --excludepkgs / --module-enable.
 *
 * Spec (from e2e/test_lock_features.sh): repeated and comma/space separated values are
 * flattened, stripped, empty tokens dropped and duplicates removed ("mysql:8.4,mysql:8.4"
 * + "mysql:8.4" -> one entry); the joined list handed to libdnf is "a,b,c".
 */
#include "glib_model.h"
#include "split_option_values.inc"

#ifndef L
#define L 4 /* max length of one input string */
#endif
#ifdef FIXED /* one fixed-size buffer per input, NUL at a symbolic position */
#define INPUT_STR(l) nondet_str_fixed ("", 0, l)
#else
#define INPUT_STR nondet_str
#endif
#ifndef NV
#define NV 2
#endif
/* NV: max number of --excludepkgs / --module-enable occurrences */

static int is_sep (char c) { return c == ',' || c == ' '; }

/* Does the stripped run v[a,b) equal the string t? Compared in place (no copy of the run). */
static int run_eq (const char *v, size_t a, size_t b, const char *t)
{
  while (a < b && model_isspace (v[a])) a++;
  while (b > a && model_isspace (v[b - 1])) b--;
  if (b == a) return 0; /* empty runs are dropped, they equal nothing */
  size_t i = 0;
  for (; a + i < b; i++)
    if (t[i] != v[a + i]) return 0;
  return t[i] == '\0';
}

/* Does the stripped run v[a,b) have any content? */
static int run_nonempty (const char *v, size_t a, size_t b)
{
  for (; a < b; a++)
    if (!model_isspace (v[a])) return 1;
  return 0;
}

#ifndef ONLY
#define ONLY (-1) /* all properties in one run; run.sh splits them into separate jobs */
#endif
#define WANT(n) (ONLY < 0 || ONLY == (n))

#ifdef FLAT_SOUND
/* Soundness, flat form: tok (no leading/trailing whitespace, non-empty) is a stripped run of some
 * input iff it occurs at some position whose left context is whitespace (not a separator) back to
 * a separator / the start, and whose right context is whitespace forward to a separator / the end. */
static int is_run_of (char **values, const char *tok)
{
  size_t m = strlen (tok);
  if (m == 0 || model_isspace (tok[0]) || model_isspace (tok[m - 1])) return 0;
  for (char **v = values; v && *v; v++)
    {
      size_t len = strlen (*v);
      for (size_t a = 0; a + m <= len; a++)
        {
          const char *s = *v;
          size_t i = 0;
          while (i < m && s[a + i] == tok[i]) i++;
          if (i < m) continue;
          size_t l = a;
          while (l > 0 && model_isspace (s[l - 1]) && !is_sep (s[l - 1])) l--;
          size_t r = a + m;
          while (s[r] && model_isspace (s[r]) && !is_sep (s[r])) r++;
          if ((l == 0 || is_sep (s[l - 1])) && (s[r] == '\0' || is_sep (s[r]))) return 1;
        }
    }
  return 0;
}
#else
/* Soundness: is the (stripped, non-empty) token tok some stripped run of some input? */
static int is_run_of (char **values, const char *tok)
{
  for (char **v = values; v && *v; v++)
    {
      size_t i = 0, start = 0;
      for (;;)
        {
          while ((*v)[i] && !is_sep ((*v)[i])) i++;
          if (run_eq (*v, start, i, tok)) return 1;
          if (!(*v)[i]) break;
          i++;
          start = i;
        }
    }
  return 0;
}
#endif

/* Properties quantified over "every output token" / "every pair" / "every input run" are checked
 * for ONE nondeterministically chosen token / pair / run: CBMC explores all choices, so this is
 * the universal statement, at the cost of one instance instead of a loop over all of them. */
static unsigned choose_below (unsigned n)
{
  unsigned k;
  __CPROVER_assume (k < n);
  return k;
}

int main (void)
{
  /* values: NULL (option absent) or a NULL-terminated array of 0..NV strings */
  char **values = NULL;
  unsigned nv;
  __CPROVER_assume (nv <= NV + 1);
  if (nv <= NV)
    {
      values = g_new0 (gchar *, nv + 1);
      for (unsigned i = 0; i < nv; i++) values[i] = INPUT_STR (L);
    }
  else
    nv = 0;

  GStrv res = split_option_values (values);

  /* terminated, usable result */
  __CPROVER_assert (res != NULL, "result is non-NULL");
  unsigned n = 0;
  while (res[n]) n++;
  if (nv == 0) __CPROVER_assert (n == 0, "no input -> empty list");

  if (n > 0 && WANT (0))
    {
      /* one chosen token is clean ... */
      char *t = res[choose_below (n)];
      __CPROVER_assert (*t != '\0', "no empty token");
      for (char *c = t; *c; c++) __CPROVER_assert (!is_sep (*c), "no separator inside a token");
      __CPROVER_assert (!model_isspace (t[0]), "no leading whitespace");
      __CPROVER_assert (!model_isspace (t[strlen (t) - 1]), "no trailing whitespace");
      /* ... and differs from any other chosen token (uniqueness) */
      unsigned k2 = choose_below (n);
      if (res[k2] != t) __CPROVER_assert (strcmp (res[k2], t) != 0, "uniqueness");
    }
  if (n > 0 && WANT (1))
    __CPROVER_assert (is_run_of (values, res[choose_below (n)]), "soundness: token comes from the input");

  if (nv > 0 && WANT (2))
    {
      /* completeness: a chosen maximal separator-free run of a chosen input, if it has content,
       * appears in the output */
      const char *v = values[choose_below (nv)];
      size_t len = strlen (v), a, b;
      __CPROVER_assume (a <= b && b <= len);
      __CPROVER_assume (a == 0 || is_sep (v[a - 1]));
      __CPROVER_assume (b == len || is_sep (v[b]));
      for (size_t i = a; i < b; i++) __CPROVER_assume (!is_sep (v[i]));
      __CPROVER_assume (run_nonempty (v, a, b));
      int found = 0;
      for (char **p = res; *p; p++)
        if (run_eq (v, a, b, *p)) found = 1;
      __CPROVER_assert (found, "completeness: input token missing from output");
    }

  /* run_lock: excludepkgs[0] ? join(",") -> dnf_conf_main_set_option */
  if (n > 0 && WANT (3))
    {
      gchar *globs = g_strjoinv (",", res);
      __CPROVER_assert (globs != NULL && globs[0] != '\0', "joined list is non-empty");
      /* round trip: splitting the joined value gives the same list back */
      char *one[2] = { globs, NULL };
      GStrv again = split_option_values (one);
      unsigned m = 0;
      while (again[m]) m++;
      __CPROVER_assert (m == n, "round trip: same length");
      if (m == n)
        {
          unsigned k = choose_below (n);
          __CPROVER_assert (strcmp (again[k], res[k]) == 0, "round trip");
        }
      g_strfreev (again);
      g_free (globs);
    }

/* Non-vacuity (run.sh w-split-*): -DWITNESS=n asserts the NEGATION of an interesting shape, so
   * CBMC must report exactly the assertion "witness" as failed, i.e. the shape is reachable. */
#if defined(WITNESS) && WITNESS == 1
  __CPROVER_assert (!(res[0] && res[1]), "witness"); /* two distinct tokens kept */
#elif defined(WITNESS) && WITNESS == 2
  __CPROVER_assert (!(values && values[0] && values[1] && strcmp (values[0], values[1]) == 0 && values[0][0]
                      && res[0] && !res[1]), "witness"); /* repeated occurrence collapsed to one entry */
#elif defined(WITNESS) && WITNESS == 3
  __CPROVER_assert (!(values && values[0] && strlen (values[0]) >= 2 && values[0][strlen (values[0]) - 1] == ','
                      && res[0]), "witness"); /* trailing comma (empty token) with a real token present */
#endif

  g_strfreev (res);
  g_strfreev (values);
  __CPROVER_assert (model_live == 0, "no leak");
  return 0;
}
