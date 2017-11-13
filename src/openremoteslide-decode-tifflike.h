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

#ifndef OPENREMOTESLIDE_OPENREMOTESLIDE_DECODE_TIFFLIKE_H_
#define OPENREMOTESLIDE_OPENREMOTESLIDE_DECODE_TIFFLIKE_H_

#include "openremoteslide-private.h"
#include "openremoteslide-hash.h"

#include <stdio.h>
#include <stdint.h>
#include <glib.h>

/* TIFF container support (for formats violating the TIFF spec) */
/* Thread-safe. */

struct _openremoteslide_tifflike *_openremoteslide_tifflike_create(const char *filename,
                                                       GError **err);

void _openremoteslide_tifflike_destroy(struct _openremoteslide_tifflike *tl);

bool _openremoteslide_tifflike_init_properties_and_hash(openremoteslide_t *osr,
                                                  struct _openremoteslide_tifflike *tl,
                                                  struct _openremoteslide_hash *quickhash1,
                                                  int32_t lowest_resolution_level,
                                                  int32_t property_dir,
                                                  GError **err);

// helpful printout?
void _openremoteslide_tifflike_print(struct _openremoteslide_tifflike *tl);

int64_t _openremoteslide_tifflike_get_directory_count(struct _openremoteslide_tifflike *tl);

int64_t _openremoteslide_tifflike_get_value_count(struct _openremoteslide_tifflike *tl,
                                            int64_t dir, int32_t tag);

// accessors
// element accessor returns first element only
// array accessor returns pointer to array of elements; do not free

// TIFF_BYTE, TIFF_SHORT, TIFF_LONG, TIFF_IFD
uint64_t _openremoteslide_tifflike_get_uint(struct _openremoteslide_tifflike *tl,
                                      int64_t dir, int32_t tag,
                                      GError **err);

const uint64_t *_openremoteslide_tifflike_get_uints(struct _openremoteslide_tifflike *tl,
                                              int64_t dir, int32_t tag,
                                              GError **err);

// if the file was detected as NDPI, heuristically add high-order bits to
// the specified offset
uint64_t _openremoteslide_tifflike_uint_fix_offset_ndpi(struct _openremoteslide_tifflike *tl,
                                                  int64_t dir, uint64_t offset);

// TIFF_SBYTE, TIFF_SSHORT, TIFF_SLONG
int64_t _openremoteslide_tifflike_get_sint(struct _openremoteslide_tifflike *tl,
                                     int64_t dir, int32_t tag,
                                     GError **err);

const int64_t *_openremoteslide_tifflike_get_sints(struct _openremoteslide_tifflike *tl,
                                             int64_t dir, int32_t tag,
                                             GError **err);


// TIFF_FLOAT, TIFF_DOUBLE, TIFF_RATIONAL, TIFF_SRATIONAL
double _openremoteslide_tifflike_get_float(struct _openremoteslide_tifflike *tl,
                                     int64_t dir, int32_t tag,
                                     GError **err);

const double *_openremoteslide_tifflike_get_floats(struct _openremoteslide_tifflike *tl,
                                             int64_t dir, int32_t tag,
                                             GError **err);


// TIFF_ASCII, TIFF_BYTE, TIFF_UNDEFINED
// guaranteed to be null-terminated
const void *_openremoteslide_tifflike_get_buffer(struct _openremoteslide_tifflike *tl,
                                           int64_t dir, int32_t tag,
                                           GError **err);

// return true if directory is tiled
bool _openremoteslide_tifflike_is_tiled(struct _openremoteslide_tifflike *tl,
                                  int64_t dir);

#endif
