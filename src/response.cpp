#include "response.h"


int handle_404_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  dest_buffer->len = lws_snprintf(dest_buffer->str, sizeof(dest_buffer->str),
      "REST API was not found");

  if (lws_add_http_common_headers(wsi, 404, "text/html", dest_buffer->len, &p, end))
    return 1;
  if (lws_finalize_write_http_header(wsi, start, &p, end))
    return 1;

  lws_callback_on_writable(wsi);

  return 0;
}

int handle_http_GET_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  dest_buffer->len = lws_snprintf(dest_buffer->str, sizeof(dest_buffer->str),
      "<html>"
      "<h1>GET request</h1>"
      "</html>");

  if(write_GET_response_headers(wsi, dest_buffer, start, p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}

int write_GET_response_headers(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;
  /* prepare and write http headers */
  if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
          "text/html", dest_buffer->len, &p, end))
    return 1;
  if (lws_finalize_write_http_header(wsi, start, &p, end))
    return 1;

  return false;
}

int handle_http_POST_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  dest_buffer->len = lws_snprintf(dest_buffer->str, sizeof(dest_buffer->str),
      "<html>"
      "<h1>POST request</h1>"
      "</html>");

  if(write_POST_response_headers(wsi, dest_buffer, start, p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}

int write_POST_response_headers(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;
  /* prepare and write http headers */
  if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
          "text/html", dest_buffer->len, &p, end))
    return 1;
  if (lws_finalize_write_http_header(wsi, start, &p, end))
    return 1;

  return false;
}

int handle_http_DELETE_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  dest_buffer->len = lws_snprintf(dest_buffer->str, sizeof(dest_buffer->str),
      "<html>"
      "<h1>DELETE request</h1>"
      "</html>");

  if(write_DELETE_response_headers(wsi, dest_buffer, start, p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}

int write_DELETE_response_headers(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;
  /* prepare and write http headers */
  if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
          "text/html", dest_buffer->len, &p, end))
    return 1;
  if (lws_finalize_write_http_header(wsi, start, &p, end))
    return 1;

  return false;
}
