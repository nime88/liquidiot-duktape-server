#include "app_response.h"

#include "constant.h"

int AppResponse::generateResponseHeaders(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct HttpRequest::user_buffer_data *dest_buffer = (struct HttpRequest::user_buffer_data*)buffer_data;

  string content_type = Constant::String::REQ_TYPE_TEXT_HTML;
  if(headers_.find(Constant::Attributes::AR_HEAD_CONTENT_LENGTH) != headers_.end()) {
    content_type = headers_.at(Constant::Attributes::AR_HEAD_CONTENT_LENGTH);
  }

  /* prepare and write http headers */
  if(lws_add_http_common_headers(wsi, status_code_, content_type.c_str(), dest_buffer->len, &p, end)) {
    return 1;
  }

  if (lws_finalize_write_http_header(wsi, start, &p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}
