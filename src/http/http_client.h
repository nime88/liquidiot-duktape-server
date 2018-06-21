#ifndef HTTP_CLIENT_H_INCLUDED
#define HTTP_CLIENT_H_INCLUDED

#include <libwebsockets.h>
#include "client_request_config.h"

class HttpClient {
  public:
    static const size_t BUFFER_SIZE;

    static int http_client_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

    HttpClient(){}
    void run(ClientRequestConfig *config);

  private:

    struct user_buffer_data {
  	   char boundary[32];
       char body_part;
       int connected_status;
    };

    static const struct lws_protocols protocols[];

    static struct lws *client_wsi;

};

#endif
