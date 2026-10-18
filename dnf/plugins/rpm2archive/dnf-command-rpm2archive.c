/* dnf-command-rpm2archive.c
 *
 * Microdnf plugin implementation for rpm2archive.
 */

#include "dnf-command-rpm2archive.h"
#include "rpm2archive.h"

struct _DnfCommandRpm2archive
{
  PeasExtensionBase parent_instance;
};

static void dnf_command_rpm2archive_iface_init (DnfCommandInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (DnfCommandRpm2archive,
                                dnf_command_rpm2archive,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_IMPLEMENT_INTERFACE (DNF_TYPE_COMMAND,
                                                       dnf_command_rpm2archive_iface_init))

static void
dnf_command_rpm2archive_init (DnfCommandRpm2archive *self)
{
}

static gboolean
dnf_command_rpm2archive_run (DnfCommand      *cmd,
                             int              argc,
                             char            *argv[],
                             GOptionContext  *opt_ctx,
                             DnfContext      *ctx,
                             GError         **error)
{
  int rc = run_rpm2archive (argc, argv, error);
  return (rc == 0);
}

static void
dnf_command_rpm2archive_class_init (DnfCommandRpm2archiveClass *klass)
{
}

static void
dnf_command_rpm2archive_iface_init (DnfCommandInterface *iface)
{
  iface->run = dnf_command_rpm2archive_run;
}

static void
dnf_command_rpm2archive_class_finalize (DnfCommandRpm2archiveClass *klass)
{
}

G_MODULE_EXPORT void
dnf_command_rpm2archive_register_types (PeasObjectModule *module)
{
  dnf_command_rpm2archive_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              DNF_TYPE_COMMAND,
                                              DNF_TYPE_COMMAND_RPM2ARCHIVE);
}
