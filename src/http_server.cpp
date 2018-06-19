#include "http_server.h"

#if defined (__cplusplus)
extern "C" {
#endif

  #include "globals.h"

#if defined (__cplusplus)
}
#endif

#include <regex>
#include <string>
using std::regex;
using std::string;
using std::smatch;

#include "application.h"
#include "urls.h"
#include "get_request.h"
#include "post_request.h"
#include "delete_request.h"
#include "put_request.h"
#include "app_request.h"

struct lws_protocols HttpServer::protocols[] = {
  /* name, callback, per_session_data_size, rx_buffer_size, id, user, tx_packet_size */
  { "http", rest_api_callback, sizeof(struct HttpRequest::user_buffer_data), HttpRequest::BUFFER_SIZE, 1, new struct HttpRequest::user_buffer_data, 0},
  { NULL, NULL, 0, 0, 0, NULL, 0 } /* terminator */
};

int HttpServer::run() {
  // const char *p;
  int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_PARSER
    /* for LLL_ verbosity above NOTICE to be built into lws,
     * lws must have been configured and built with
     * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
    /* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
    /* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
    /* | LLL_DEBUG */;

  signal(SIGINT, sigint_handler);

  // if ((p = lws_cmdline_option(argc, argv, "-d")))
  // logs = atoi(p);

  lws_set_log_level(logs, NULL);
  lwsl_user("LWS minimal http server | visit http://localhost:7681\n");

  memset(&info_, 0, sizeof info_); /* otherwise uninitialized garbage */

  info_.port = 7681;
  info_.mounts = &mount_;
  info_.protocols = protocols;
  info_.error_document_404 = "/404.html";

  context_ = lws_create_context(&info_);
  if (!context_) {
    lwsl_err("lws init failed\n");
    return 1;
  }

  while (n >= 0 && !interrupted)
		n = lws_service(context_, 1000);

  lws_context_destroy(context_);

  return 0;
}

// look reasons from https://libwebsockets.org/lws-api-doc-master/html/group__usercb.html
int HttpServer::rest_api_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len){
  struct HttpRequest::user_buffer_data *dest_buffer = (struct HttpRequest::user_buffer_data *)user;

  // filtering out some stuff that isn't handled
  if(reason ==  LWS_CALLBACK_HTTP_BIND_PROTOCOL ||
    reason == LWS_CALLBACK_FILTER_HTTP_CONNECTION ||
    reason == LWS_CALLBACK_CHECK_ACCESS_RIGHTS)
    return 0;

  if(reason == LWS_CALLBACK_HTTP) {
    for(unsigned int i = 0; i < ARRAY_SIZE(URL_MOUNTS); ++i) {
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
  struct HttpRequest::user_buffer_data *dest_buffer = (struct HttpRequest::user_buffer_data *)user;

  // Get request
  if (lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI)) {
    if(!dest_buffer->request)
      dest_buffer->request = new GetRequest();

    return ((GetRequest*)dest_buffer->request)->handleHttpRequest(wsi, dest_buffer, in, start, p, end, len, reason);
  }

  // Post request
  if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)) {
    if(!dest_buffer->request)
      dest_buffer->request = new PostRequest();

    return ((PostRequest*)dest_buffer->request)->handleHttpRequest(wsi, dest_buffer, in, start, p, end, len, reason);
  }

  // Delete request
  if(lws_hdr_total_length(wsi, WSI_TOKEN_DELETE_URI)) {
    if(!dest_buffer->request)
      dest_buffer->request = new DeleteRequest();

    return ((DeleteRequest*)dest_buffer->request)->handleHttpRequest(wsi, dest_buffer, in, start, p, end, len, reason);
  }

  // Put request
  if(lws_hdr_total_length(wsi, WSI_TOKEN_PUT_URI)) {
    if(!dest_buffer->request)
      dest_buffer->request = new PutRequest();

    return ((PutRequest*)dest_buffer->request)->handleHttpRequest(wsi, dest_buffer, in, start, p, end, len, reason);
  }

  return lws_callback_http_dummy(wsi, reason, user, in, len);
}

int HttpServer::app_rest_api(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
  uint8_t buf[LWS_PRE + 256], *start = &buf[LWS_PRE], *p = start,
    *end = &buf[sizeof(buf) - 1];
  struct HttpRequest::user_buffer_data *dest_buffer = (struct HttpRequest::user_buffer_data *)user;
  if(reason == LWS_CALLBACK_HTTP) {
    string url = (char*)in;
    int id = HttpRequest::parseIdFromURL(url);
    string ai = HttpRequest::parseApiFromURL(url);

    JSApplication *app = JSApplication::getApplicationById(id);

    if(app && app->hasAI(ai)) {
      if(!dest_buffer->request) {
        dest_buffer->request = new AppRequest();
      }
    }

    ((AppRequest*)dest_buffer->request)->setApp(app);
    ((AppRequest*)dest_buffer->request)->setAI(ai);
  }

  int cont = dest_buffer->request->handleHttpRequest(wsi, dest_buffer, in, start, p, end, len, reason);

  if(cont >= 0)
    return cont;
      // TODO find "class" or function to be called app->generateResponse(string function)
      // TODO maybe create respose and request libs to keep documentation easier
      // TODO probably just imagine implementation/interface for others to use

  return lws_callback_http_dummy(wsi, reason, user, in, len);
}
