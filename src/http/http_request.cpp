#include "http_request.h"

#include <stdexcept>
#include <algorithm>

using std::invalid_argument;
using std::remove;

int HttpRequest::writeHttpRequest(struct lws *wsi, void* buffer_data, void* in, size_t len) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data *)buffer_data;

  if (!dest_buffer || !dest_buffer->len)
    return lws_callback_http_dummy(wsi, LWS_CALLBACK_HTTP_WRITEABLE, buffer_data, in, len);

  /*
   * Use LWS_WRITE_HTTP for intermediate writes, on http/2
   * lws uses this to understand to end the stream with this
   * frame
   */
  if(dest_buffer->large_str.length() > 0) {
    if (lws_write(wsi, (uint8_t *)dest_buffer->large_str.c_str(), dest_buffer->large_str.length(),
            LWS_WRITE_HTTP_FINAL) != (int)dest_buffer->large_str.length()) {
      return 1;
    }
  } else if (lws_write(wsi, (uint8_t *)dest_buffer->str, dest_buffer->len,
          LWS_WRITE_HTTP_FINAL) != dest_buffer->len) {
    return 1;
  }

  /*
   * HTTP/1.0 no keepalive: close network connection
   * HTTP/1.1 or HTTP1.0 + KA: wait / process next transaction
   * HTTP/2: stream ended, parent connection remains up
   */
  if (lws_http_transaction_completed(wsi))
    return -1;

  return 0;
}

int HttpRequest::handleDropProtocol(void* buffer_data) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data *)buffer_data;
  /* called when our wsi user_space is going to be destroyed */
  if (dest_buffer->spa) {
    lws_spa_destroy(dest_buffer->spa);
    dest_buffer->spa = NULL;
  }

  return 0;
}

void HttpRequest::optimizeResponseString(string response, void* buffer_data) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data *)buffer_data;

  dest_buffer->len = response.length();
  if(dest_buffer->len < ARRAY_SIZE(dest_buffer->str)) {
    strcpy(dest_buffer->str,response.c_str());
    if(dest_buffer->large_str.length() > 0)
      dest_buffer->large_str = "";
  } else {
    dest_buffer->large_str = response;
  }
}

int HttpRequest::parseIdFromURL(string url) {
  string temp_request_url = url;

  // removing '/' so we can parse int
  temp_request_url.erase(remove(temp_request_url.begin(), temp_request_url.end(), '/'), temp_request_url.end());

  int id = -1;

  if(temp_request_url.length() > 0) {
    try {
      id = stoi(temp_request_url);
    } catch( invalid_argument e) {
      id = -2;
    }
  }

  return id;
}
