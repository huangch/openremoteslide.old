/*
 *  OpenSlide, a library for reading whole slide image files
 *
 *  Copyright (c) 2007-2010 Carnegie Mellon University
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

#ifndef OPENREMOTESLIDE_OPENREMOTESLIDE_HASH_H_
#define OPENREMOTESLIDE_OPENREMOTESLIDE_HASH_H_

#include <config.h>

#include "openremoteslide.h"

#include <stdbool.h>
#include <stdint.h>
#include <tiffio.h>
#include <glib.h>

struct _openremoteslide_hash;

// constructor
struct _openremoteslide_hash *_openremoteslide_hash_quickhash1_create(void);

// hashers
void _openremoteslide_hash_data(struct _openremoteslide_hash *hash, const void *data,
                          int32_t datalen);
void _openremoteslide_hash_string(struct _openremoteslide_hash *hash, const char *str);
bool _openremoteslide_hash_file(struct _openremoteslide_hash *hash, const char *filename,
                          GError **err);
bool _openremoteslide_hash_file_part(struct _openremoteslide_hash *hash,
			       const char *filename,
			       int64_t offset, int64_t size,
			       GError **err);

// lockout
void _openremoteslide_hash_disable(struct _openremoteslide_hash *hash);

// accessor
const char *_openremoteslide_hash_get_string(struct _openremoteslide_hash *hash);

// destructor
void _openremoteslide_hash_destroy(struct _openremoteslide_hash *hash);

#endif
