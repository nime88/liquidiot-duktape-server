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

#include "http_request.h"
#include "get_request.h"

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

	switch (reason) {
  	case LWS_CALLBACK_HTTP: {
      cout << "LWS_CALLBACK_HTTP" << endl;
  		/* in contains the url part after our mountpoint /api, if any */

      if (lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI)) {
        if(!dest_buffer->request)
          dest_buffer->request = new GetRequest();

        int get_resp = ((GetRequest*) (dest_buffer->request))->handleHttpRequest(wsi, dest_buffer,in);
        if(get_resp == 0) {
          get_resp = ((GetRequest*) (dest_buffer->request))->generateResponse(wsi, dest_buffer, start, p, end);
          return get_resp;
        } else if(get_resp < 0) {
          get_resp = ((GetRequest*)(dest_buffer->request))->generateFailResponse(wsi, dest_buffer, start, p, end);
          return get_resp;
        }
      }

      if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)) {
        // storing the request url from the mountpoint
        dest_buffer->request_url = (char*)in;

        /* POST request */
        return 0;

      }

      if(lws_hdr_total_length(wsi, WSI_TOKEN_DELETE_URI))
        // storing the request url from the mountpoint
        dest_buffer->request_url = (char*)in;

        cout << "Completing DELETE" << endl;
        /* DELETE request */
        int del_resp = handle_http_DELETE_response(wsi, user, start, p, end);
        cout << "Error msg: " << dest_buffer->error_msg << endl;
        if(dest_buffer->error_msg.length() > 0) {
          return handle_http_DELETE_fail_response(wsi, user, start, p, end);
        }

        return del_resp;

      return handle_404_response(wsi, user, start, p, end);
    }

  	case LWS_CALLBACK_HTTP_WRITEABLE: {
      cout << "LWS_CALLBACK_HTTP_WRITEABLE" << endl;

  		if (!dest_buffer || !dest_buffer->len)
  			break;


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

    // handling reading of the body
    case LWS_CALLBACK_HTTP_BODY: {
      cout << "LWS_CALLBACK_HTTP_BODY" << endl;
      if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)) {
        /* POST request */
        int post_form = handle_http_POST_form(wsi, user, in, len);

        if(dest_buffer->error_msg.length() > 0) {
          // if we have an error, just continue to body completion
          return 0;
        }

        if(post_form)
          return post_form;
      }

      break;
    }

    // finishing up the form reading
    case LWS_CALLBACK_HTTP_BODY_COMPLETION: {
      cout << "LWS_CALLBACK_HTTP_BODY_COMPLETION" << endl;

      if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)) {
        if(dest_buffer->error_msg.length() > 0) {
          return handle_http_POST_fail_response(wsi, user, start, p, end);
        }

        cout << "Completing POST form" << endl;
        /* POST request */
        /* inform the spa no more payload data coming */
        lws_spa_finalize(dest_buffer->spa);

        // generating response
        int post_resp = handle_http_POST_response(wsi, user, start, p, end);

        if(dest_buffer->error_msg.length() > 0) {
          return handle_http_POST_fail_response(wsi, user, start, p, end);
        }

        return post_resp;
      }

      if (lws_hdr_total_length(wsi, WSI_TOKEN_DELETE_URI)) {

      }

      break;
    }

    case LWS_CALLBACK_HTTP_DROP_PROTOCOL: {
      cout << "LWS_CALLBACK_HTTP_DROP_PROTOCOL" << endl;
      /* called when our wsi user_space is going to be destroyed */
      if (dest_buffer->spa) {
        lws_spa_destroy(dest_buffer->spa);
        dest_buffer->spa = NULL;
      }
      break;
    }

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}
