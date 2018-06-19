#ifndef APP_REQUEST_H_INCLUDED
#define APP_REQUEST_H_INCLUDED
// Simplified version of request

#include <libwebsockets.h>

#include <string>
using std::string;

#include <map>
using std::map;
using std::pair;

#include "http_request.h"
class JSApplication;
#include "../application.h"

class AppRequest : public HttpRequest {
  public:
    enum request_types { UNDEFINED=0,GET, POST, PUT, PATCH, DELETE, COPY, HEAD, OPTIONS, LINK, UNLINK, PURGE, LOCK, UNLOCK, PROPFIND, VIEW};
    const char* request_types_char[16] = {
      "UNDEFINED","GET", "POST", "PUT", "PATCH", "DELETE", "COPY", "HEAD", "OPTIONS", "LINK", "UNLINK", "PURGE", "LOCK", "UNLOCK", "PROPFIND", "VIEW"
    };

    // setters
    void setRequestType(request_types type) {
      request_type_ = type;
    }

    void setApp(JSApplication *app) {
      app_ = app;
    }

    void setAI(string ai) {
      application_interface_ = ai;
    }

    // getters
    request_types getRequestType() {
      return request_type_;
    }

    string getAI() {
      return application_interface_;
    }

    const char* requestTypeToString(request_types type) {
      return request_types_char[type];
    }

    map<string,string> getHeaders() { return headers_; }
    string getProtocol() { return protocol_; }
    map<string,string> getBodyArguments() { return body_args_; }

    int handleHttpRequest(struct lws *wsi, void* buffer_data, void* in, uint8_t *start, uint8_t *p, uint8_t *end, size_t len, enum lws_callback_reasons reason);

    int generateResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

    int generateFailResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

    request_types getRequestTypeByName(const char* name) {
      for( int i = 0; i < ARRAY_SIZE(request_types_char);++i) {
        if(string(request_types_char[i]) == string(name)) {
          return request_types(i);
        }
      }

      return request_types(0);
    }

  private:
    request_types request_type_ = request_types::UNDEFINED;
    map<string,string> body_args_;
    JSApplication *app_;
    string application_interface_;

    // header stuff
    map<string,string> headers_;

    // other
    string protocol_;

    // raw input body
    string input_body_;

    map<string,string> parseUrlArgs(struct lws *wsi, void* buffer_data);
};

#endif
