/*
 *  OpenSlide, a library for reading whole slide image files
 *
 *  Copyright (c) 2007-2015 Carnegie Mellon University
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
#include "openremoteslide-url.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <glib.h>
#include <cairo.h>

// #include <sys/syscall.h>
#define gettidv1() syscall(__NR_gettid)
#define gettidv2() syscall(SYS_gettid)

#undef HAVE_FCNTL

#ifdef HAVE_FCNTL
#include <unistd.h>
#include <fcntl.h>
#endif

#define KEY_FILE_HARD_MAX_SIZE (100 << 20)

static const char DEBUG_ENV_VAR[] = "OPENREMOTESLIDE_DEBUG";

static const struct debug_option {
  const char *kw;
  enum _openremoteslide_debug_flag flag;
  const char *desc;
} debug_options[] = {
  {"detection", OPENREMOTESLIDE_DEBUG_DETECTION, "log format detection errors"},
  {"jpeg-markers", OPENREMOTESLIDE_DEBUG_JPEG_MARKERS,
   "verify Hamamatsu restart markers"},
  {"performance", OPENREMOTESLIDE_DEBUG_PERFORMANCE,
   "log conditions causing poor performance"},
  {"tiles", OPENREMOTESLIDE_DEBUG_TILES, "render tile outlines"},
  {NULL, 0, NULL}
};

static uint32_t debug_flags;


guint _openremoteslide_int64_hash(gconstpointer v) {
  int64_t i = *((const int64_t *) v);
  return i ^ (i >> 32);
}

gboolean _openremoteslide_int64_equal(gconstpointer v1, gconstpointer v2) {
  return *((int64_t *) v1) == *((int64_t *) v2);
}

void _openremoteslide_int64_free(gpointer data) {
  g_slice_free(int64_t, data);
}

bool _openremoteslide_read_key_file(GKeyFile *key_file, const char *filename,
                              int32_t max_size, GKeyFileFlags flags,
                              GError **err) {
  char *buf = NULL;

  /* We load the whole key file into memory and parse it with
   * g_key_file_load_from_data instead of using g_key_file_load_from_file
   * because the load_from_file function incorrectly parses a value when
   * the terminating '\r\n' falls across a 4KB boundary.
   * https://bugzilla.redhat.com/show_bug.cgi?id=649936 */

  /* this also allows us to skip a UTF-8 BOM which the g_key_file parser
   * does not expect to find. */

  /* Hamamatsu attempts to load the slide file as a key file.  We impose
     a maximum file size to avoid loading an entire slide into RAM. */

  // hard limit
  if (max_size <= 0) {
    max_size = KEY_FILE_HARD_MAX_SIZE;
  }
  max_size = MIN(max_size, KEY_FILE_HARD_MAX_SIZE);

  URLIO_FILE *f = _openremoteslide_fopen(filename, "rb", err);
  if (f == NULL) {
    return false;
  }

  // get file size and check against maximum
  if (urlio_fseek(f, 0, SEEK_END)) {
    _openremoteslide_io_error(err, "Couldn't seek %s", filename);
    goto FAIL;
  }
  int64_t size = urlio_ftell(f);
  if (size == -1) {
    _openremoteslide_io_error(err, "Couldn't get size of %s", filename);
    goto FAIL;
  }
  if (size == 0) {
    // glib < 2.32 logs a critical error when parsing a zero-length key file
    g_set_error(err, OPENREMOTESLIDE_ERROR, OPENREMOTESLIDE_ERROR_FAILED,
                "Key file %s is empty", filename);
    goto FAIL;
  }
  if (size > max_size) {
    g_set_error(err, G_FILE_ERROR, G_FILE_ERROR_NOMEM,
                "Key file %s too large", filename);
    goto FAIL;
  }

  // read
  if (urlio_fseek(f, 0, SEEK_SET)) {
    _openremoteslide_io_error(err, "Couldn't seek %s", filename);
    goto FAIL;
  }
  // catch file size changes
  buf = g_malloc(size + 1);
  int64_t total = 0;
  size_t cur_len;
  while ((cur_len = urlio_fread(buf + total, 1, size + 1 - total, f)) > 0) {
    total += cur_len;
  }
  if (urlio_ferror(f) || total != size) {
    _openremoteslide_io_error(err, "Couldn't read key file %s", filename);
    goto FAIL;
  }

  /* skip the UTF-8 BOM if it is present. */
  int offset = 0;
  if (size >= 3 && memcmp(buf, "\xef\xbb\xbf", 3) == 0) {
    offset = 3;
  }

  bool result = g_key_file_load_from_data(key_file,
                                          buf + offset, size - offset,
                                          flags, err);
  g_free(buf);
  urlio_fclose(f);
  return result;

FAIL:
  g_free(buf);
  urlio_fclose(f);
  return false;
}

// #undef urlio_fopen
URLIO_FILE *_openremoteslide_fopen(const char *path, const char *mode, GError **err)
{
  char *m = g_strconcat(mode, FOPEN_CLOEXEC_FLAG, NULL);
  URLIO_FILE *f = urlio_fopen(path, m);
  g_free(m);

  if (f == NULL) {
    _openremoteslide_io_error(err, "Couldn't open %s", path);
    return NULL;
  }

  /* Unnecessary if FOPEN_CLOEXEC_FLAG is non-empty.  Not built on Windows. */
/*
#ifdef HAVE_FCNTL
  if (!FOPEN_CLOEXEC_FLAG[0]) {
    int fd = fileno(f);
    if (fd == -1) {
      _openremoteslide_io_error(err, "Couldn't fileno() %s", path);
      urlio_fclose(f);
      return NULL;
    }
    long flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
      _openremoteslide_io_error(err, "Couldn't F_GETFD %s", path);
      urlio_fclose(f);
      return NULL;
    }
    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC)) {
      _openremoteslide_io_error(err, "Couldn't F_SETFD %s", path);
      urlio_fclose(f);
      return NULL;
    }
  }
#endif
*/

  return f;
}
// #define urlio_fopen _OPENREMOTESLIDE_POISON(_openremoteslide_fopen)

#undef g_ascii_strtod
double _openremoteslide_parse_double(const char *value) {
  // Canonicalize comma to decimal point, since the locale of the
  // originating system sometimes leaks into slide files.
  // This will break if the value includes grouping characters.
  char *canonical = g_strdup(value);
  g_strdelimit(canonical, ",", '.');

  char *endptr;
  errno = 0;
  double result = g_ascii_strtod(canonical, &endptr);
  // fail on overflow/underflow
  if (canonical[0] == 0 || endptr[0] != 0 || errno == ERANGE) {
    result = NAN;
  }

  g_free(canonical);
  return result;
}
#define g_ascii_strtod _OPENREMOTESLIDE_POISON(_openremoteslide_parse_double)

char *_openremoteslide_format_double(double d) {
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr(buf, sizeof buf, d);
  return g_strdup(buf);
}

// if the src prop is an int, canonicalize it and copy it to dest
void _openremoteslide_duplicate_int_prop(openremoteslide_t *osr, const char *src,
                                   const char *dest) {
  g_return_if_fail(g_hash_table_lookup(osr->properties, dest) == NULL);

  char *value = g_hash_table_lookup(osr->properties, src);
  if (value && value[0]) {
    char *endptr;
    int64_t result = g_ascii_strtoll(value, &endptr, 10);
    if (endptr[0] == 0) {
      g_hash_table_insert(osr->properties,
                          g_strdup(dest),
                          g_strdup_printf("%"PRId64, result));
    }
  }
}

// if the src prop is a double, canonicalize it and copy it to dest
void _openremoteslide_duplicate_double_prop(openremoteslide_t *osr, const char *src,
                                      const char *dest) {
  g_return_if_fail(g_hash_table_lookup(osr->properties, dest) == NULL);

  char *value = g_hash_table_lookup(osr->properties, src);
  if (value) {
    double result = _openremoteslide_parse_double(value);
    if (!isnan(result)) {
      g_hash_table_insert(osr->properties, g_strdup(dest),
                          _openremoteslide_format_double(result));
    }
  }
}

