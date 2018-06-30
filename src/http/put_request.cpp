#include "put_request.h"

#include "application.h"
#include "constant.h"

#include <string>
using std::to_string;

const char * const PutRequest::put_param_names[] = {
  "status",
};

int PutRequest::handleHttpRequest(struct lws *wsi, void* buffer_data, void* in, uint8_t *start, uint8_t *p, uint8_t *end, size_t len, enum lws_callback_reasons reason) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  switch (reason) {
    // initial http request (headers)
    case LWS_CALLBACK_HTTP: {
      dest_buffer->request_url = (char*)in;
      return 0;
    }

    case LWS_CALLBACK_HTTP_BODY: {
      if (!dest_buffer->spa) {
        dest_buffer->spa = lws_spa_create(wsi, put_param_names, ARRAY_SIZE(put_param_names), 1024, NULL, NULL); /* no file upload */

        if (!dest_buffer->spa)
          return -1;
      }

      if (lws_spa_process(dest_buffer->spa, (char *)in, (int)len))
        return -1;

      break;
    }

    case LWS_CALLBACK_HTTP_BODY_COMPLETION: {
      /* inform the spa no more payload data coming */

      lws_spa_finalize(dest_buffer->spa);

      int get_resp = calculateHttpRequest(buffer_data, in);
      if(get_resp == 0) {
        return generateResponse(wsi, buffer_data, start, p, end);
      } else if(get_resp < 0) {
        return generateFailResponse(wsi, buffer_data, start, p, end);
      }

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

int PutRequest::generateResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
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

int PutRequest::generateFailResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
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

int PutRequest::calculateHttpRequest(void* buffer_data, void* in) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  int id =  parseIdFromURL(dest_buffer->request_url);

  (void)in;

  // can instantly fail the request if not valid id
  if(id == -1) {
    dest_buffer->error_msg = "No application with given id.";
    optimizeResponseString(dest_buffer->error_msg, buffer_data);
    return -1;
  } else if( id == -2 ) {
    dest_buffer->error_msg = "No application with id '" + dest_buffer->request_url + "'.";
    optimizeResponseString(dest_buffer->error_msg, buffer_data);
    return -1;
  }

  // now we have to find the applications
  map<duk_context*, JSApplication*> apps = JSApplication::getApplications();
  JSApplication *app = 0;

  for (  map<duk_context*, JSApplication*>::const_iterator it=apps.begin(); it!=apps.end(); ++it) {
    if(it->second->getAppId() == id) {
      app = it->second;
      break;
    }
  }

  if(!app) {
    dest_buffer->error_msg = "No application with id '" + to_string(id) + "'.";
    optimizeResponseString(dest_buffer->error_msg, buffer_data);
    return -1;
  }

  JSApplication::APP_STATES state;
  string parsed_str;

  for (int n = 0; n < (int)ARRAY_SIZE(put_param_names); n++) {
    if (!lws_spa_get_string(dest_buffer->spa, n)) {
      dest_buffer->error_msg = "Attribute couldn't be read";
      optimizeResponseString(dest_buffer->error_msg, buffer_data);
      return -1;
    }

    parsed_str = lws_spa_get_string(dest_buffer->spa, n);
  }

  for(unsigned int i = 0; i < app->APP_STATES_CHAR.size(); ++i) {
    if(app->APP_STATES_CHAR.at(i) == parsed_str) {
      state = JSApplication::APP_STATES(i);
      break;
    }
  }

  bool app_state_ok = app->setAppState(state);

  if(app_state_ok) {
    dest_buffer->large_str = "Application went to target status successfully.";
    optimizeResponseString(dest_buffer->large_str, buffer_data);

    return 0;
  }

  dest_buffer->error_msg = "Something went wrong";
  optimizeResponseString(dest_buffer->error_msg, buffer_data);

  return -1;
}
