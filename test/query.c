/*
 *  OpenSlide, a library for reading whole slide image files
 *
 *  Copyright (c) 2012-2013 Carnegie Mellon University
 *  All rights reserved.
 *
 *  OpenSlide is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, version 2.1.
 *
 *  OpenSlide is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with OpenSlide. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <glib.h>
#include "openremoteslide.h"

static gboolean query_vendor = FALSE;

static GOptionEntry options[] = {
  {"vendor", 'n', 0, G_OPTION_ARG_NONE, &query_vendor,
   "Report format vendor", NULL},
  {NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL}
};

int main(int argc, char **argv) {
  GError *tmp_err = NULL;

  // Parse arguments
  GOptionContext *ctx =
    g_option_context_new("SLIDE - retrieve information about a slide file");
  g_option_context_add_main_entries(ctx, options, NULL);
  if (!g_option_context_parse(ctx, &argc, &argv, &tmp_err)) {
    fprintf(stderr, "%s\n", tmp_err->message);
    g_clear_error(&tmp_err);
    g_option_context_free(ctx);
    return 2;
  }
  g_option_context_free(ctx);
  if (argc != 2) {
    fprintf(stderr, "No slide specified\n");
    return 2;
  }
  const char *filename = argv[1];

  // Query vendor
  if (query_vendor) {
    const char *vendor = openremoteslide_detect_vendor(filename);
    printf("%s\n", vendor ? vendor : "");
  }

  return 0;
}