void _openremoteslide_set_background_color_prop(openremoteslide_t *osr,
                                          uint8_t r, uint8_t g, uint8_t b) {
  g_return_if_fail(g_hash_table_lookup(osr->properties,
                                       OPENREMOTESLIDE_PROPERTY_NAME_BACKGROUND_COLOR) == NULL);

  g_hash_table_insert(osr->properties,
                      g_strdup(OPENREMOTESLIDE_PROPERTY_NAME_BACKGROUND_COLOR),
                      g_strdup_printf("%.02X%.02X%.02X", r, g, b));
}

void _openremoteslide_set_bounds_props_from_grid(openremoteslide_t *osr,
                                           struct _openremoteslide_grid *grid) {
  g_return_if_fail(g_hash_table_lookup(osr->properties,
                                       OPENREMOTESLIDE_PROPERTY_NAME_BOUNDS_X) == NULL);

  double x, y, w, h;
  _openremoteslide_grid_get_bounds(grid, &x, &y, &w, &h);

  g_hash_table_insert(osr->properties,
                      g_strdup(OPENREMOTESLIDE_PROPERTY_NAME_BOUNDS_X),
                      g_strdup_printf("%"PRId64,
                                      (int64_t) floor(x)));
  g_hash_table_insert(osr->properties,
                      g_strdup(OPENREMOTESLIDE_PROPERTY_NAME_BOUNDS_Y),
                      g_strdup_printf("%"PRId64,
                                      (int64_t) floor(y)));
  g_hash_table_insert(osr->properties,
                      g_strdup(OPENREMOTESLIDE_PROPERTY_NAME_BOUNDS_WIDTH),
                      g_strdup_printf("%"PRId64,
                                      (int64_t) (ceil(x + w) - floor(x))));
  g_hash_table_insert(osr->properties,
                      g_strdup(OPENREMOTESLIDE_PROPERTY_NAME_BOUNDS_HEIGHT),
                      g_strdup_printf("%"PRId64,
                                      (int64_t) (ceil(y + h) - floor(y))));
}

bool _openremoteslide_clip_tile(uint32_t *tiledata,
                          int64_t tile_w, int64_t tile_h,
                          int64_t clip_w, int64_t clip_h,
                          GError **err) {
  if (clip_w >= tile_w && clip_h >= tile_h) {
    return true;
  }

  cairo_surface_t *surface =
    cairo_image_surface_create_for_data((unsigned char *) tiledata,
                                        CAIRO_FORMAT_ARGB32,
                                        tile_w, tile_h,
                                        tile_w * 4);
  cairo_t *cr = cairo_create(surface);
  cairo_surface_destroy(surface);

  cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);

  cairo_rectangle(cr, clip_w, 0, tile_w - clip_w, tile_h);
  cairo_fill(cr);

  cairo_rectangle(cr, 0, clip_h, tile_w, tile_h - clip_h);
  cairo_fill(cr);

  bool success = _openremoteslide_check_cairo_status(cr, err);
  cairo_destroy(cr);

  return success;
}

// note: g_getenv() is not reentrant
void _openremoteslide_debug_init(void) {
  const char *debug_str = g_getenv(DEBUG_ENV_VAR);
  if (!debug_str) {
    return;
  }

  char **keywords = g_strsplit(debug_str, ",", 0);
  bool printed_help = false;
  for (char **kw = keywords; *kw; kw++) {
    g_strstrip(*kw);
    bool found = false;
    for (const struct debug_option *opt = debug_options; opt->kw; opt++) {
      if (!g_ascii_strcasecmp(*kw, opt->kw)) {
        debug_flags |= 1 << opt->flag;
        found = true;
        break;
      }
    }
    if (!found && !printed_help) {
      printed_help = true;
      g_message("%s options (comma-delimited):", DEBUG_ENV_VAR);
      for (const struct debug_option *opt = debug_options; opt->kw; opt++) {
        g_message("   %-15s - %s", opt->kw, opt->desc);
      }
    }
  }
  g_strfreev(keywords);
}

bool _openremoteslide_debug(enum _openremoteslide_debug_flag flag) {
  return !!(debug_flags & (1 << flag));
}

void _openremoteslide_performance_warn_once(gint *warned_flag,
                                      const char *str, ...) {
  if (_openremoteslide_debug(OPENREMOTESLIDE_DEBUG_PERFORMANCE)) {
    if (warned_flag == NULL ||
        g_atomic_int_compare_and_exchange(warned_flag, 0, 1)) {
      va_list ap;
      va_start(ap, str);
      g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, str, ap);
      va_end(ap);
    }
  }
}

#undef fopen
#undef fseek
#undef ftell

/* we use a global one for convenience */
// CURLM *multi_handle;

/* curl calls this routine to get more data */
static size_t write_callback(char *buffer, size_t size, size_t nitems,
		void *userp) {
	char *newbuff;
	size_t rembuff;

	URLIO_FILE *url = (URLIO_FILE *) userp;
	size *= nitems;

	rembuff = url->buffer_len - url->buffer_pos; /* remaining space in buffer */

	if (size > rembuff) {
		/* not enough space in buffer */
		newbuff = (char*)realloc(url->buffer, url->buffer_len + (size - rembuff));
		if (newbuff == NULL) {
			fprintf(stderr, "callback buffer grow failed\n");
			size = rembuff;
		} else {
			/* realloc succeeded increase buffer size*/
			url->buffer_len += size - rembuff;
			url->buffer = newbuff;
		}
	}

	memcpy(&url->buffer[url->buffer_pos], buffer, size);
	url->buffer_pos += size;

	return size;
}

/* use to attempt to fill the read buffer up to requested number of bytes */
static int fill_buffer(URLIO_FILE *file, size_t want) {
	fd_set fdread;
	fd_set fdwrite;
	fd_set fdexcep;
	struct timeval timeout;
	int rc;
	CURLMcode mc; /* curl_multi_fdset() return code */

	/* only attempt to fill buffer if transactions still running and buffer
	 * doesn't exceed required size already
	 */
	if ((!file->still_running) || (file->buffer_pos > want))
		return 0;

	/* attempt to fill buffer */
	do {
		int maxfd = -1;
		long curl_timeo = -1;

		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		FD_ZERO(&fdexcep);

		/* set a suitable timeout to fail on */
		timeout.tv_sec = 60; /* 1 minute */
		timeout.tv_usec = 0;

		curl_multi_timeout(file->multi_handle, &curl_timeo);

		if (curl_timeo >= 0) {
			timeout.tv_sec = curl_timeo / 1000;
			if (timeout.tv_sec > 1)
				timeout.tv_sec = 1;
			else
				timeout.tv_usec = (curl_timeo % 1000) * 1000;
		}

		/* get file descriptors from the transfers */
		mc = curl_multi_fdset(file->multi_handle, &fdread, &fdwrite, &fdexcep,
				&maxfd);

		if (mc != CURLM_OK) {
			fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
			break;
		}

		/* On success the value of maxfd is guaranteed to be >= -1. We call
		 select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
		 no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
		 to sleep 100ms, which is the minimum suggested value in the
		 curl_multi_fdset() doc. */

		if (maxfd == -1) {
#ifdef _WIN32
			Sleep(100);
			rc = 0;
#else
			/* Portable sleep for platforms other than Windows. */
			struct timeval wait = { 0, 100 * 1000 }; /* 100ms */
			rc = select(0, NULL, NULL, NULL, &wait);
#endif
		} else {
			/* Note that on some platforms 'timeout' may be modified by select().
			 If you need access to the original value save a copy beforehand. */
			rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
		}

		switch (rc) {
		case -1:
			/* select error */
			break;

		case 0:
		default:
			/* timeout or readable/writable sockets */

			curl_multi_perform(file->multi_handle, &file->still_running);
			break;
		}
	} while (file->still_running && (file->buffer_pos < want));
	return 1;
}

