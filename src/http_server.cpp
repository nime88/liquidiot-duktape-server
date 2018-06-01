#include "http_server.h"

#if defined (__cplusplus)
extern "C" {
#endif

  #include "globals.h"

#if defined (__cplusplus)
}
#endif

#include <iostream>
using namespace std;

struct lws_protocols HttpServer::protocols[] = {
  /* name, callback, per_session_data_size, rx_buffer_size, id, user, tx_packet_size */
  { "http", rest_api_callback, 8096, 256, 1, new struct user_buffer_data, 256},
  { NULL, NULL, 0, 0, 0, NULL, 0 } /* terminator */
};

int HttpServer::run() {
  // const char *p;
  int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
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
  struct user_buffer_data *dest_buffer = (struct user_buffer_data *)user;

	switch (reason) {
  	case LWS_CALLBACK_HTTP: {
      // string wut = parseRequestType(in);
  		/* in contains the url part after our mountpoint /dyn, if any */

      if (lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI))
        /* GET request */
        return handle_http_GET_response(wsi, user, start, p, end);

      if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI))
        /* POST request */
        return handle_http_POST_response(wsi, user, start, p, end);

      if(lws_hdr_total_length(wsi, WSI_TOKEN_DELETE_URI))
        /* DELETE request */
        return handle_http_DELETE_response(wsi, user, start, p, end);

      return handle_404_response(wsi, user, start, p, end);
    }

  	case LWS_CALLBACK_HTTP_WRITEABLE: {

  		if (!dest_buffer || !dest_buffer->len)
  			break;

  		/*
  		 * Use LWS_WRITE_HTTP for intermediate writes, on http/2
  		 * lws uses this to understand to end the stream with this
  		 * frame
  		 */

  		if (lws_write(wsi, (uint8_t *)dest_buffer->str, dest_buffer->len,
  			      LWS_WRITE_HTTP_FINAL) != dest_buffer->len)
  			return 1;

  		/*
  		 * HTTP/1.0 no keepalive: close network connection
  		 * HTTP/1.1 or HTTP1.0 + KA: wait / process next transaction
  		 * HTTP/2: stream ended, parent connection remains up
  		 */
  		if (lws_http_transaction_completed(wsi))
  			return -1;

  		return 0;
    }

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}
