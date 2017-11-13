
#ifndef OPENREMOTESLIDE_OPENREMOTESLIDE_URL_H_
#define OPENREMOTESLIDE_OPENREMOTESLIDE_URL_H_

#define THREAD_NUM 4
#define THREAD_CACHE_SIZE (256*1024)
#define CACHE_SIZE (THREAD_NUM*THREAD_CACHE_SIZE)
#define RETRY_TIMES 10
#define URLIO_VERBOSE
#define CURL_VERBOSE 0L

//// #define MAX_SURVIVAL_TIME 300

#include <curl/curl.h>

enum fcurl_type_e {
	CFTYPE_NONE = 0, CFTYPE_FILE = 1, CFTYPE_CURL = 2
};

struct fcurl_data_struct {
	enum fcurl_type_e type; /* type of handle */
	union {
		CURL *curl;
		FILE *file;
	} handle; /* handle */

	char *buffer; /* buffer to store cached data*/
	size_t buffer_len; /* currently allocated buffers length */
	size_t buffer_pos; /* end of data in buffer*/
	int still_running; /* Is background url fetch still in progress */

	char *url;
	long int pos;
	size_t size;
	size_t want;
	char *cache;
	int tid;
	CURLM *multi_handle;
};

typedef struct fcurl_data_struct FCURL_DATA;

struct URLIO_FILE_struct {
	enum fcurl_type_e type; /* type of handle */
	union {
		CURL *curl;
		FILE *file;
	} handle; /* handle */

	char *buffer; /* buffer to store cached data*/
	size_t buffer_len; /* currently allocated buffers length */
	size_t buffer_pos; /* end of data in buffer*/
	int still_running; /* Is background url fetch still in progress */

	FCURL_DATA *fcurl_data[THREAD_NUM];

	char *url;
	long int pos;
	size_t size;

	CURLM *multi_handle;

	char **cache_list;
	long int *cache_id_list;
	int cache_count;
	// int cacheLifeSpan;
	bool close_flag;
};

typedef struct URLIO_FILE_struct URLIO_FILE;

/* exported functions */
URLIO_FILE	*	urlio_fopen(const char *url, const char *operation);
int 			urlio_fclose(URLIO_FILE *file);
void			urlio_finitial(void);
int				urlio_frelease(const char *url);
int 			urlio_feof(URLIO_FILE *file);
size_t 			urlio_fread(void *ptr, size_t size, size_t nmemb, URLIO_FILE *file);
char 		*	urlio_fgets(char *ptr, size_t size, URLIO_FILE *file);
int 			urlio_fgetc(URLIO_FILE *file);
void 			urlio_rewind(URLIO_FILE *file);
long int 		urlio_ftell(URLIO_FILE * file);
int 			urlio_fseek(URLIO_FILE * file, long int offset, int origin);
int 			urlio_ferror(URLIO_FILE *file);
#endif