/* use to attempt to fill the read buffer up to requested number of bytes */
static int fill_buffer_thread(FCURL_DATA *data, size_t want) {
	fd_set fdread;
	fd_set fdwrite;
	fd_set fdexcep;
	struct timeval timeout;
	int rc;
	CURLMcode mc; /* curl_multi_fdset() return code */

	/* only attempt to fill buffer if transactions still running and buffer
	 * doesn't exceed required size already
	 */
	if ((!data->still_running) || (data->buffer_pos > want))
		return 0;

	/* attempt to fill buffer */
	do {
		int maxfd = -1;
		long curl_timeo = -1;

		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		FD_ZERO(&fdexcep);

		/* set a suitable timeout to fail on */
		timeout.tv_sec = 60; /* 1 minute */
		timeout.tv_usec = 0;

		curl_multi_timeout(data->multi_handle, &curl_timeo);

		if (curl_timeo >= 0) {
			timeout.tv_sec = curl_timeo / 1000;
			if (timeout.tv_sec > 1)
				timeout.tv_sec = 1;
			else
				timeout.tv_usec = (curl_timeo % 1000) * 1000;
		}

		/* get file descriptors from the transfers */
		mc = curl_multi_fdset(data->multi_handle, &fdread, &fdwrite, &fdexcep,
				&maxfd);

		if (mc != CURLM_OK) {
			fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
			break;
		}

		/* On success the value of maxfd is guaranteed to be >= -1. We call
		 select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
		 no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
		 to sleep 100ms, which is the minimum suggested value in the
		 curl_multi_fdset() doc. */

		if (maxfd == -1) {
#ifdef _WIN32
			Sleep(100);
			rc = 0;
#else
			/* Portable sleep for platforms other than Windows. */
			struct timeval wait = { 0, 100 * 1000 }; /* 100ms */
			rc = select(0, NULL, NULL, NULL, &wait);
#endif
		} else {
			/* Note that on some platforms 'timeout' may be modified by select().
			 If you need access to the original value save a copy beforehand. */
			rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
		}

		switch (rc) {
		case -1:
			/* select error */
			break;

		case 0:
		default:
			/* timeout or readable/writable sockets */

			curl_multi_perform(data->multi_handle, &data->still_running);
			break;
		}
	} while (data->still_running && (data->buffer_pos < want));
	return 1;
}

/* use to remove want bytes from the front of a files buffer */
static int use_buffer(URLIO_FILE *file, size_t want) {
	/* sort out buffer */
	if ((file->buffer_pos - want) <= 0) {
		/* ditch buffer - write will recreate */
		free(file->buffer);
		file->buffer = NULL;
		file->buffer_pos = 0;
		file->buffer_len = 0;
	} else {
		/* move rest down make it available for later */
		memmove(file->buffer, &file->buffer[want], (file->buffer_pos - want));

		file->buffer_pos -= want;
	}
	return 0;
}

/* use to remove want bytes from the front of a files buffer */
static int use_buffer_thread(FCURL_DATA *data, size_t want) {
	/* sort out buffer */
	if ((data->buffer_pos - want) <= 0) {
		/* ditch buffer - write will recreate */
		free(data->buffer);
		data->buffer = NULL;
		data->buffer_pos = 0;
		data->buffer_len = 0;
	} else {
		/* move rest down make it available for later */
		memmove(data->buffer, &data->buffer[want], (data->buffer_pos - want));

		data->buffer_pos -= want;
	}
	return 0;
}

// static GThread *ghousekeepingthread = NULL;
// static GMutex cache_lock;
static URLIO_FILE **url_cache = NULL;
static long int url_cache_count = 0;
// static bool curl_global_inited = false;

//static void *housekeepingthread_old(void *data) {
//	for (;;) {
//		sleep(1);
//		// g_mutex_lock(&cache_lock);
//
//		for (int count = 0; count < url_cache_count; count++) {
//			if (url_cache[count]->close_flag) {
//				if(url_cache[count]->cacheLifeSpan > 0) url_cache[count]->cacheLifeSpan -= 1;
//			}
//			else {
//				url_cache[count]->cacheLifeSpan = MAX_SURVIVAL_TIME;
//			}
//
//			if (url_cache[count]->cacheLifeSpan == 0) {
//
//
//
//
//				curl_multi_remove_handle(url_cache[count]->multi_handle,
//						url_cache[count]->handle.curl);
//
//				/* cleanup */
//				curl_easy_cleanup(url_cache[count]->handle.curl);
//
//				/* clean up multithread */
//				for (int t = 0; t < THREAD_NUM; t ++) {
//					/* if still_running is 0 now, we should return NULL */
//
//					/* make sure the easy handle is not in the multi handle anymore */
//					curl_multi_remove_handle(url_cache[count]->fcurl_data[t]->multi_handle, url_cache[count]->fcurl_data[t]->handle.curl);
//
//					/* cleanup */
//					curl_easy_cleanup(url_cache[count]->fcurl_data[t]->handle.curl);
//
//					free(url_cache[count]->fcurl_data[t]->buffer);/* free any allocated buffer space */
//					free(url_cache[count]->fcurl_data[t]->url);
//					free(url_cache[count]->fcurl_data[t]->cache);
//					free(url_cache[count]->fcurl_data[t]);
//
//					url_cache[count]->fcurl_data[t] = NULL;
//				}
//
//				free(url_cache[count]->buffer);/* free any allocated buffer space */
//				free(url_cache[count]->url);
//				if (url_cache[count]->cache_count != 0) {
//					for (int i = 0; i < url_cache[count]->cache_count; i++)
//						free(url_cache[count]->cache_list[i]);
//					free(url_cache[count]->cache_list);
//					free(url_cache[count]->cache_id_list);
//				}
//
//				free(url_cache[count]);
//
//				url_cache[count] = NULL;
//
//				if (count != url_cache_count - 1)
//					memmove(&url_cache[count], &url_cache[count + 1],
//							(url_cache_count - count - 1) * sizeof(URLIO_FILE*));
//				if (url_cache_count != 1) {
//					url_cache = (URLIO_FILE**) realloc(url_cache,
//							(url_cache_count - 1) * sizeof(URLIO_FILE*));
//					url_cache_count--;
//				} else {
//					free(url_cache);
//					url_cache = NULL;
//					url_cache_count = 0;
//				}
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//			}
//		}
//
//
//		// if(url_cache_count == 0) {
//		// 	ghousekeepingthread = NULL;
//		// 	g_mutex_unlock(&cache_lock);
//		// 	break;
//		// }
//		// else {
//		// 	g_mutex_unlock(&cache_lock);
//		// }
//	}
//}


