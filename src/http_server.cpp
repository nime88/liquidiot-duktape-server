#include "http_server.h"

#include "globals.h"
#include "prints.h"

#include <regex>
#include <string>
#include <map>
#include <memory>

using std::regex;
using std::string;
using std::smatch;
using std::map;
using std::shared_ptr;

#include "application.h"
#include "urls.h"
#include "get_request.h"
#include "post_request.h"
#include "delete_request.h"
#include "put_request.h"
#include "app_request.h"
#include "util.h"
#include "http_request_structs.h"

struct lws_protocols HttpServer::protocols[] = {
  /* name, callback, per_session_data_size, rx_buffer_size, id, user, tx_packet_size */
  { Constant::String::PROTOCOL_HTTP, rest_api_callback, sizeof(struct user_buffer_data), HttpRequest::BUFFER_SIZE, 1, 0, 0},
  { NULL, NULL, 0, 0, 0, NULL, 0 } /* terminator */
};

int HttpServer::run() {
  int n = 0, logs = LLL_ERR | LLL_WARN
    /* for LLL_ verbosity above NOTICE to be built into lws,
     * lws must have been configured and built with
     * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
    /* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
    /* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
    /* | LLL_DEBUG */;

  signal(SIGINT, sigint_handler);

  lws_set_log_level(logs, NULL);

  DBOUT("Starting http server");

  memset(&info_, 0, sizeof info_); /* otherwise uninitialized garbage */

  // loading some configs
  duk_context *ctx = duk_create_heap_default();

  if (!ctx) {
    ERROUT("HttpServer: Duk context could not be created.");
    throw "Duk context could not be created.";
  }

  map<string,map<string,string> > config = get_config(ctx, Device::getInstance().getExecPath());
  duk_destroy_heap(ctx);
  ctx = 0;

  map<string,string> dev_config;

  if(config.find(Constant::Attributes::DEVICE) != config.end()) {
    dev_config = config.at(Constant::Attributes::DEVICE);
  } else {
    ERROUT("HttpServer: No device defined in config");
    throw "No device defined in config";
  }

  if(dev_config.find(Constant::Attributes::DEVICE_PORT) != dev_config.end()) {
    info_.port = stoi(dev_config.at(Constant::Attributes::DEVICE_PORT));
  } else {
    ERROUT("HttpServer: No port defined at device config");
    throw "No port defined at device config";
  }

  info_.mounts = &mount_;
  info_.protocols = protocols;
  info_.error_document_404 = "/404.html";

  context_ = lws_create_context(&info_);
  if (!context_) {
    lwsl_err("lws init failed\n");
    return 1;
  }

  DBOUT("HttpServer started at port " << info_.port);

  while (n >= 0 && !interrupted)
		n = lws_service(context_, 1000);

  lws_context_destroy(context_);

  return 0;
}

// look reasons from https://libwebsockets.org/lws-api-doc-master/html/group__usercb.html
int HttpServer::rest_api_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len){
  struct user_buffer_data *dest_buffer = (struct user_buffer_data *)user;

  // filtering out some stuff that isn't handled
  if(reason ==  LWS_CALLBACK_HTTP_BIND_PROTOCOL ||
    reason == LWS_CALLBACK_FILTER_HTTP_CONNECTION ||
    reason == LWS_CALLBACK_CHECK_ACCESS_RIGHTS)
    return 0;

  if(reason == LWS_CALLBACK_HTTP) {
    for(unsigned int i = 0; i < URL_MOUNTS_SIZE; ++i) {
      string url = (char*)in;
      smatch m;
      regex_match(url,m,URL_MOUNTS[i].url_rule);

      if(m.size() > 0) {
        dest_buffer->request_callback_index = i;
        return URL_MOUNTS[i].call_back(wsi, reason,user,in,len);
      }
    }
  }

  if(dest_buffer && dest_buffer->request_callback_index >= 0)
    return URL_MOUNTS[dest_buffer->request_callback_index].call_back(wsi, reason,user,in,len);

  return lws_callback_http_dummy(wsi, reason, user, in, len);
}

int HttpServer::server_rest_api(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
  uint8_t buf[LWS_PRE + 256], *start = &buf[LWS_PRE], *p = start,
    *end = &buf[sizeof(buf) - 1];
  struct user_buffer_data *dest_buffer = (struct user_buffer_data *)user;

  // Get request
  if (lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI)) {
    return GetRequest().handleHttpRequest(wsi, dest_buffer, in, start, p, end, len, reason);
  }

  // Post request
  if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)) {
    return PostRequest().handleHttpRequest(wsi, dest_buffer, in, start, p, end, len, reason);
  }

  // Delete request
  if(lws_hdr_total_length(wsi, WSI_TOKEN_DELETE_URI)) {

    return DeleteRequest().handleHttpRequest(wsi, dest_buffer, in, start, p, end, len, reason);
  }

  // Put request
  if(lws_hdr_total_length(wsi, WSI_TOKEN_PUT_URI)) {

    return PutRequest().handleHttpRequest(wsi, dest_buffer, in, start, p, end, len, reason);
  }

  return lws_callback_http_dummy(wsi, reason, user, in, len);
}

int HttpServer::app_rest_api(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
  uint8_t buf[LWS_PRE + 256], *start = &buf[LWS_PRE], *p = start,
    *end = &buf[sizeof(buf) - 1];
  struct user_buffer_data *dest_buffer = (struct user_buffer_data *)user;
  if(reason == LWS_CALLBACK_HTTP) {
    DBOUT(__func__ << ": LWS_CALLBACK_HTTP");
    string url = (char*)in;

    DBOUT(__func__ << ": parseId");
    int id = HttpRequest::parseIdFromURL(url);
    DBOUT(__func__ << ": parseAPI");
    string ai = "/" + HttpRequest::parseApiFromURL(url);

    DBOUT(__func__ << ": getApp, ai: " << ai);
    JSApplication *app = JSApplication::getApplicationById(id);

    DBOUT(__func__ << ": hasAI");
    if(app && app->hasAI(ai)) {
      DBOUT(__func__ << ": setting App");
      dest_buffer->app_request.setApp(app);
      DBOUT(__func__ << ": setting AI");
      dest_buffer->app_request.setAI(ai);
    }

    DBOUT(__func__ << ": LWS_CALLBACK_HTTP Ok");
  }

  int cont = dest_buffer->app_request.handleHttpRequest(wsi, dest_buffer, in, start, p, end, len, reason);

  if(cont >= 0)
    return cont;

  return lws_callback_http_dummy(wsi, reason, user, in, len);
}
