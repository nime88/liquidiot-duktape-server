#ifndef HTTP_CLIENT_H_INCLUDED
#define HTTP_CLIENT_H_INCLUDED

#include <libwebsockets.h>

#include "client_request_config.h"
#include "constant.h"

class HttpClient {
  public:
    static const size_t BUFFER_SIZE;

    static int http_client_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

    /* ******************* */
    /* Setters and Getters */
    /* ******************* */

    inline ClientRequestConfig* getCRConfig() { return crconfig_; }

    inline bool isReady() { return ready_; }
    inline void setReady(bool ready) { ready_ = ready; }

    HttpClient();
    ~HttpClient();

    void run(ClientRequestConfig *config);

  private:

    struct user_buffer_data {
      char str[256];
      int len;
  	  char boundary[32];
      char body_part;
      string raw_data;
      unsigned int buffer_idx = 0;
      ClientRequestConfig *config;
      HttpClient *client;
    };

    user_buffer_data *data_;

    static const struct lws_protocols protocols[];

    struct lws *client_wsi;

    ClientRequestConfig *crconfig_;
    int status_;

    bool ready_ = false;

};

#endif