URLIO_FILE *urlio_fopen(const char *url, const char *operation) {
	/* this code could check for URLs or types in the 'url' and
	 basically use the real fopen() for standard files */
#ifdef URLIO_VERBOSE
	printf("fopen: %s\n", url);
#endif











	URLIO_FILE *file;

	FILE *f;







	f = fopen(url, operation);
	if (f) {
		file = (URLIO_FILE*)malloc(sizeof(URLIO_FILE));

		if (!file) {
			errno = EBADF;
			// g_mutex_unlock(&cache_lock);
			return NULL;
		}

		memset(file, 0, sizeof(URLIO_FILE));

		file->url = (char*) malloc((strlen(url)+1) * sizeof(char));
		strcpy(file->url, url);

		file->close_flag = false;
		file->handle.file = f;
		file->type = CFTYPE_FILE; /* marked as URL */
	} else {
		// g_mutex_lock(&cache_lock);

		for (int i = 0; i < url_cache_count; i++) {
			if (0 == strcmp(url_cache[i]->url, url)) {
				file = url_cache[i];

				/* halt transaction */
				curl_multi_remove_handle(file->multi_handle, file->handle.curl);

				/* ditch buffer - write will recreate - resets stream pos*/
				free(file->buffer);
				file->buffer = NULL;
				file->buffer_pos = 0;
				file->buffer_len = 0;
				file->pos = 0;

				/* reset */
				curl_easy_reset(file->handle.curl);
				curl_easy_setopt(file->handle.curl, CURLOPT_URL, file->url);
				curl_easy_setopt(file->handle.curl, CURLOPT_WRITEDATA, file);
				curl_easy_setopt(file->handle.curl, CURLOPT_VERBOSE, CURL_VERBOSE);
				curl_easy_setopt(file->handle.curl, CURLOPT_WRITEFUNCTION,
						write_callback);
				curl_easy_setopt(file->handle.curl, CURLOPT_RESUME_FROM_LARGE,
						file->pos);

				/* restart */
				curl_multi_add_handle(file->multi_handle, file->handle.curl);
				curl_multi_perform(file->multi_handle, &file->still_running);

				file->close_flag = false;
				// file->cacheLifeSpan = MAX_SURVIVAL_TIME;

				if ((file->buffer_pos == 0) && (!file->still_running)) {
					/* if still_running is 0 now, we should return NULL */

					/* make sure the easy handle is not in the multi handle anymore */
					curl_multi_remove_handle(file->multi_handle, file->handle.curl);

					/* cleanup */
					curl_easy_cleanup(file->handle.curl);

					free(file->buffer);/* free any allocated buffer space */
					free(file->url);
					if (file->cache_count != 0) {
						for (int i = 0; i < file->cache_count; i++)
							free(file->cache_list[i]);
						free(file->cache_list);
						free(file->cache_id_list);
					}
					free(file);

					file = NULL;
				}

				// g_mutex_unlock(&cache_lock);

				return file;
			}
		}

		file = (URLIO_FILE*)malloc(sizeof(URLIO_FILE));

		file->close_flag = false;

		if (!file) {
			errno = EBADF;
			// g_mutex_unlock(&cache_lock);
			return NULL;
		}

		memset(file, 0, sizeof(URLIO_FILE));



		file->type = CFTYPE_CURL; /* marked as URL */
		file->handle.curl = curl_easy_init();

		file->url = (char*) malloc((strlen(url)+1) * sizeof(char));
		strcpy(file->url, url);

		curl_easy_setopt(file->handle.curl, CURLOPT_URL, url);
		curl_easy_setopt(file->handle.curl, CURLOPT_WRITEDATA, file);
		curl_easy_setopt(file->handle.curl, CURLOPT_VERBOSE, CURL_VERBOSE);
		curl_easy_setopt(file->handle.curl, CURLOPT_WRITEFUNCTION,
				write_callback);

		if (!file->multi_handle)
			file->multi_handle = curl_multi_init();

		curl_multi_add_handle(file->multi_handle, file->handle.curl);

		/* lets start the fetch */
		curl_multi_perform(file->multi_handle, &file->still_running);

		if ((file->buffer_pos == 0) && (!file->still_running)) {
			/* if still_running is 0 now, we should return NULL */

			/* make sure the easy handle is not in the multi handle anymore */
			curl_multi_remove_handle(file->multi_handle, file->handle.curl);

			/* cleanup */
			curl_easy_cleanup(file->handle.curl);

			free(file->buffer);/* free any allocated buffer space */
			free(file->url);
			if (file->cache_count != 0) {
				for (int i = 0; i < file->cache_count; i++)
					free(file->cache_list[i]);
				free(file->cache_list);
				free(file->cache_id_list);
			}
			free(file);

			file = NULL;
			errno = EBADF;
		} else {
			for (int retry = 0; retry < RETRY_TIMES; retry++) {
				fill_buffer(file, 1);
				if (file->buffer_pos)
					break;

				/* halt transaction */
				curl_multi_remove_handle(file->multi_handle, file->handle.curl);

				/* ditch buffer - write will recreate - resets stream pos*/
				free(file->buffer);
				file->buffer = NULL;
				file->buffer_pos = 0;
				file->buffer_len = 0;

				/* restart */
				curl_multi_add_handle(file->multi_handle, file->handle.curl);

				/* lets start the fetch again */
				curl_multi_perform(file->multi_handle, &file->still_running);
			}

			/* check if theres data in the buffer - if not fill_buffer()
			 * either errored or EOF */
			if (!file->buffer_pos) {
				/* if still_running is 0 now, we should return NULL */

				/* make sure the easy handle is not in the multi handle anymore */
				curl_multi_remove_handle(file->multi_handle, file->handle.curl);

				/* cleanup */
				curl_easy_cleanup(file->handle.curl);

				free(file->buffer);/* free any allocated buffer space */
				free(file->url);
				if (file->cache_count != 0) {
					for (int i = 0; i < file->cache_count; i++)
						free(file->cache_list[i]);
					free(file->cache_list);
					free(file->cache_id_list);
				}
				free(file);

				file = NULL;
				errno = EBADF;
			}

			double dSize;
			curl_easy_getinfo(file->handle.curl,
					CURLINFO_CONTENT_LENGTH_DOWNLOAD, &dSize);
			file->size = (long) dSize;
#ifdef URLIO_VERBOSE
			printf("fopen: file length %zu\n", file->size);
#endif
			/* halt transaction */
			curl_multi_remove_handle(file->multi_handle, file->handle.curl);

			/* ditch buffer - write will recreate - resets stream pos*/
			free(file->buffer);
			file->buffer = NULL;
			file->buffer_pos = 0;
			file->buffer_len = 0;
			file->pos = 0L;

			/* reset */
			curl_easy_reset(file->handle.curl);
			curl_easy_setopt(file->handle.curl, CURLOPT_URL, file->url);
			curl_easy_setopt(file->handle.curl, CURLOPT_WRITEDATA, file);
			curl_easy_setopt(file->handle.curl, CURLOPT_VERBOSE, CURL_VERBOSE);
			curl_easy_setopt(file->handle.curl, CURLOPT_WRITEFUNCTION,
					write_callback);
			curl_easy_setopt(file->handle.curl, CURLOPT_RESUME_FROM_LARGE,
					file->pos);

			/* restart */
			curl_multi_add_handle(file->multi_handle, file->handle.curl);

			/* lets start the fetch again */
			curl_multi_perform(file->multi_handle, &file->still_running);

			if ((file->buffer_pos == 0) && (!file->still_running)) {
				/* if still_running is 0 now, we should return NULL */

				/* make sure the easy handle is not in the multi handle anymore */
				curl_multi_remove_handle(file->multi_handle, file->handle.curl);

				/* cleanup */
				curl_easy_cleanup(file->handle.curl);

				free(file->buffer);/* free any allocated buffer space */
				free(file->url);
				if (file->cache_count != 0) {
					for (int i = 0; i < file->cache_count; i++)
						free(file->cache_list[i]);
					free(file->cache_list);
					free(file->cache_id_list);
				}
				free(file);

				file = NULL;
				errno = EBADF;
			}
		}


		if (file != NULL) {
			// initial remote access for multuthread

			memset(file->fcurl_data, 0, sizeof(file->fcurl_data));

			bool areAllThreadsSet = true;

			for (int t = 0; t < THREAD_NUM; t ++) {
				file->fcurl_data[t] = (FCURL_DATA*)malloc(sizeof(FCURL_DATA));
				memset(file->fcurl_data[t], 0, sizeof(FCURL_DATA));

				file->fcurl_data[t]->type = CFTYPE_CURL; /* marked as URL */
				file->fcurl_data[t]->handle.curl = curl_easy_init();
				file->fcurl_data[t]->tid = t;
				file->fcurl_data[t]->size =file->size;

				file->fcurl_data[t]->url = (char*) malloc(strlen(url) * sizeof(char));
				strcpy(file->fcurl_data[t]->url, url);
				file->fcurl_data[t]->cache = (char*) malloc(THREAD_CACHE_SIZE * sizeof(char));

				curl_easy_setopt(file->fcurl_data[t]->handle.curl, CURLOPT_URL, file->url);
				curl_easy_setopt(file->fcurl_data[t]->handle.curl, CURLOPT_WRITEDATA, file->fcurl_data[t]);
				curl_easy_setopt(file->fcurl_data[t]->handle.curl, CURLOPT_VERBOSE, CURL_VERBOSE);
				curl_easy_setopt(file->fcurl_data[t]->handle.curl, CURLOPT_WRITEFUNCTION,
						write_callback);

				file->fcurl_data[t]->multi_handle = curl_multi_init();

				curl_multi_add_handle(file->fcurl_data[t]->multi_handle, file->fcurl_data[t]->handle.curl);

				/* lets start the fetch */
				curl_multi_perform(file->fcurl_data[t]->multi_handle, &file->fcurl_data[t]->still_running);

				if ((file->fcurl_data[t]->buffer_pos == 0) && (!file->fcurl_data[t]->still_running)) {
					areAllThreadsSet = false;

					break;
				}
			}

			if(!areAllThreadsSet) {
				for (int t = 0; t < THREAD_NUM; t ++) {
					/* if still_running is 0 now, we should return NULL */

					/* make sure the easy handle is not in the multi handle anymore */
					curl_multi_remove_handle(file->fcurl_data[t]->multi_handle, file->fcurl_data[t]->handle.curl);

					/* cleanup */
					curl_easy_cleanup(file->fcurl_data[t]->handle.curl);

					free(file->fcurl_data[t]->buffer);/* free any allocated buffer space */
					free(file->fcurl_data[t]);

					file->fcurl_data[t] = NULL;
				}

				/* if still_running is 0 now, we should return NULL */

				/* make sure the easy handle is not in the multi handle anymore */
				curl_multi_remove_handle(file->multi_handle, file->handle.curl);

				/* cleanup */
				curl_easy_cleanup(file->handle.curl);

				free(file->buffer);/* free any allocated buffer space */
				free(file->url);
				if (file->cache_count != 0) {
					for (int i = 0; i < file->cache_count; i++)
						free(file->cache_list[i]);
					free(file->cache_list);
					free(file->cache_id_list);
				}
				free(file);

				file = NULL;
				errno = EBADF;
			}
		}

		if (file != NULL) {
			// initial url cache
			if (url_cache_count == 0)
				url_cache = (URLIO_FILE**) malloc(sizeof(URLIO_FILE*));
			else
				url_cache = (URLIO_FILE**) realloc(url_cache,
						(url_cache_count + 1) * sizeof(URLIO_FILE*));

			url_cache[url_cache_count] = file;

			url_cache_count++;

			// g_mutex_unlock(&cache_lock);

			// initial house keeping thread

			// if(ghousekeepingthread == NULL) {
			// 	ghousekeepingthread = g_thread_new("housekeeping thread",
			// 		&housekeepingthread, NULL);
			// }
		}
	}

	return file;
}

