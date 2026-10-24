/* dnf/lock.h
 *
 * Header for lockfile generator.
 */

#pragma once

#include <glib.h>
#include <libdnf/libdnf.h>

G_BEGIN_DECLS

gboolean run_lock (int argc, char **argv, GOptionContext *opt_ctx, DnfContext *ctx, GError **error);

G_END_DECLS
