/* dnf/rpm2archive.h
 *
 * Header for rpm2archive library and plugin integration.
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

int run_rpm2archive (int argc, char **argv, GError **error);

G_END_DECLS
