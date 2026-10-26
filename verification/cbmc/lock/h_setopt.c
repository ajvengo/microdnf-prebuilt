/* process_global_option, "--setopt" branch (dnf/lock-main.c).
 * Spec (e2e test_lock_features.sh / test_lock_attrs.sh): "--setopt=<name>=<value>" splits at the
 * FIRST '='; cachedir / reposdir / varsdir / module_platform_id are routed to their context
 * setter, everything else (excludepkgs, best, <repo>.excludepkgs, ...) goes to
 * dnf_conf_add_setopt unchanged; a value without '=' is ignored.
 * The dnf_* setters are stubs that check their arguments are readable strings.
 */
#include "glib_model.h"

typedef struct { int dummy; } DnfContext;
typedef struct _GSList { gpointer data; struct _GSList *next; } GSList;
enum DnfConfPriority { DNF_CONF_COMMANDLINE = 70 };
#define DNF_CONTEXT(p) ((DnfContext *)(p))
#define DNF_CONF_COMMANDLINE_CONST 70

enum kind { NONE, CACHE, REPOS, VARS, PLATFORM, ADDOPT, OTHER };
static int calls;
static enum kind last;
static const char *exp_name, *exp_val;

static GSList *enable_disable_repos = NULL;
static GSList *g_slist_append(GSList *l, gpointer d) { return l; } /* other options, out of scope */
static void dnf_context_set_config_file_path (const gchar *v) { }
static void dnf_context_set_install_root (DnfContext *c, const gchar *v) { }
static void dnf_context_set_release_ver (DnfContext *c, const gchar *v) { }

static void check_strv (const gchar *const *v)
{
  MODEL_ASSERT (v != NULL, "list is non-NULL");
  for (const gchar *const *p = v; *p; p++) (void)strlen (*p);
}
static void dnf_context_set_repos_dir (DnfContext *c, const gchar *const *v)
{
  check_strv (v); last = REPOS; calls++;
  MODEL_ASSERT (exp_val != NULL, "reposdir gets the value");
}
static void dnf_context_set_vars_dir (DnfContext *c, const gchar *const *v)
{
  check_strv (v); last = VARS; calls++;
}
static void dnf_context_set_cache_dir (DnfContext *c, const gchar *v)
{
  (void)strlen (v); last = CACHE; calls++;
}
static void dnf_context_set_solv_dir (DnfContext *c, const gchar *v) { (void)strlen (v); }
static void dnf_context_set_lock_dir (DnfContext *c, const gchar *v) { (void)strlen (v); }
static void dnf_context_set_platform_module (DnfContext *c, const gchar *v)
{
  last = PLATFORM; calls++;
  MODEL_ASSERT (strcmp (v, exp_val) == 0, "module_platform_id receives exactly the value");
}
static gboolean dnf_conf_add_setopt (const gchar *name, enum DnfConfPriority p, const gchar *v, GError **e)
{
  last = ADDOPT; calls++;
  MODEL_ASSERT (strcmp (name, exp_name) == 0 && strcmp (v, exp_val) == 0,
                "unknown option passed through verbatim (split at the first '=')");
  MODEL_ASSERT (p == DNF_CONF_COMMANDLINE, "command-line priority");
  return TRUE;
}

#include "process_global_option.inc"

#ifndef L
#define L 3
#endif
#ifdef WITNESS
#define WITNESS_IF(c) __CPROVER_assert (!(c), "witness")
#else
#define WITNESS_IF(c) ((void)0)
#endif

#define VMAX (20 + 1 + L + 1) /* longest known name ("features.excludepkgs") + '=' + tail + NUL */

int main (void)
{
#ifdef SEL
  unsigned sel = SEL; /* one proof per branch: a single run over all eight needs > 20 GB */
#else
  unsigned sel;
  __CPROVER_assume (sel <= 7);
#endif
  /* value = <prefix> + 0..L arbitrary bytes. The prefix is a constant string; the length of the
   * tail is chosen by a chain of concrete-size allocations (see nondet_str_after). */
  static const char *const prefix[8] = { "cachedir=", "reposdir=", "varsdir=", "module_platform_id=",
                                         "excludepkgs=", "best=", "features.excludepkgs=", "" };
  gchar *value = nondet_str_fixed (prefix[sel], strlen (prefix[sel]), L);

  /* oracle: split at the first '=' (manual scan, independent of the model's strsplit) */
  size_t vl = strlen (value), eqpos = vl;
  for (size_t i = 0; i < vl; i++)
    if (value[i] == '=') { eqpos = i; break; }
  const int eq = eqpos < vl;
  char name[VMAX];
  if (eq)
    {
      memcpy (name, value, eqpos);
      name[eqpos] = '\0';
      exp_name = name;
      exp_val = value + eqpos + 1;
    }

  gboolean ok = process_global_option ("--setopt", value, (gpointer)&(DnfContext){ 0 }, NULL);
  __CPROVER_assert (ok == TRUE, "callback never rejects");

  if (!eq)
    __CPROVER_assert (calls == 0, "no '=' -> ignored");
  else if (strcmp (name, "cachedir") == 0)
    {
      __CPROVER_assert (last == CACHE && calls == 1, "cachedir -> cache dir");
      WITNESS_IF (1);
    }
  else if (strcmp (name, "reposdir") == 0)
    __CPROVER_assert (last == REPOS && calls == 1, "reposdir");
  else if (strcmp (name, "varsdir") == 0)
    __CPROVER_assert (last == VARS && calls == 1, "varsdir");
  else if (strcmp (name, "module_platform_id") == 0)
    {
      __CPROVER_assert (last == PLATFORM && calls == 1, "module_platform_id -> platform module");
      WITNESS_IF (1);
    }
  else
    {
      __CPROVER_assert (last == ADDOPT && calls == 1, "other -> dnf_conf_add_setopt");
      WITNESS_IF (strcmp (name, "excludepkgs") == 0);
    }

  g_free (value);
  __CPROVER_assert (model_live == 0, "no leak in the callback");
  return 0;
}
