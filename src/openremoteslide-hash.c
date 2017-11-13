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

#include <config.h>

#include "openremoteslide-private.h"

#include "openremoteslide-hash.h"

#include <stdio.h>
#include <string.h>
#include <glib.h>

struct _openremoteslide_hash {
  GChecksum *checksum;
  bool enabled;
};

struct _openremoteslide_hash *_openremoteslide_hash_quickhash1_create(void) {
  struct _openremoteslide_hash *hash = g_slice_new(struct _openremoteslide_hash);
  hash->checksum = g_checksum_new(G_CHECKSUM_SHA256);
  hash->enabled = true;

  return hash;
}

void _openremoteslide_hash_data(struct _openremoteslide_hash *hash, const void *data,
                          int32_t datalen) {
  if (hash && hash->enabled && data && datalen) {
    g_checksum_update(hash->checksum, data, datalen);
  }
}

void _openremoteslide_hash_string(struct _openremoteslide_hash *hash, const char *str) {
  const char *str_to_hash = str ? str : "";
  _openremoteslide_hash_data(hash, str_to_hash, strlen(str_to_hash) + 1);
}

bool _openremoteslide_hash_file(struct _openremoteslide_hash *hash, const char *filename,
                          GError **err) {
  return _openremoteslide_hash_file_part(hash, filename, 0, -1, err);
}

bool _openremoteslide_hash_file_part(struct _openremoteslide_hash *hash,
			       const char *filename,
			       int64_t offset, int64_t size,
			       GError **err) {
  bool success = false;

  URLIO_FILE *f = _openremoteslide_fopen(filename, "rb", err);
  if (f == NULL) {
    return false;
  }

  if (size == -1) {
    // hash to end of file
    if (urlio_fseek(f, 0, SEEK_END)) {
      _openremoteslide_io_error(err, "Couldn't seek %s", filename);
      goto DONE;
    }
    int64_t len = urlio_ftell(f);
    if (len == -1) {
      _openremoteslide_io_error(err, "Couldn't get size of %s", filename);
      goto DONE;
    }
    size = len - offset;
  }

  uint8_t buf[4096];

  if (urlio_fseek(f, offset, SEEK_SET) == -1) {
    _openremoteslide_io_error(err, "Can't seek in %s", filename);
    goto DONE;
  }

  int64_t bytes_left = size;
  while (bytes_left > 0) {
    int64_t bytes_to_read = MIN((int64_t) sizeof buf, bytes_left);
    int64_t bytes_read = urlio_fread(buf, 1, bytes_to_read, f);

    if (bytes_read != bytes_to_read) {
      g_set_error(err, OPENREMOTESLIDE_ERROR, OPENREMOTESLIDE_ERROR_FAILED,
                  "Can't read from %s", filename);
      goto DONE;
    }

    //    g_debug("hash '%s' %"PRId64" %d", filename, offset + (size - bytes_left), bytes_to_read);

    bytes_left -= bytes_read;

    _openremoteslide_hash_data(hash, buf, bytes_read);
  }

  success = true;

DONE:
  urlio_fclose(f);
  return success;
}

// Invalidate this hash.  Use if this slide is unhashable for some reason.
void _openremoteslide_hash_disable(struct _openremoteslide_hash *hash) {
  if (hash) {
    hash->enabled = false;
  }
}

const char *_openremoteslide_hash_get_string(struct _openremoteslide_hash *hash) {
  if (hash->enabled) {
    return g_checksum_get_string(hash->checksum);
  } else {
    return NULL;
  }
}

void _openremoteslide_hash_destroy(struct _openremoteslide_hash *hash) {
  g_checksum_free(hash->checksum);
  g_slice_free(struct _openremoteslide_hash, hash);
}