int urlio_fclose(URLIO_FILE *file) {
#ifdef URLIO_VERBOSE
	printf("fclose: %s\n", file->url);
#endif


	int ret = 0; /* default is good return */

	switch (file->type) {
	case CFTYPE_FILE:
		ret = fclose(file->handle.file);
		free(file->url);
		free(file);

		break;

	case CFTYPE_CURL:
		// g_mutex_lock(&cache_lock);

		// int ret = 0;/* default is good return */

		for (int i = 0; i < url_cache_count; i++) {
			if ((long int) file == (long int) url_cache[i]) {
				file->close_flag = true;
				break;
			}
		}

		// g_mutex_unlock(&cache_lock);

		break;

	default: /* unknown or supported type - oh dear */
		ret = -1;
		errno = EBADF;
		break;
	}

	return ret;
}

void urlio_finitial(void) {
#ifdef URLIO_VERBOSE
	printf("finitial\n");
#endif

	g_thread_init(NULL);
	curl_global_init(CURL_GLOBAL_ALL);
}

int urlio_frelease(const char *url) {
#ifdef URLIO_VERBOSE
	printf("frelease: %s\n", url);
#endif
	// g_mutex_lock(&cache_lock);

	int ret = -1;/* default is bad return */

	for (int count = 0; count < url_cache_count; count++) {
		if (0 == strcmp(url_cache[count]->url, url)) {
			// url_cache[count]->cacheLifeSpan = 0;

















			curl_multi_remove_handle(url_cache[count]->multi_handle,
									url_cache[count]->handle.curl);

			/* cleanup */
			curl_easy_cleanup(url_cache[count]->handle.curl);

			/* clean up multithread */
			for (int t = 0; t < THREAD_NUM; t ++) {
				/* if still_running is 0 now, we should return NULL */

				/* make sure the easy handle is not in the multi handle anymore */
				curl_multi_remove_handle(url_cache[count]->fcurl_data[t]->multi_handle, url_cache[count]->fcurl_data[t]->handle.curl);

				/* cleanup */
				curl_easy_cleanup(url_cache[count]->fcurl_data[t]->handle.curl);

				free(url_cache[count]->fcurl_data[t]->buffer);/* free any allocated buffer space */
				free(url_cache[count]->fcurl_data[t]->url);
				free(url_cache[count]->fcurl_data[t]->cache);
				free(url_cache[count]->fcurl_data[t]);

				url_cache[count]->fcurl_data[t] = NULL;
			}

			free(url_cache[count]->buffer);/* free any allocated buffer space */
			free(url_cache[count]->url);
			if (url_cache[count]->cache_count != 0) {
				for (int i = 0; i < url_cache[count]->cache_count; i++)
					free(url_cache[count]->cache_list[i]);
				free(url_cache[count]->cache_list);
				free(url_cache[count]->cache_id_list);
			}

			free(url_cache[count]);

			url_cache[count] = NULL;

			if (count != url_cache_count - 1)
				memmove(&url_cache[count], &url_cache[count + 1],
						(url_cache_count - count - 1) * sizeof(URLIO_FILE*));
			if (url_cache_count != 1) {
				url_cache = (URLIO_FILE**) realloc(url_cache,
						(url_cache_count - 1) * sizeof(URLIO_FILE*));
				url_cache_count--;
			} else {
				free(url_cache);
				url_cache = NULL;
				url_cache_count = 0;
			}



















			ret = 0;
		}
	}

	// g_mutex_unlock(&cache_lock);
	return ret;
}

int urlio_feof(URLIO_FILE *file) {
#ifdef URLIO_VERBOSE
	printf("feof: %s\n", file->url);
#endif
	int ret = 0;

	switch (file->type) {
	case CFTYPE_FILE:
		ret = feof(file->handle.file);
		break;

	case CFTYPE_CURL:
		if ((file->buffer_pos == 0) && (!file->still_running))
			ret = 1;
		break;

	default: /* unknown or supported type - oh dear */
		ret = -1;
		errno = EBADF;
		break;
	}

	return ret;
}

int urlio_ferror(URLIO_FILE *file) {
#ifdef URLIO_VERBOSE
	printf("ferror: %s\n", file->url);
#endif

	int ret = 0;

	switch (file->type) {
	case CFTYPE_FILE:
		ret = ferror(file->handle.file);
		break;

	case CFTYPE_CURL:
		if ((file->buffer_pos == 0) && (!file->still_running))
			ret = 1;
		break;

	default: /* unknown or supported type - oh dear */
		ret = -1;
		errno = EBADF;
		break;
	}

	return ret;
}

char *urlio_fgets(char *ptr, size_t size, URLIO_FILE *file) {
	size_t want = size - 1;/* always need to leave room for zero termination */
	size_t loop;

	switch (file->type) {
	case CFTYPE_FILE:
#ifdef URLIO_VERBOSE
		printf("fgets: from position %ld read %zu byte(s)\n", ftell(file->handle.file), size);
#endif
		ptr = fgets(ptr, (int) size, file->handle.file);
		break;

	case CFTYPE_CURL:
#ifdef URLIO_VERBOSE
		printf("fgets: from position %ld read %zu byte(s)\n", file->pos, size);
#endif

		for (int retry = 0; retry < RETRY_TIMES; retry++) {
			fill_buffer(file, want);
			if (file->buffer_pos)
				break;

			/* halt transaction */
			curl_multi_remove_handle(file->multi_handle, file->handle.curl);

			/* ditch buffer - write will recreate - resets stream pos*/
			free(file->buffer);
			file->buffer = NULL;
			file->buffer_pos = 0;
			file->buffer_len = 0;

			/* restart */
			curl_multi_add_handle(file->multi_handle, file->handle.curl);

			/* lets start the fetch again */
			curl_multi_perform(file->multi_handle, &file->still_running);
		}

		/* check if theres data in the buffer - if not fill either errored or
		 * EOF */
		if (!file->buffer_pos)
			return NULL;

		/* ensure only available data is considered */
		if (file->buffer_pos < want)
			want = file->buffer_pos;

		/*buffer contains data */
		/* look for newline or eof */
		for (loop = 0; loop < want; loop++) {
			if (file->buffer[loop] == '\n') {
				want = loop + 1;/* include newline */
				break;
			}
		}

		/* xfer data to caller */
		memcpy(ptr, file->buffer, want);
		ptr[want] = 0;/* allways null terminate */

		use_buffer(file, want);
		file->pos += want;
		break;

	default: /* unknown or supported type - oh dear */
		ptr = NULL;
		errno = EBADF;
		break;
	}

	return ptr;/*success */
}

