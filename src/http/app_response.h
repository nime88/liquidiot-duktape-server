#ifndef APP_RESPONSE_H_INCLUDED
#define APP_RESPONSE_H_INCLUDED
// Simplified version of response

#include <string>
using std::string;
#include <map>
using std::map;

#include "http_request.h"

class AppResponse {
  public:
    void setHeaders(map<string,string> headers) { headers_ = headers; }

    void setStatusCode(int status_code) { status_code_ = status_code; }

    void setStatusMessage(string message) { status_message_ = message; }

    void setContent(string content) { content_body_ = content; }

    string getContent() { return content_body_; }

    int generateResponseHeaders(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

  private:
    map<string,string> headers_;
    int status_code_;
    string status_message_;
    string content_body_;

};

#endif
