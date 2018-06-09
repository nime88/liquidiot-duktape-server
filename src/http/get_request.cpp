#include "get_request.h"

#include "application.h"

int GetRequest::handleHttpRequest(struct lws *wsi, void* buffer_data, void* in) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;
  map<duk_context*, JSApplication*> apps = JSApplication::getApplications();
  JSApplication *app = 0;

  dest_buffer->request_url = (char*)in;
  string temp_request_url = dest_buffer->request_url;

  // removing '/' so we can parse int in legit case
  temp_request_url.erase(std::remove(temp_request_url.begin(), temp_request_url.end(), '/'), temp_request_url.end());
  // setting default id to be -1 for later use if int could not be parsed
  int id = -1;

  // if there  is something to we try parse the id (otherwise list everything else)
  if(temp_request_url.length() > 0) {
    try {
      id = stoi(temp_request_url);
    } catch( invalid_argument e) {
      id = -2;
    }
  } else {
    dest_buffer->large_str = "[\n";
    for (  map<duk_context*, JSApplication*>::const_iterator it=apps.begin(); it!=apps.end(); ++it) {
      dest_buffer->large_str += it->second->getDescriptionAsJSON();
      dest_buffer->large_str += ",\n";
    }
    dest_buffer->large_str += "]";
    dest_buffer->len = dest_buffer->large_str.length();
    if(dest_buffer->len < ARRAY_SIZE(dest_buffer->str)) {
      strcpy(dest_buffer->str,dest_buffer->large_str.c_str());
      dest_buffer->large_str = "";
    }

    return 0;
  }

  // handling parse error
  if(id == -2) {
    dest_buffer->error_msg = "No application with id '" + temp_request_url + "'.";
    dest_buffer->len = dest_buffer->error_msg.length();
    if(dest_buffer->len < ARRAY_SIZE(dest_buffer->str)) {
      strcpy(dest_buffer->str,dest_buffer->error_msg.c_str());

      if(dest_buffer->large_str.length() > 0)
        dest_buffer->large_str = "";

    } else {
      dest_buffer->large_str = dest_buffer->error_msg;
    }

    return -1;
  }

  for (  map<duk_context*, JSApplication*>::const_iterator it=apps.begin(); it!=apps.end(); ++it) {
    if(it->second->getId() == id) {
      dest_buffer->large_str = it->second->getDescriptionAsJSON();
      dest_buffer->len = dest_buffer->large_str.length();
      if(dest_buffer->len < ARRAY_SIZE(dest_buffer->str)) {
        strcpy(dest_buffer->str,dest_buffer->large_str.c_str());
        dest_buffer->large_str = "";
      }

      return 0;
    }
  }

  dest_buffer->error_msg = "No application with id '" + to_string(id) + "'.";
  dest_buffer->len = dest_buffer->error_msg.length();
  if(dest_buffer->len < ARRAY_SIZE(dest_buffer->str)) {
    strcpy(dest_buffer->str,dest_buffer->error_msg.c_str());
    if(dest_buffer->large_str.length() > 0)
      dest_buffer->large_str = "";
  }

  return -1;
}

int GetRequest::generateResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  /* prepare and write http headers */
  if(lws_add_http_common_headers(wsi, HTTP_STATUS_OK, "text/html", dest_buffer->len, &p, end)) {
    return 1;
  }

  if (lws_finalize_write_http_header(wsi, start, &p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}

int GetRequest::generateFailResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;
  /* prepare and write http headers */
  if (lws_add_http_common_headers(wsi, HTTP_STATUS_NOT_FOUND, "text/html", dest_buffer->len, &p, end)) {
    return 1;
  }

  if (lws_finalize_write_http_header(wsi, start, &p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}
