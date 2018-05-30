#include "http_server.h"

#include <iostream>
using namespace std;

struct lws_protocols HttpServer::protocols[] = {
  { "http", callback_dynamic_http, sizeof(struct pss), 0 },
  { NULL, NULL, 0, 0 } /* terminator */
};

int HttpServer::interrupted_;

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

  while (n >= 0 && !interrupted_)
		n = lws_service(context_, 1000);

  lws_context_destroy(context_);

  return 0;
}

// look reasons from https://libwebsockets.org/lws-api-doc-master/html/group__usercb.html
int HttpServer::callback_dynamic_http(struct lws *wsi, enum lws_callback_reasons reason,
  void *user, void *in, size_t len){
	struct pss *pss = (struct pss *)user;
	uint8_t buf[LWS_PRE + 256], *start = &buf[LWS_PRE], *p = start,
		*end = &buf[sizeof(buf) - 1];
	time_t t;

	switch (reason) {
	case LWS_CALLBACK_HTTP:

		/* in contains the url part after our mountpoint /dyn, if any */

		t = time(NULL);
		pss->len = lws_snprintf(pss->str, sizeof(pss->str),
				"<html>"
				"<img src=\"/libwebsockets.org-logo.png\">"
				"<br>Dynamic content for '%s' from mountpoint."
				"<br>Time: %s"
				"</html>", (const char *)in, ctime(&t));

		/* prepare and write http headers */
		if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
						"text/html", pss->len, &p, end))
			return 1;
		if (lws_finalize_write_http_header(wsi, start, &p, end))
			return 1;

		/* write the body separately */
		lws_callback_on_writable(wsi);

		return 0;

	case LWS_CALLBACK_HTTP_WRITEABLE:

		if (!pss || !pss->len)
			break;

		/*
		 * Use LWS_WRITE_HTTP for intermediate writes, on http/2
		 * lws uses this to understand to end the stream with this
		 * frame
		 */
		if (lws_write(wsi, (uint8_t *)pss->str, pss->len,
			      LWS_WRITE_HTTP_FINAL) != pss->len)
			return 1;

		/*
		 * HTTP/1.0 no keepalive: close network connection
		 * HTTP/1.1 or HTTP1.0 + KA: wait / process next transaction
		 * HTTP/2: stream ended, parent connection remains up
		 */
		if (lws_http_transaction_completed(wsi))
			return -1;

		return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}
