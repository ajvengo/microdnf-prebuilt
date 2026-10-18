/* dnf-command-rpm2archive.h */

#pragma once

#include "dnf-command.h"
#include <libpeas/peas.h>

G_BEGIN_DECLS

#define DNF_TYPE_COMMAND_RPM2ARCHIVE dnf_command_rpm2archive_get_type ()
G_DECLARE_FINAL_TYPE (DnfCommandRpm2archive, dnf_command_rpm2archive, DNF, COMMAND_RPM2ARCHIVE, PeasExtensionBase)

G_MODULE_EXPORT void dnf_command_rpm2archive_register_types (PeasObjectModule *module);

G_END_DECLS
