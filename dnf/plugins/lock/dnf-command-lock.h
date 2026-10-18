/* dnf-command-lock.h */

#pragma once

#include "dnf-command.h"
#include <libpeas/peas.h>

G_BEGIN_DECLS

#define DNF_TYPE_COMMAND_LOCK dnf_command_lock_get_type ()
G_DECLARE_FINAL_TYPE (DnfCommandLock, dnf_command_lock, DNF, COMMAND_LOCK, PeasExtensionBase)

G_MODULE_EXPORT void dnf_command_lock_register_types (PeasObjectModule *module);

G_END_DECLS
