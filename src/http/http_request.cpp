#include "http_request.h"

#include <stdexcept>
#include <algorithm>
#include <regex>

#include <iostream>
using namespace std;

using std::invalid_argument;
using std::remove;


const size_t HttpRequest::BUFFER_SIZE = 4096;

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
    // handling larger string by splitting it apart and writing it in chunks
    string write_buffer = dest_buffer->large_str.substr( dest_buffer->buffer_idx, HttpRequest::BUFFER_SIZE);
    dest_buffer->buffer_idx += HttpRequest::BUFFER_SIZE;

    if(dest_buffer->buffer_idx < dest_buffer->len && write_buffer.length() == HttpRequest::BUFFER_SIZE) {
      char *out = (char *)malloc(sizeof(char)*(LWS_SEND_BUFFER_PRE_PADDING + write_buffer.length() + LWS_SEND_BUFFER_POST_PADDING));
      memcpy (out + LWS_SEND_BUFFER_PRE_PADDING, write_buffer.c_str(), write_buffer.length() );
      if (lws_write( wsi, (uint8_t *)out + LWS_SEND_BUFFER_PRE_PADDING, write_buffer.length(), LWS_WRITE_HTTP ) < 0) {
        free(out);
        return 1;
      }

      free(out);
      return lws_callback_on_writable(wsi);
    }
    else {
      // if string
      if (lws_write(wsi, (uint8_t *)write_buffer.c_str(), write_buffer.length(), LWS_WRITE_HTTP_FINAL) < 0) return 1;
    }
  } else if (dest_buffer->large_str.length() == 0 && lws_write(wsi, (uint8_t *)dest_buffer->str, dest_buffer->len, LWS_WRITE_HTTP_FINAL) != dest_buffer->len) {
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
  if(dest_buffer->len < (int)ARRAY_SIZE(dest_buffer->str)) {
    strcpy(dest_buffer->str,response.c_str());
    if(dest_buffer->large_str.length() > 0)
      dest_buffer->large_str = "";
  } else {
    dest_buffer->large_str = response;
  }
}

int HttpRequest::parseIdFromURL(string url) {
  std::smatch m_id;
  std::regex e_id("^[/]?(\\d+)([/](log)[/]?)?$");
  std::regex_match(url,m_id,e_id);

  int id = -1;

  if(m_id.size() > 1) {
    try {
      id = stoi(m_id[1]);
    } catch( invalid_argument e) {
      id = -2;
    }

    return id;
  }

  std::smatch m_no;
  std::regex e_no("(^[/]?$)");
  std::regex_match(url,m_no,e_no);

  if(m_no.size() > 0) {
    return -1;
  }

  return -2;
}