int urlio_fgetc(URLIO_FILE *file) {

	int c;

	switch (file->type) {
	case CFTYPE_FILE:
#ifdef URLIO_VERBOSE
		printf("fgetc: from position %ld read 1 byte\n", ftell(file->handle.file));
#endif
		c = fgetc(file->handle.file);
		break;

	case CFTYPE_CURL:
#ifdef URLIO_VERBOSE
		printf("fgetc: from position %ld read 1 byte\n", file->pos);
#endif

		for (int retry = 0; retry < RETRY_TIMES; retry++) {
			fill_buffer(file, 1);
			if (file->buffer_pos)
				break;

			/* halt transaction */
			curl_multi_remove_handle(file->multi_handle, file->handle.curl);

			/* ditch buffer - write will recreate - resets stream pos*/
			free(file->buffer);
			file->buffer = NULL;
			file->buffer_pos = 0;
			file->buffer_len = 0;

			/* restart */
			curl_multi_add_handle(file->multi_handle, file->handle.curl);

			/* lets start the fetch again */
			curl_multi_perform(file->multi_handle, &file->still_running);
		}

		/* check if theres data in the buffer - if not fill_buffer()
		 * either errored or EOF */
		if (!file->buffer_pos)
			return EOF;

		/* xfer data to caller */
		c = (int) *(file->buffer);
		use_buffer(file, 1);
		file->pos++;

		break;

	default: /* unknown or supported type - oh dear */
		errno = EBADF;
		c = EOF;
		break;

	}

	return c;
}

void urlio_rewind(URLIO_FILE *file) {
	long int p;
	switch (file->type) {
	case CFTYPE_FILE:
		p = ftell(file->handle.file);
#ifdef URLIO_VERBOSE
		printf("rewind: from position %ld\n", p);
#endif
		rewind(file->handle.file); /* passthrough */
		break;

	case CFTYPE_CURL:
#ifdef URLIO_VERBOSE
		printf("rewind: from position %ld\n", file->pos);
#endif

		/* halt transaction */
		curl_multi_remove_handle(file->multi_handle, file->handle.curl);

		/* ditch buffer - write will recreate - resets stream pos*/
		free(file->buffer);
		file->buffer = NULL;
		file->buffer_pos = 0;
		file->buffer_len = 0;
		file->pos = 0;

		/* reset */
		curl_easy_reset(file->handle.curl);
		curl_easy_setopt(file->handle.curl, CURLOPT_URL, file->url);
		curl_easy_setopt(file->handle.curl, CURLOPT_WRITEDATA, file);
		curl_easy_setopt(file->handle.curl, CURLOPT_VERBOSE, CURL_VERBOSE);
		curl_easy_setopt(file->handle.curl, CURLOPT_WRITEFUNCTION,
				write_callback);
		curl_easy_setopt(file->handle.curl, CURLOPT_RESUME_FROM_LARGE,
				file->pos);

		/* restart */
		curl_multi_add_handle(file->multi_handle, file->handle.curl);
		curl_multi_perform(file->multi_handle, &file->still_running);

		if ((file->buffer_pos == 0) && (!file->still_running)) {
			/* if still_running is 0 now, we should return NULL */

			/* make sure the easy handle is not in the multi handle anymore */
			curl_multi_remove_handle(file->multi_handle, file->handle.curl);

			/* cleanup */
			curl_easy_cleanup(file->handle.curl);

			free(file->buffer);/* free any allocated buffer space */
			free(file->url);
			if (file->cache_count != 0) {
				for (int i = 0; i < file->cache_count; i++)
					free(file->cache_list[i]);
				free(file->cache_list);
				free(file->cache_id_list);
			}
			free(file);

			file = NULL;
		}
		break;

	default: /* unknown or supported type - oh dear */
		errno = EBADF;
		break;
	}
}

long int urlio_ftell(URLIO_FILE * file) {
	long int p;
	switch (file->type) {
	case CFTYPE_FILE:
		p = ftell(file->handle.file);
#ifdef URLIO_VERBOSE
		printf("ftell: current position %ld\n", p);
#endif
		return p;
		break;

	case CFTYPE_CURL:
#ifdef URLIO_VERBOSE
		printf("ftell: current position %ld\n", file->pos);
#endif
		return file->pos;
		break;
	default:
		errno = EBADF;
		return -1;
		break;
	}
}
int urlio_fseek(URLIO_FILE * file, long int offset, int origin) {
	switch (file->type) {
	case CFTYPE_FILE:
		return fseek(file->handle.file, offset, origin); /* passthrough */
		break;

	case CFTYPE_CURL:
		switch (origin) {
		case SEEK_SET:
#ifdef URLIO_VERBOSE
			printf("fseek: seek to offset %ld from head\n", offset);
#endif
			file->pos = offset;
			break;
		case SEEK_CUR:
#ifdef URLIO_VERBOSE
			printf("fseek: seek to offset %ld from position %ld\n", offset, file->pos);
#endif
			file->pos = file->pos + offset;
			break;
		case SEEK_END:
#ifdef URLIO_VERBOSE
			printf("fseek: seek to offset %ld from tail\n", offset);
#endif
			file->pos = file->size + offset;
			break;
		default: /* unknown or supported type - oh dear */
			errno = EBADF;
			return -1;
			break;
		}

		/* halt transaction */
		curl_multi_remove_handle(file->multi_handle, file->handle.curl);

		/* ditch buffer - write will recreate - resets stream pos*/
		free(file->buffer);
		file->buffer = NULL;
		file->buffer_pos = 0;
		file->buffer_len = 0;

		/* reset */
		curl_easy_reset(file->handle.curl);
		curl_easy_setopt(file->handle.curl, CURLOPT_URL, file->url);
		curl_easy_setopt(file->handle.curl, CURLOPT_WRITEDATA, file);
		curl_easy_setopt(file->handle.curl, CURLOPT_VERBOSE, CURL_VERBOSE);
		curl_easy_setopt(file->handle.curl, CURLOPT_WRITEFUNCTION,
				write_callback);
		curl_easy_setopt(file->handle.curl, CURLOPT_RESUME_FROM_LARGE,
				file->pos);

		/* restart */
		curl_multi_add_handle(file->multi_handle, file->handle.curl);

		/* lets start the fetch again */
		curl_multi_perform(file->multi_handle, &file->still_running);

		if ((file->buffer_pos == 0) && (!file->still_running)) {
			/* if still_running is 0 now, we should return NULL */

			/* make sure the easy handle is not in the multi handle anymore */
			curl_multi_remove_handle(file->multi_handle, file->handle.curl);

			/* cleanup */
			curl_easy_cleanup(file->handle.curl);

			free(file->buffer);/* free any allocated buffer space */
			free(file->url);
			if (file->cache_count != 0) {
				for (int i = 0; i < file->cache_count; i++)
					free(file->cache_list[i]);
				free(file->cache_list);
				free(file->cache_id_list);
			}
			free(file);

			file = NULL;
			return -1;
		}

		return 0;

		break;
	default: /* unknown or supported type - oh dear */
		errno = EBADF;
		return -1;
		break;
	}
}

