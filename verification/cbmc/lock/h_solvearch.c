/* find_solve_arch (dnf/lock-main.c): --forcearch, else first non-noarch/src --archlist entry.
 * Spec (e2e test_lock_features.sh basearch section and the function comment): --forcearch wins
 * when non-empty; otherwise the first non-empty --archlist entry (split on ',' and ' ') that is
 * not "noarch"/"src"; otherwise NULL (host arch). Malformed options -> NULL.
 *
 * GOptionContext is modelled (see TRUSTED.md): parse may fail, may leave any of the two string
 * options unset or set them to an arbitrary string, and may consume any number of arguments.
 */
#include "glib_model.h"

typedef struct { int dummy; } GOptionContext;
enum { G_OPTION_ARG_NONE, G_OPTION_ARG_STRING };
typedef struct
{
  const gchar *long_name;
  gchar short_name;
  gint flags;
  int arg;
  gpointer arg_data;
  const gchar *description;
  const gchar *arg_description;
} GOptionEntry;

#ifndef L
#define L 6
#endif
#ifdef WITNESS
#define WITNESS_IF(c) __CPROVER_assert (!(c), "witness")
#else
#define WITNESS_IF(c) ((void)0)
#endif

int nondet_int (void);
static const GOptionEntry *model_entries;
/* Oracle copies of what the modelled parse stored: fixed buffers, not heap, so they are neither
 * leak-checked nor able to fail to allocate. */
static char arch_buf[L + 1], force_buf[L + 1];
static char *oracle_archlist, *oracle_forcearch; /* NULL = option not given */
static int model_parse_ok;

static GOptionContext *g_option_context_new (const gchar *s)
{
  GOptionContext *c = g_malloc (sizeof *c);
  return c;
}
static void g_option_context_free (GOptionContext *c) { g_free (c); }
static void model_cleanup_GOptionContext (GOptionContext **p) { if (*p) g_option_context_free (*p); }
#define glib_autoptr_GOptionContext GOptionContext *
static void g_option_context_add_main_entries (GOptionContext *c, const GOptionEntry *e, const gchar *d)
{
  MODEL_ASSERT (c && e, "add_main_entries: NULL");
  model_entries = e;
}
static void g_option_context_set_ignore_unknown_options (GOptionContext *c, gboolean b) { MODEL_ASSERT (c, "NULL ctx"); }
static void g_option_context_set_help_enabled (GOptionContext *c, gboolean b) { MODEL_ASSERT (c, "NULL ctx"); }
static gboolean g_option_context_parse (GOptionContext *c, int *argc, char ***argv, GError **err)
{
  MODEL_ASSERT (c && argc && argv && *argv, "parse: NULL");
  for (int i = 0; i < *argc; i++)
    (void)(*argv)[i]; /* every consumed slot must exist */
  for (const GOptionEntry *e = model_entries; e->long_name; e++)
    {
      int set;
      if (e->arg == G_OPTION_ARG_STRING && set)
        {
          gchar **dst = e->arg_data;
          g_free (*dst);
          *dst = nondet_str (L);
          int is_arch = strcmp (e->long_name, "archlist") == 0;
          char *buf = is_arch ? arch_buf : force_buf;
          strcpy (buf, *dst); /* *dst has at most L chars: fits */
          if (is_arch)
            oracle_archlist = buf;
          else
            oracle_forcearch = buf;
        }
    }
  int newargc;
  __CPROVER_assume (newargc >= 0 && newargc <= *argc);
  *argc = newargc;
  model_parse_ok = nondet_int ();
  return model_parse_ok;
}

#include "find_solve_arch.inc"

static int ok_archlist_entry (const char *s) { return *s && strcmp (s, "noarch") != 0 && strcmp (s, "src") != 0; }

int main (void)
{
  int argc;
  __CPROVER_assume (argc >= 1 && argc <= 3); /* a process always has argv[0] */
  char **argv = g_new0 (char *, argc + 1);   /* argv[argc] == NULL, as the kernel provides */
  for (int i = 0; i < argc; i++)
    argv[i] = (char *)"arg";

  gchar *r = find_solve_arch (argc, argv);

  /* oracle */
  if (!model_parse_ok)
    __CPROVER_assert (r == NULL, "malformed options -> NULL");
  else if (oracle_forcearch && *oracle_forcearch)
    {
      __CPROVER_assert (r && strcmp (r, oracle_forcearch) == 0, "forcearch wins");
      WITNESS_IF (1);
    }
  else
    {
      char exp[L + 1];
      int have_exp = 0;
      if (oracle_archlist)
        {
          size_t i = 0, start = 0;
          for (;;)
            {
              while (oracle_archlist[i] && oracle_archlist[i] != ',' && oracle_archlist[i] != ' ') i++;
              char tok[L + 1];
              size_t k = i - start;
              memcpy (tok, oracle_archlist + start, k);
              tok[k] = '\0';
              if (ok_archlist_entry (tok))
                {
                  strcpy (exp, tok);
                  have_exp = 1;
                  break;
                }
              if (!oracle_archlist[i]) break;
              i++;
              start = i;
            }
        }
      if (have_exp)
        {
          __CPROVER_assert (r && strcmp (r, exp) == 0, "first usable archlist entry");
          WITNESS_IF (1);
        }
      else
        __CPROVER_assert (r == NULL, "no usable entry -> NULL");
    }

  /* the caller owns r and g_free()s it (g_autofree solve_arch) */
  g_free (r);
  g_free (argv);
  __CPROVER_assert (model_live == 0, "no leak, no double free");
  return 0;
}
