#ifndef RESPONSE_H_INCLUDED
#define RESPONSE_H_INCLUDED

#include <libwebsockets.h>

// creating own user_buffer_data to avoid circular definitions
// TODO maybe create some structures header file to avoid this problem later
struct user_buffer_data {
  char str[256];
  int len;
};

extern int handle_404_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

extern int handle_http_GET_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);
extern int write_GET_response_headers(struct lws *wsi, void *buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

extern int handle_http_POST_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);
extern int write_POST_response_headers(struct lws *wsi, void *buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

extern int handle_http_DELETE_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);
extern int write_DELETE_response_headers(struct lws *wsi, void *buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

# endif
