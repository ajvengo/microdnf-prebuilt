/* dnf/rpm2archive-main.c
 *
 * Standalone entrypoint for rpm2archive.
 */

#include "rpm2archive.h"
#include <stdio.h>
#include <glib.h>

int
main (int argc, char *argv[])
{
  g_autoptr(GError) error = NULL;
  int rc = run_rpm2archive (argc, argv, &error);
  if (rc != 0 && error)
    {
      g_printerr ("rpm2archive error: %s\n", error->message);
    }
  return rc;
}
