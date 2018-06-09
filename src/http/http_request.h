#ifndef HTTP_REQUEST_H_INCLUDED
#define HTTP_REQUEST_H_INCLUDED

#define DEBUG 1

#include <libwebsockets.h>

#include <string>
using std::string;

class HttpRequest {
  public:
    HttpRequest() {}

    virtual ~HttpRequest() {}

    // http request defines the user Data
    struct user_buffer_data {
      char str[256];
      int len;
      struct lws_spa *spa;		/* lws helper decodes multipart form */
      char filename[128];		/* the filename of the uploaded file */
      unsigned long long file_length; /* the amount of bytes uploaded */
      int fd;				/* fd on file being saved */
      char ext_filename[128];
      string large_str;
      string request_url;
      string error_msg;
      HttpRequest *request;
    };

    // return: < 0 will we an error
    // >= 0 will have the same effect that libwebsockets has
    virtual int handleHttpRequest(struct lws *wsi, void* buffer_data, void* in) = 0;

    virtual int generateResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) = 0;

    virtual int generateFailResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) = 0;

  protected:

};

#endif
