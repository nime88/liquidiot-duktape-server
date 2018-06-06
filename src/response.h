#ifndef RESPONSE_H_INCLUDED
#define RESPONSE_H_INCLUDED

#include <libwebsockets.h>

// creating own user_buffer_data to avoid circular definitions
// TODO maybe create some structures header file to avoid this problem later
struct user_buffer_data {
  char str[256];
  int len;
  struct lws_spa *spa;		/* lws helper decodes multipart form */
  char filename[128];		/* the filename of the uploaded file */
  unsigned long long file_length; /* the amount of bytes uploaded */
  int fd;				/* fd on file being saved */
  char ext_filename[128];
};

// defining post parameters
static const char * const post_param_names[] = {
	"filekey",
};

enum post_enum_param_names {
	EPN_FILEKEY,
};

extern int handle_404_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

extern int handle_http_GET_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);
extern int write_GET_response_headers(struct lws *wsi, void *buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

extern int handle_http_POST_header(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);
extern int handle_http_POST_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);
extern int write_POST_response_headers(struct lws *wsi, void *buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);
extern int handle_http_POST_form(struct lws *wsi, void *buffer_data, void *in, size_t len);
extern int handle_http_POST_form_complete(struct lws *wsi, void *buffer_data, void *in, size_t len);
extern int handle_http_POST_form_filecb(void *data, const char *name, const char *filename,
	       char *buf, int len, enum lws_spa_fileupload_states state);
extern int generate_POST_response();

extern int handle_http_DELETE_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);
extern int write_DELETE_response_headers(struct lws *wsi, void *buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

# endif
