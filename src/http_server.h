#ifndef HTTP_SERVER_H_INCLUDED
#define HTTP_SERVER_H_INCLUDED

 #include <libwebsockets.h>
 #include <string.h>
 #include <signal.h>

 #include <iostream>
 using namespace std;

 #include "response.h"

 class HttpServer {
  public:
    HttpServer(){}
    int run();

  private:
    static string parseRequestType(void *in);

    struct lws_context_creation_info info_;
    struct lws_context *context_;

    struct user_buffer_data {
      char str[256];
      int len;
    };

    static struct lws_protocols protocols[];

    const struct lws_http_mount mount_rest_ = {
     /* .mount_next */		NULL,		/* linked-list "next" */
     /* .mountpoint */		"/app",		/* mountpoint URL */
     /* .origin */			NULL,
     /* .def */			NULL,
     /* .protocol */  "http",/* Protocol used */
     /* .cgienv */			NULL,
     /* .extra_mimetypes */		NULL,
     /* .interpret */		NULL,
     /* .cgi_timeout */		0,
     /* .cache_max_age */		0,
     /* .auth_mask */		0,
     /* .cache_reusable */		0,
     /* .cache_revalidate */		0,
     /* .cache_intermediaries */	0,
     /* .origin_protocol */		LWSMPRO_CALLBACK,	/* callback for protocol */
     /* .mountpoint_len */		4,		/* char count */
     /* .basic_auth_login_file */	NULL,
     /* ._unused */ {} /* Extra to suppress warnings */
    };

    const struct lws_http_mount mount_ = {
     /* .mount_next */		&mount_rest_,		/* linked-list "next" */
     /* .mountpoint */		"/",		/* mountpoint URL */
     /* .origin */			"./http-files", /* serve from dir */
     /* .def */			"index.html",	/* default filename */
     /* .protocol */			NULL,
     /* .cgienv */			NULL,
     /* .extra_mimetypes */		NULL,
     /* .interpret */		NULL,
     /* .cgi_timeout */		0,
     /* .cache_max_age */		0,
     /* .auth_mask */		0,
     /* .cache_reusable */		0,
     /* .cache_revalidate */		0,
     /* .cache_intermediaries */	0,
     /* .origin_protocol */		LWSMPRO_FILE,	/* files in a dir */
     /* .mountpoint_len */		1,		/* char count */
     /* .basic_auth_login_file */	NULL,
     /* ._unused */ {} /* Extra to suppress warnings */
   };

    static int rest_api_callback(struct lws *wsi, enum lws_callback_reasons reason,
      void *user, void *in, size_t len);

 };

#endif
