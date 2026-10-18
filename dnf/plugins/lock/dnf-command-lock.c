/* dnf-command-lock.c
 *
 * Microdnf plugin implementation for lock command.
 */

#include "dnf-command-lock.h"
#include "lock.h"

struct _DnfCommandLock
{
  PeasExtensionBase parent_instance;
};

static void dnf_command_lock_iface_init (DnfCommandInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (DnfCommandLock,
                                dnf_command_lock,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_IMPLEMENT_INTERFACE (DNF_TYPE_COMMAND,
                                                       dnf_command_lock_iface_init))

static void
dnf_command_lock_init (DnfCommandLock *self)
{
}

static gboolean
dnf_command_lock_run (DnfCommand      *cmd,
                      int              argc,
                      char            *argv[],
                      GOptionContext  *opt_ctx,
                      DnfContext      *ctx,
                      GError         **error)
{
  return run_lock (argc, argv, opt_ctx, ctx, error);
}

static void
dnf_command_lock_class_init (DnfCommandLockClass *klass)
{
}

static void
dnf_command_lock_iface_init (DnfCommandInterface *iface)
{
  iface->run = dnf_command_lock_run;
}

static void
dnf_command_lock_class_finalize (DnfCommandLockClass *klass)
{
}

G_MODULE_EXPORT void
dnf_command_lock_register_types (PeasObjectModule *module)
{
  dnf_command_lock_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              DNF_TYPE_COMMAND,
                                              DNF_TYPE_COMMAND_LOCK);
}
