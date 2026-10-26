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
#define NV 2 /* max number of --excludepkgs / --module-enable occurrences */

static int is_sep (char c) { return c == ',' || c == ' '; }

/* Next run [*s,*e) of non-separator characters of v after position *pos, stripped. */
static int strip_run (const char *v, size_t a, size_t b, char *buf)
{
  while (a < b && model_isspace (v[a])) a++;
  while (b > a && model_isspace (v[b - 1])) b--;
  for (size_t i = a; i < b; i++) buf[i - a] = v[i];
  buf[b - a] = '\0';
  return b > a;
}

static int in_list (char **list, const char *tok)
{
  for (char **p = list; *p; p++)
    if (strcmp (*p, tok) == 0) return 1;
  return 0;
}

/* completeness: every non-empty stripped run of every input is in the output */
static void check_complete (char **values, char **res)
{
  for (char **v = values; v && *v; v++)
    {
      size_t i = 0, start = 0;
      for (;;)
        {
          while ((*v)[i] && !is_sep ((*v)[i])) i++;
          char buf[L + 1];
          if (strip_run (*v, start, i, buf))
            __CPROVER_assert (in_list (res, buf), "completeness: input token missing from output");
          if (!(*v)[i]) break;
          i++;
          start = i;
        }
    }
}

/* soundness: every output token is a stripped run of some input */
static int is_run_of (char **values, const char *tok)
{
  for (char **v = values; v && *v; v++)
    {
      size_t i = 0, start = 0;
      for (;;)
        {
          while ((*v)[i] && !is_sep ((*v)[i])) i++;
          char buf[L + 1];
          if (strip_run (*v, start, i, buf) && strcmp (buf, tok) == 0) return 1;
          if (!(*v)[i]) break;
          i++;
          start = i;
        }
    }
  return 0;
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
      for (unsigned i = 0; i < nv; i++) values[i] = nondet_str (L);
    }

  GStrv res = split_option_values (values);

  /* terminated, usable result; every entry clean and unique */
  __CPROVER_assert (res != NULL, "result is non-NULL");
  unsigned n = 0;
  for (char **p = res; *p; p++, n++)
    {
      __CPROVER_assert (**p != '\0', "no empty token");
      for (char *c = *p; *c; c++) __CPROVER_assert (!is_sep (*c), "no separator inside a token");
      __CPROVER_assert (!model_isspace ((*p)[0]), "no leading whitespace");
      __CPROVER_assert (!model_isspace ((*p)[strlen (*p) - 1]), "no trailing whitespace");
      __CPROVER_assert (is_run_of (values, *p), "soundness: token comes from the input");
      for (char **q = p + 1; *q; q++) __CPROVER_assert (strcmp (*p, *q) != 0, "uniqueness");
    }
  check_complete (values, res);
  if (!values || !values[0]) __CPROVER_assert (res[0] == NULL, "no input -> empty list");

  /* run_lock: excludepkgs[0] ? join(",") -> dnf_conf_main_set_option */
  if (res[0])
    {
      gchar *globs = g_strjoinv (",", res);
      __CPROVER_assert (globs != NULL && globs[0] != '\0', "joined list is non-empty");
      /* round trip: splitting the joined value gives the same list back */
      char *one[2] = { globs, NULL };
      GStrv again = split_option_values (one);
      unsigned m = 0;
      for (; again[m] && res[m]; m++) __CPROVER_assert (strcmp (again[m], res[m]) == 0, "round trip");
      __CPROVER_assert (!again[m] && !res[m], "round trip: same length");
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
