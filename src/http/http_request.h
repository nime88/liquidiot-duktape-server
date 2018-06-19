#ifndef HTTP_REQUEST_H_INCLUDED
#define HTTP_REQUEST_H_INCLUDED

#if defined (__cplusplus)
extern "C" {
#endif

  #include <libwebsockets.h>

#if defined (__cplusplus)
}
#endif

#include <string>
using std::string;
#include <map>
using std::map;

class HttpRequest {
  public:
    static const size_t BUFFER_SIZE;

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
      int buffer_idx;
      string request_url;
      string error_msg;
      HttpRequest *request;
      int request_callback_index = -1;
    };

    // return: < 0 will we an error
    // >= 0 will have the same effect that libwebsockets has
    virtual int handleHttpRequest(struct lws *wsi, void* buffer_data, void* in, uint8_t *start, uint8_t *p, uint8_t *end, size_t len, enum lws_callback_reasons reason) = 0;

    virtual int generateResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) = 0;

    virtual int generateFailResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) = 0;

    static void optimizeResponseString(string response, void* buffer_data);

    // return: -1 empty
    // return: -2 parse error
    static int parseIdFromURL(string url);

    static string parseApiFromURL(string url);
    static map<string,string> parseBodyAttributes(string body);

  protected:
    int writeHttpRequest(struct lws *wsi, void* buffer_data, void* in, size_t len);
    int handleDropProtocol(void* buffer_data);

};

#endif
