#include "get_request.h"

#include "application.h"
#include "constant.h"

#include <regex>
#include <string>

using std::to_string;

int GetRequest::handleHttpRequest(struct lws *wsi, void* buffer_data, void* in, uint8_t *start, uint8_t *p, uint8_t *end, size_t len, enum lws_callback_reasons reason) {
  switch (reason) {
    // initial http request (headers)
    case LWS_CALLBACK_HTTP: {
      int get_resp = calculateHttpRequest(buffer_data, in);
      if(get_resp == 0) {
        return generateResponse(wsi, buffer_data, start, p, end);
      } else if(get_resp < 0) {
        return generateFailResponse(wsi, buffer_data, start, p, end);
      } else if(get_resp == 1) {
        return generateJSONResponse(wsi, buffer_data, start, p, end);
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

int GetRequest::generateResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  /* prepare and write http headers */
  if(lws_add_http_common_headers(wsi, HTTP_STATUS_OK, Constant::String::REQ_TYPE_TEXT_HTML, dest_buffer->len, &p, end)) {
    return 1;
  }

  if (lws_finalize_write_http_header(wsi, start, &p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}

int GetRequest::generateJSONResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  /* prepare and write http headers */
  if(lws_add_http_common_headers(wsi, HTTP_STATUS_OK, Constant::String::REQ_TYPE_APP_JSON, dest_buffer->len, &p, end)) {
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
  if (lws_add_http_common_headers(wsi, HTTP_STATUS_NOT_FOUND, Constant::String::REQ_TYPE_TEXT_HTML, dest_buffer->len, &p, end)) {
    return 1;
  }

  if (lws_finalize_write_http_header(wsi, start, &p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}

int GetRequest::calculateHttpRequest(void* buffer_data, void* in) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  map<duk_context*, JSApplication*> apps = JSApplication::getApplications();
  JSApplication *app = 0;

  dest_buffer->request_url = (char*)in;

  int id =  parseIdFromURL(dest_buffer->request_url);

  // if there  is something to we try parse the id (otherwise list everything else)
  if(id == -1) {
    dest_buffer->large_str = "[\n";
    for (  map<duk_context*, JSApplication*>::const_iterator it=apps.begin(); it!=apps.end(); ++it) {
      dest_buffer->large_str += it->second->getDescriptionAsJSON();
      dest_buffer->large_str += ",\n";
    }
    if(apps.size() > 0)
      dest_buffer->large_str.erase(dest_buffer->large_str.length()-2,2);
    dest_buffer->large_str += "]";

    optimizeResponseString(dest_buffer->large_str, buffer_data);

    // sending JSON
    return 1;
  }

  // handling parse error
  if(id == -2) {
    dest_buffer->error_msg = "No application with id '" + dest_buffer->request_url + "'.";

    optimizeResponseString(dest_buffer->error_msg, buffer_data);

    // error
    return -1;
  }

  for (  map<duk_context*, JSApplication*>::const_iterator it=apps.begin(); it!=apps.end(); ++it) {
    if(it->second->getAppId() == id) {
      app = it->second;
      break;
    }
  }

  if(app) {
    if(!hasLogs(string(dest_buffer->request_url))) {
      dest_buffer->large_str = app->getDescriptionAsJSON();

      optimizeResponseString(dest_buffer->large_str, buffer_data);

      // sending JSON
      return 1;
    } else {
      string logs = app->getLogsAsJSON();
      dest_buffer->large_str = logs;

      optimizeResponseString(dest_buffer->large_str, buffer_data);

      // sending JSON
      return 1;
    }
  }

  dest_buffer->error_msg = "No application with id '" + to_string(id) + "'.";

  optimizeResponseString(dest_buffer->error_msg, buffer_data);

  // error
  return -1;
}

bool GetRequest::hasLogs(string url) {
  std::smatch m;
  std::regex e("^[/]?(\\d+)[/](log)[/]?$");
  std::regex_match(url,m,e);

  if(m.size() > 0) {
    return true;
  }

  return false;
}
