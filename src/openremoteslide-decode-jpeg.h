/*
 *  OpenSlide, a library for reading whole slide image files
 *
 *  Copyright (c) 2007-2014 Carnegie Mellon University
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

#ifndef OPENREMOTESLIDE_OPENREMOTESLIDE_DECODE_JPEG_H_
#define OPENREMOTESLIDE_OPENREMOTESLIDE_DECODE_JPEG_H_

// jconfig.h redefines HAVE_STDLIB_H if libjpeg was not built with Autoconf
#undef HAVE_STDLIB_H
#include <jpeglib.h>
#undef HAVE_STDLIB_H
#include <config.h>  // fix damage

#include <stdio.h>
#include <stdint.h>
#include <glib.h>
#include <setjmp.h>

bool _openremoteslide_jpeg_read_dimensions(const char *filename,
                                     int64_t offset,
                                     int32_t *w, int32_t *h,
                                     GError **err);

bool _openremoteslide_jpeg_decode_buffer_dimensions(const void *buf, uint32_t len,
                                              int32_t *w, int32_t *h,
                                              GError **err);

bool _openremoteslide_jpeg_read(const char *filename,
                          int64_t offset,
                          uint32_t *dest,
                          int32_t w, int32_t h,
                          GError **err);

bool _openremoteslide_jpeg_decode_buffer(const void *buf, uint32_t len,
                                   uint32_t *dest,
                                   int32_t w, int32_t h,
                                   GError **err);

bool _openremoteslide_jpeg_decode_buffer_gray(const void *buf, uint32_t len,
                                        uint8_t *dest,
                                        int32_t w, int32_t h,
                                        GError **err);

bool _openremoteslide_jpeg_add_associated_image(openremoteslide_t *osr,
                                          const char *name,
                                          const char *filename,
                                          int64_t offset,
                                          GError **err);

/*
 * On Windows, we cannot fopen a file and pass it to another DLL that does fread.
 * So we need to compile all our freading into the OpenSlide DLL directly.
 */
void _openremoteslide_jpeg_stdio_src(j_decompress_ptr cinfo, URLIO_FILE *infile);

/*
 * Some libjpegs don't provide mem_src, so we have our own copy.
 */
void _openremoteslide_jpeg_mem_src (j_decompress_ptr cinfo,
                              unsigned char *inbuffer, unsigned long insize);


/*
 * Low-level JPEG decoding mechanism
 */
struct _openremoteslide_jpeg_decompress *_openremoteslide_jpeg_decompress_create(struct jpeg_decompress_struct **out_cinfo);

void _openremoteslide_jpeg_decompress_init(struct _openremoteslide_jpeg_decompress *dc,
                                     jmp_buf *env);

bool _openremoteslide_jpeg_decompress_run(struct _openremoteslide_jpeg_decompress *dc,
                                    // uint8_t * if grayscale, else uint32_t *
                                    void *dest,
                                    bool grayscale,
                                    int32_t w, int32_t h,
                                    GError **err);

void _openremoteslide_jpeg_propagate_error(GError **err,
                                     struct _openremoteslide_jpeg_decompress *dc);

void _openremoteslide_jpeg_decompress_destroy(struct _openremoteslide_jpeg_decompress *dc);

#endif
