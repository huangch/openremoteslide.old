/*
 *  OpenSlide, a library for reading whole slide image files
 *
 *  Copyright (c) 2007-2013 Carnegie Mellon University
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

#ifndef OPENREMOTESLIDE_OPENREMOTESLIDE_DECODE_TIFF_H_
#define OPENREMOTESLIDE_OPENREMOTESLIDE_DECODE_TIFF_H_

#include "openremoteslide-private.h"
#include "openremoteslide-hash.h"

#include <stdint.h>
#include <glib.h>
#include <tiffio.h>

struct _openremoteslide_tiff_level {
  tdir_t dir;
  int64_t image_w;
  int64_t image_h;
  int64_t tile_w;
  int64_t tile_h;
  int64_t tiles_across;
  int64_t tiles_down;

  bool tile_read_direct;
  gint warned_read_indirect;
  uint16_t photometric;
};

struct _openremoteslide_tiffcache;

bool _openremoteslide_tiff_level_init(TIFF *tiff,
                                tdir_t dir,
                                struct _openremoteslide_level *level,
                                struct _openremoteslide_tiff_level *tiffl,
                                GError **err);

bool _openremoteslide_tiff_check_missing_tile(struct _openremoteslide_tiff_level *tiffl,
                                        TIFF *tiff,
                                        int64_t tile_col, int64_t tile_row,
                                        bool *is_missing,
                                        GError **err);

bool _openremoteslide_tiff_read_tile(struct _openremoteslide_tiff_level *tiffl,
                               TIFF *tiff,
                               uint32_t *dest,
                               int64_t tile_col, int64_t tile_row,
                               GError **err);

bool _openremoteslide_tiff_read_tile_data(struct _openremoteslide_tiff_level *tiffl,
                                    TIFF *tiff,
                                    void **buf, int32_t *len,
                                    int64_t tile_col, int64_t tile_row,
                                    GError **err);

bool _openremoteslide_tiff_clip_tile(struct _openremoteslide_tiff_level *tiffl,
                               uint32_t *tiledata,
                               int64_t tile_col, int64_t tile_row,
                               GError **err);

bool _openremoteslide_tiff_add_associated_image(openremoteslide_t *osr,
                                          const char *name,
                                          struct _openremoteslide_tiffcache *tc,
                                          tdir_t dir,
                                          GError **err);

bool _openremoteslide_tiff_set_dir(TIFF *tiff,
                             tdir_t dir,
                             GError **err);


/* TIFF handles are not thread-safe, so we have a handle cache for
   multithreaded access */
struct _openremoteslide_tiffcache *_openremoteslide_tiffcache_create(const char *filename);

TIFF *_openremoteslide_tiffcache_get(struct _openremoteslide_tiffcache *tc, GError **err);

void _openremoteslide_tiffcache_put(struct _openremoteslide_tiffcache *tc, TIFF *tiff);

void _openremoteslide_tiffcache_destroy(struct _openremoteslide_tiffcache *tc);

#endif
