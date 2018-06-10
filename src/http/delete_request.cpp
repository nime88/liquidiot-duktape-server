#include "delete_request.h"

#include "application.h"

int DeleteRequest::handleHttpRequest(struct lws *wsi, void* buffer_data, void* in, uint8_t *start, uint8_t *p, uint8_t *end, size_t len, enum lws_callback_reasons reason) {
  switch (reason) {
    // initial http request (headers)
    case LWS_CALLBACK_HTTP: {
      int get_resp = calculateHttpRequest(wsi, buffer_data, in);
      if(get_resp == 0) {
        return generateResponse(wsi, buffer_data, start, p, end);
      } else if(get_resp < 0) {
        return generateFailResponse(wsi, buffer_data, start, p, end);
      }

      break;
    }

    case LWS_CALLBACK_HTTP_WRITEABLE: {
      // writing request when asked to
      return writeHttpRequest(wsi, buffer_data, in, len);
    }

    case LWS_CALLBACK_HTTP_DROP_PROTOCOL: {
      return handleDropProtocol(buffer_data);
    }

    default: {
      break;
    }
  }

  return lws_callback_http_dummy(wsi, reason, buffer_data, in, len);
}

int DeleteRequest::generateResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
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

int DeleteRequest::generateFailResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
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

int DeleteRequest::calculateHttpRequest(struct lws *wsi, void* buffer_data, void* in) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  dest_buffer->request_url = (char*)in;

  int id =  parseIdFromURL(dest_buffer->request_url);
  int deleted_apps = 0;
  map<duk_context*, JSApplication*> apps = JSApplication::getApplications();

  if(id == -1) {
    int app_amount = apps.size();

    for (  map<duk_context*, JSApplication*>::const_iterator it=apps.begin(); it!=apps.end(); ++it) {
      deleted_apps += it->second->deleteApp();
    }

    if(deleted_apps == app_amount) {
      dest_buffer->large_str = "All apps were deleted successfully";
    } else {
      dest_buffer->large_str = to_string(deleted_apps) + " of " + to_string(app_amount) + " app were deleted successfully.";
    }
  } else if(id >= 0) {
    for (  map<duk_context*, JSApplication*>::const_iterator it=apps.begin(); it!=apps.end(); ++it) {
      if(id == it->second->getId()) {
        deleted_apps += it->second->deleteApp();
        if(deleted_apps) {
          dest_buffer->large_str = "Application " + to_string(id) + " was deleted successfully.";
        }
      }
    }

    if(!deleted_apps) {
      dest_buffer->error_msg = "No application with id '" + to_string(id) + "'.";
      optimizeResponseString(dest_buffer->error_msg, buffer_data);
      return -1;
    }
  }

  if(id == -2) {
    dest_buffer->error_msg = "No application with id '" + dest_buffer->request_url + "'.";
    optimizeResponseString(dest_buffer->error_msg, buffer_data);
    return -1;
  }

  dest_buffer->len = dest_buffer->large_str.length();
  optimizeResponseString(dest_buffer->large_str, buffer_data);

  return 0;
}
