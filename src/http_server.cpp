#include "http_server.h"

#if defined (__cplusplus)
extern "C" {
#endif

  #include "globals.h"

#if defined (__cplusplus)
}
#endif

#include <iostream>
#include <algorithm>
using namespace std;

#include "get_request.h"
#include "post_request.h"
#include "delete_request.h"
#include "put_request.h"

struct lws_protocols HttpServer::protocols[] = {
  /* name, callback, per_session_data_size, rx_buffer_size, id, user, tx_packet_size */
  { "http", rest_api_callback, sizeof(struct HttpRequest::user_buffer_data), 1024, 1, new struct HttpRequest::user_buffer_data, 1024},
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
int HttpServer::rest_api_callback(struct lws *wsi, enum lws_callback_reasons reason,
  void *user, void *in, size_t len){
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
