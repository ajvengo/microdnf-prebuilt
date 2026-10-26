/* basearch_for_arch (dnf/lock-main.c): solve architecture -> $basearch.
 * Spec (e2e test_lock_features.sh basearch section, dnf docs): i686 -> i386, armv7hl -> armhfp,
 * every other arch (x86_64, aarch64, ppc64le, ...) maps to itself.
 */
#include "glib_model.h"
#include "basearch_for_arch.inc"

#ifndef L
#define L 7
#endif
#ifdef WITNESS
#define WITNESS_IF(c) __CPROVER_assert (!(c), "witness")
#else
#define WITNESS_IF(c) ((void)0)
#endif

int main (void)
{
  /* concrete cases first (oracle taken from the dnf basearch table) */
  __CPROVER_assert (strcmp (basearch_for_arch ("i686"), "i386") == 0, "i686");
  __CPROVER_assert (strcmp (basearch_for_arch ("i386"), "i386") == 0, "i386");
  __CPROVER_assert (strcmp (basearch_for_arch ("i586"), "i386") == 0, "i586");
  __CPROVER_assert (strcmp (basearch_for_arch ("armv7hl"), "armhfp") == 0, "armv7hl");
  __CPROVER_assert (strcmp (basearch_for_arch ("x86_64"), "x86_64") == 0, "x86_64");
  __CPROVER_assert (strcmp (basearch_for_arch ("aarch64"), "aarch64") == 0, "aarch64");
  __CPROVER_assert (strcmp (basearch_for_arch ("ppc64le"), "ppc64le") == 0, "ppc64le");
  __CPROVER_assert (strcmp (basearch_for_arch ("noarch"), "noarch") == 0, "noarch");

  /* any non-NULL string, including "" */
  gchar *arch = nondet_str (L);
  const gchar *r = basearch_for_arch (arch);
  __CPROVER_assert (r != NULL, "never NULL");
  size_t n = strlen (arch);
  int is_i = n == 4 && arch[0] == 'i' && arch[2] == '8' && arch[3] == '6';
  int is_arm = n >= 6 && strncmp (arch, "armv", 4) == 0 && strcmp (arch + n - 2, "hl") == 0;
  if (is_i)
    __CPROVER_assert (strcmp (r, "i386") == 0, "i?86 -> i386");
  else if (is_arm)
    __CPROVER_assert (strcmp (r, "armhfp") == 0, "armv*hl -> armhfp");
  else
    __CPROVER_assert (r == arch, "identity otherwise");
  WITNESS_IF (is_arm);
  g_free (arch);
  __CPROVER_assert (model_live == 0, "no leak");
  return 0;
}
