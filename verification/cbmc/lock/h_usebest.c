/* lock_use_best (dnf/lock.c): best/nobest decision.
 * Spec (e2e test_lock_features.sh best section and the function comment): --nobest -> FALSE;
 * --best -> TRUE; neither -> libdnf's value when "best" was set explicitly (priority above
 * DEFAULT: config file / --setopt), else TRUE (lockfile pins the newest candidate).
 * dnf_conf_main_get_option / dnf_context_get_best are stubs, see TRUSTED.md.
 */
#include "glib_model.h"

enum DnfConfPriority
{
  DNF_CONF_EMPTY = 0,
  DNF_CONF_DEFAULT = 10,
  DNF_CONF_MAINCONFIG = 20,
  DNF_CONF_AUTOMATICCONFIG = 30,
  DNF_CONF_REPOCONFIG = 40,
  DNF_CONF_PLUGINDEFAULT = 50,
  DNF_CONF_PLUGINCONFIG = 60,
  DNF_CONF_COMMANDLINE = 70,
  DNF_CONF_RUNTIME = 80
};

static int stub_has_val;
static enum DnfConfPriority stub_prio;
static gboolean stub_best;
static int get_option_calls;

static gchar *dnf_conf_main_get_option (const gchar *name, enum DnfConfPriority *priority, GError **error)
{
  MODEL_ASSERT (name != NULL && strcmp (name, "best") == 0, "asks for the best option");
  MODEL_ASSERT (priority != NULL, "priority out-pointer is valid");
  get_option_calls++;
  *priority = stub_prio;
  return stub_has_val ? g_strdup ("True") : NULL; /* caller owns the string */
}
static gboolean dnf_context_get_best (void) { return stub_best; }

#include "lock_use_best.inc"

#ifdef WITNESS
#define WITNESS_IF(c) __CPROVER_assert (!(c), "witness")
#else
#define WITNESS_IF(c) ((void)0)
#endif

int main (void)
{
  /* Globals are zero-initialised in CBMC, so the stub state must be assigned a nondeterministic
   * value explicitly (an `assume` on an untouched global would be vacuous). */
  gboolean opt_best, opt_nobest;
  int nondet_int (void);
  stub_has_val = nondet_int ();
  stub_best = nondet_int ();
  stub_prio = (enum DnfConfPriority)nondet_int ();
  __CPROVER_assume (opt_best == 0 || opt_best == 1);
  __CPROVER_assume (opt_nobest == 0 || opt_nobest == 1);
  __CPROVER_assume (stub_has_val == 0 || stub_has_val == 1);
  __CPROVER_assume (stub_best == 0 || stub_best == 1);
  __CPROVER_assume (stub_prio == DNF_CONF_EMPTY || stub_prio == DNF_CONF_DEFAULT || stub_prio == DNF_CONF_MAINCONFIG
                    || stub_prio == DNF_CONF_AUTOMATICCONFIG || stub_prio == DNF_CONF_REPOCONFIG
                    || stub_prio == DNF_CONF_PLUGINDEFAULT || stub_prio == DNF_CONF_PLUGINCONFIG
                    || stub_prio == DNF_CONF_COMMANDLINE || stub_prio == DNF_CONF_RUNTIME);

  gboolean r = lock_use_best (opt_best, opt_nobest);

  if (opt_nobest)
    __CPROVER_assert (r == FALSE, "--nobest wins");
  else if (opt_best)
    {
      __CPROVER_assert (r == TRUE, "--best");
      __CPROVER_assert (get_option_calls == 0, "flags short-circuit the config lookup");
    }
  else if (stub_has_val && stub_prio > DNF_CONF_DEFAULT)
    __CPROVER_assert (r == stub_best, "explicit config value is honoured");
  else
    __CPROVER_assert (r == TRUE, "default is best");
  __CPROVER_assert (r == TRUE || r == FALSE, "boolean result");
  WITNESS_IF (!opt_nobest && !opt_best && stub_has_val && stub_prio > DNF_CONF_DEFAULT && !stub_best);
  __CPROVER_assert (model_live == 0, "no leak of the option string");
  return 0;
}