//size_t urlio_fread(void *ptr, size_t size, size_t nmemb, URLIO_FILE *file) {
//	printf("fread: %d, %d\n", file->pos, size);
//	long int orig_pointer = file->pos;
//	size_t orig_size = size * nmemb;
//
//	size_t current_size = orig_size;
//	long int current_pointer = file->pos;
//	long int ptr_pointer = 0;
//	long int copied_size = 0;
//
//	int cache_count = (((file->pos % CACHE_SIZE) + (size * nmemb) - 1)
//			/ CACHE_SIZE) + 1;
//
//	for (int i = 0; i < cache_count; i++) {
//		long int cache_id = (current_pointer / CACHE_SIZE) * CACHE_SIZE;
//
//		int cache_index = -1;
//
//		for (int j = 0; j < file->cache_count; j++) {
//			if (file->cache_id_list[j] == cache_id) {
//				cache_index = j;
//				break;
//			}
//		}
//
//		if (cache_index == -1) {
//
//			/* halt transaction */
//			curl_multi_remove_handle(file->multi_handle, file->handle.curl);
//
//			/* ditch buffer - write will recreate - resets stream pos*/
//			free(file->buffer);
//			file->buffer = NULL;
//			file->buffer_pos = 0;
//			file->buffer_len = 0;
//
//			/* reset */
//			curl_easy_reset(file->handle.curl);
//			curl_easy_setopt(file->handle.curl, CURLOPT_URL, file->url);
//			curl_easy_setopt(file->handle.curl, CURLOPT_WRITEDATA, file);
//			curl_easy_setopt(file->handle.curl, CURLOPT_VERBOSE, CURL_VERBOSE);
//			curl_easy_setopt(file->handle.curl, CURLOPT_WRITEFUNCTION,
//					write_callback);
//			curl_easy_setopt(file->handle.curl, CURLOPT_RESUME_FROM_LARGE,
//					cache_id);
//
//			/* restart */
//			curl_multi_add_handle(file->multi_handle, file->handle.curl);
//
//			/* lets start the fetch again */
//			curl_multi_perform(file->multi_handle, &file->still_running);
//
//			if ((file->buffer_pos == 0) && (!file->still_running)) {
//				/* if still_running is 0 now, we should return NULL */
//
//				/* make sure the easy handle is not in the multi handle anymore */
//				curl_multi_remove_handle(file->multi_handle, file->handle.curl);
//
//				/* cleanup */
//				curl_easy_cleanup(file->handle.curl);
//
//				free(file->buffer);/* free any allocated buffer space */
//				free(file->url);
//				if (file->cache_count != 0) {
//					for (int i = 0; i < file->cache_count; i++)
//						free(file->cache_list[i]);
//					free(file->cache_list);
//					free(file->cache_id_list);
//				}
//				free(file);
//
//				file = NULL;
//				return 0;
//			}
//
//			/* fill cache */
//			size_t want = CACHE_SIZE;
//
//			for (int retry = 0; retry < RETRY_TIMES; retry++) {
//				fill_buffer(file, want);
//				if (file->buffer_pos)
//					break;
//
//				/* halt transaction */
//				curl_multi_remove_handle(file->multi_handle, file->handle.curl);
//
//				/* ditch buffer - write will recreate - resets stream pos*/
//				free(file->buffer);
//				file->buffer = NULL;
//				file->buffer_pos = 0;
//				file->buffer_len = 0;
//
//				/* restart */
//				curl_multi_add_handle(file->multi_handle, file->handle.curl);
//
//				/* lets start the fetch again */
//				curl_multi_perform(file->multi_handle, &file->still_running);
//			}
//
//			/* check if theres data in the buffer - if not fill_buffer()
//			 * either errored or EOF */
//			if (!file->buffer_pos)
//				return 0;
//
//			/* ensure only available data is considered */
//			if (file->buffer_pos < want)
//				want = file->buffer_pos;
//
//			/* xfer data to caller */
//			char *cache = (char*) malloc(CACHE_SIZE * sizeof(char));
//			memcpy(cache, file->buffer, want * sizeof(char));
//
//			use_buffer(file, want);
//
//			/* add cache into list */
//
//			if (file->cache_count == 0) {
//				file->cache_list = (char**) malloc(CACHE_SIZE * sizeof(char*));
//				file->cache_id_list = (long int*) malloc(sizeof(long int));
//			} else {
//				file->cache_list = (char**) realloc(file->cache_list,
//						(file->cache_count + 1) * CACHE_SIZE * sizeof(char*));
//				file->cache_id_list = (long int*) realloc(file->cache_id_list,
//						(file->cache_count + 1) * sizeof(long int));
//			}
//
//			file->cache_count++;
//
//			file->cache_list[file->cache_count - 1] = cache;
//			file->cache_id_list[file->cache_count - 1] = cache_id;
//
//			cache_index = file->cache_count - 1;
//		}
//
//		char *src_ptr = &file->cache_list[cache_index][current_pointer
//				- file->cache_id_list[cache_index]];
//		char *dst_ptr = &ptr[ptr_pointer];
//
//		int current_copy_size = 0;
//		if (cache_id
//				== ((current_pointer + current_size - 1) / CACHE_SIZE)
//						* CACHE_SIZE)
//			current_copy_size = current_size;
//		else
//			current_copy_size = CACHE_SIZE
//					- (current_pointer - file->cache_id_list[cache_index]);
//
//		memcpy(dst_ptr, src_ptr, current_copy_size * sizeof(char));
//		copied_size += current_copy_size;
//
//		ptr_pointer += current_copy_size;
//		current_pointer += current_copy_size;
//		current_size -= current_copy_size;
//	}
//
//	/* halt transaction */
//	curl_multi_remove_handle(file->multi_handle, file->handle.curl);
//
//	/* ditch buffer - write will recreate - resets stream pos*/
//	free(file->buffer);
//	file->buffer = NULL;
//	file->buffer_pos = 0;
//	file->buffer_len = 0;
//	file->pos = orig_pointer + orig_size;
//
//	/* reset */
//	curl_easy_reset(file->handle.curl);
//	curl_easy_setopt(file->handle.curl, CURLOPT_URL, file->url);
//	curl_easy_setopt(file->handle.curl, CURLOPT_WRITEDATA, file);
//	curl_easy_setopt(file->handle.curl, CURLOPT_VERBOSE, CURL_VERBOSE);
//	curl_easy_setopt(file->handle.curl, CURLOPT_WRITEFUNCTION, write_callback);
//	curl_easy_setopt(file->handle.curl, CURLOPT_RESUME_FROM_LARGE, file->pos);
//
//	/* restart */
//	curl_multi_add_handle(file->multi_handle, file->handle.curl);
//
//	/* lets start the fetch again */
//	curl_multi_perform(file->multi_handle, &file->still_running);
//
//	if ((file->buffer_pos == 0) && (!file->still_running)) {
//		/* if still_running is 0 now, we should return NULL */
//
//		/* make sure the easy handle is not in the multi handle anymore */
//		curl_multi_remove_handle(file->multi_handle, file->handle.curl);
//
//		/* cleanup */
//		curl_easy_cleanup(file->handle.curl);
//
//		free(file->url);
//		if (file->cache_list != NULL) {
//			for (int i = 0; i < file->cache_count; i++)
//				free(file->cache_list[i]);
//			free(file->cache_list);
//			free(file->cache_id_list);
//		}
//		free(file);
//
//		file = NULL;
//		return 0;
//	}
//
//	return copied_size / size;
//}


// GMutex fread_mutex[THREAD_NUM];
// GCond fread_cond[THREAD_NUM];
GThread *gfreadthread[THREAD_NUM];

GMutex fread_thread_mutex;
GMutex fread_main_mutex;
GCond fread_main_cond;
bool freadThreadSetFlag[THREAD_NUM];
bool freadThreadGoodFlag[THREAD_NUM];

static void *fread_thread(FCURL_DATA *data) {
#ifdef URLIO_VERBOSE
	printf("thread %d started for reading %ld byte(s) from position %ld...\n", data->tid, data->want, data->pos);
#endif

	/* halt transaction */
	curl_multi_remove_handle(data->multi_handle, data->handle.curl);

	/* ditch buffer - write will recreate - resets stream pos*/
	free(data->buffer);
	data->buffer = NULL;
	data->buffer_pos = 0;
	data->buffer_len = 0;

	/* reset */
	curl_easy_reset(data->handle.curl);
	curl_easy_setopt(data->handle.curl, CURLOPT_URL, data->url);
	curl_easy_setopt(data->handle.curl, CURLOPT_WRITEDATA, data);
	curl_easy_setopt(data->handle.curl, CURLOPT_VERBOSE, CURL_VERBOSE);
	curl_easy_setopt(data->handle.curl, CURLOPT_WRITEFUNCTION,
			write_callback);
	curl_easy_setopt(data->handle.curl, CURLOPT_RESUME_FROM_LARGE,
			data->pos);

	/* restart */
	curl_multi_add_handle(data->multi_handle, data->handle.curl);

	/* lets start the fetch again */
	curl_multi_perform(data->multi_handle, &data->still_running);

	if ((data->buffer_pos == 0) && (!data->still_running)) {
		/* if still_running is 0 now, we should return NULL */

		/* make sure the easy handle is not in the multi handle anymore */
		curl_multi_remove_handle(data->multi_handle, data->handle.curl);

		/* cleanup */
		curl_easy_cleanup(data->handle.curl);

		free(data->buffer);/* free any allocated buffer space */
		free(data->url);

		free(data);

		data = NULL;
		return 0;
	}

	/* fill cache */

	if((size_t)data->pos < (size_t)data->size) {
		if((data->pos+data->want-1)>data->size) {
#ifdef URLIO_VERBOSE
			printf("thread %d exceeds file end, reduce want from %lu to %lu...\n", data->tid, data->want, data->size - data->pos);
#endif

			data->want = data->size - data->pos;
		}

		for (int retry = 0; retry < RETRY_TIMES; retry++) {
			fill_buffer_thread(data, data->want);
			if (data->buffer_pos) {
				break;
			}
			else {
#ifdef URLIO_VERBOSE
				printf("thread %d retry %d time(s)...\n", data->tid, retry+1);
#endif
				// sleep(1);
			}

			/* halt transaction */
			curl_multi_remove_handle(data->multi_handle, data->handle.curl);

			/* ditch buffer - write will recreate - resets stream pos*/
			free(data->buffer);
			data->buffer = NULL;
			data->buffer_pos = 0;
			data->buffer_len = 0;

			/* restart */
			curl_multi_add_handle(data->multi_handle, data->handle.curl);

			/* lets start the fetch again */
			curl_multi_perform(data->multi_handle, &data->still_running);
		}

		if (data->buffer_pos) {
			/* ensure only available data is considered */
			if (data->buffer_pos < data->want)
				data->want = data->buffer_pos;

			/* xfer data to caller */
			memcpy(data->cache, data->buffer, data->want * sizeof(char));

			use_buffer_thread(data, data->want);

			freadThreadGoodFlag[data->tid] = true;
		}
		else {
			freadThreadGoodFlag[data->tid] = false;
		}
	}
	else {
#ifdef URLIO_VERBOSE
		printf("thread %d position out of range...\n", data->tid);
#endif
		data->want = 0;
		freadThreadGoodFlag[data->tid] = true;
	}

#ifdef URLIO_VERBOSE
	printf("thread %d finished...\n", data->tid);
#endif

	freadThreadSetFlag[data->tid] = true;
	bool freadThreadAllSetFlag = true;

	g_mutex_lock (&fread_thread_mutex);

	for(int t = 0; t < THREAD_NUM; t ++) freadThreadAllSetFlag &= freadThreadSetFlag[t];

	if(freadThreadAllSetFlag) {
		g_mutex_lock (&fread_main_mutex);
		g_cond_signal (&fread_main_cond);
		g_mutex_unlock (&fread_main_mutex);
	}

	g_mutex_unlock (&fread_thread_mutex);
}


size_t urlio_fread(void *ptr, size_t size, size_t nmemb, URLIO_FILE *file) {
	if(file->type == CFTYPE_FILE) {
#ifdef URLIO_VERBOSE
		printf("fread: reading %lu byte(s) from position %ld\n", size*nmemb, ftell(file->handle.file));
#endif
		return fread(ptr, size, nmemb, file->handle.file);
	}
	else {
		long int orig_pointer = file->pos;
		size_t orig_size = size * nmemb;

		size_t current_size = orig_size;
		long int current_pointer = file->pos;
		long int ptr_pointer = 0;
		long int copied_size = 0;



		int cache_count = (((file->pos % CACHE_SIZE) + (size * nmemb) - 1)
				/ CACHE_SIZE) + 1;

		for (int i = 0; i < cache_count; i++) {
			long int cache_id = (current_pointer / CACHE_SIZE) * CACHE_SIZE;

			int cache_index = -1;

			for (int j = 0; j < file->cache_count; j++) {
				if (file->cache_id_list[j] == cache_id) {
					cache_index = j;
#ifdef URLIO_VERBOSE
					printf("fread: reading %zu byte(s) from position %ld cache hit\n", size, file->pos);
#endif
					break;
				}
			}


			if (cache_index == -1) {
#ifdef URLIO_VERBOSE
				printf("fread: reading %zu byte(s) from position %ld cache miss, start %d-thread(s) downloading\n", size, file->pos, THREAD_NUM);
#endif

				char *thread_cache = (char*) malloc(CACHE_SIZE * sizeof(char));
				size_t thread_want = CACHE_SIZE;

				for(int t = 0; t < THREAD_NUM; t ++) {
					file->fcurl_data[t]->pos = cache_id+t*THREAD_CACHE_SIZE;
					file->fcurl_data[t]->want = THREAD_CACHE_SIZE;
					freadThreadSetFlag[t] = false;
				}

				g_mutex_lock (&fread_main_mutex);

				for(int t = 0; t < THREAD_NUM; t ++) {
					gfreadthread[t] = g_thread_new("fread thread", &fread_thread, file->fcurl_data[t]);
				}

				g_cond_wait (&fread_main_cond, &fread_main_mutex);
				g_mutex_unlock (&fread_main_mutex);

				for(int t = 0; t < THREAD_NUM; t ++) {
					if(!freadThreadGoodFlag[t]) {
#ifdef URLIO_VERBOSE
						printf("fread: failed\n");
#endif
						return 0;
					}
				}

				thread_want = 0;
				for(int t = 0; t < THREAD_NUM; t ++) {
					memcpy(thread_cache+thread_want, file->fcurl_data[t]->cache, file->fcurl_data[t]->want * sizeof(char));
					thread_want += file->fcurl_data[t]->want;
				}

				/* add cache into list */

				if (file->cache_count == 0) {
					file->cache_list = (char**) malloc(CACHE_SIZE * sizeof(char*));
					file->cache_id_list = (long int*) malloc(sizeof(long int));
				} else {
					file->cache_list = (char**) realloc(file->cache_list,
							(file->cache_count + 1) * CACHE_SIZE * sizeof(char*));
					file->cache_id_list = (long int*) realloc(file->cache_id_list,
							(file->cache_count + 1) * sizeof(long int));
				}

				file->cache_list[file->cache_count] = thread_cache;
				file->cache_id_list[file->cache_count] = cache_id;

				cache_index = file->cache_count;

				file->cache_count++;

			}

			char *src_ptr = (char*)&file->cache_list[cache_index][current_pointer
					- file->cache_id_list[cache_index]];
			char *dst_ptr = (char*)&ptr[ptr_pointer];

			int current_copy_size = 0;
			if (cache_id
					== ((current_pointer + current_size - 1) / CACHE_SIZE)
							* CACHE_SIZE)
				current_copy_size = current_size;
			else
				current_copy_size = CACHE_SIZE
						- (current_pointer - file->cache_id_list[cache_index]);

			memcpy(dst_ptr, src_ptr, current_copy_size * sizeof(char));
			copied_size += current_copy_size;

			ptr_pointer += current_copy_size;
			current_pointer += current_copy_size;
			current_size -= current_copy_size;
		}

		/* halt transaction */
		curl_multi_remove_handle(file->multi_handle, file->handle.curl);

		/* ditch buffer - write will recreate - resets stream pos*/
		free(file->buffer);
		file->buffer = NULL;
		file->buffer_pos = 0;
		file->buffer_len = 0;
		file->pos = orig_pointer + orig_size;

		/* reset */
		curl_easy_reset(file->handle.curl);
		curl_easy_setopt(file->handle.curl, CURLOPT_URL, file->url);
		curl_easy_setopt(file->handle.curl, CURLOPT_WRITEDATA, file);
		curl_easy_setopt(file->handle.curl, CURLOPT_VERBOSE, CURL_VERBOSE);
		curl_easy_setopt(file->handle.curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(file->handle.curl, CURLOPT_RESUME_FROM_LARGE, file->pos);

		/* restart */
		curl_multi_add_handle(file->multi_handle, file->handle.curl);

		/* lets start the fetch again */
		curl_multi_perform(file->multi_handle, &file->still_running);

		if ((file->buffer_pos == 0) && (!file->still_running)) {
			/* if still_running is 0 now, we should return NULL */

			/* make sure the easy handle is not in the multi handle anymore */
			curl_multi_remove_handle(file->multi_handle, file->handle.curl);

			/* cleanup */
			curl_easy_cleanup(file->handle.curl);

			free(file->url);
			if (file->cache_list != NULL) {
				for (int i = 0; i < file->cache_count; i++)
					free(file->cache_list[i]);
				free(file->cache_list);
				free(file->cache_id_list);
			}
			free(file);

			file = NULL;
			return 0;
		}

		return copied_size / size;
	}
}

