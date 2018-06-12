#ifndef GET_REQUEST_H_INCLUDED
#define GET_REQUEST_H_INCLUDED

#include "http_request.h"

class GetRequest : public HttpRequest {
  public:

    int handleHttpRequest(struct lws *wsi, void* buffer_data, void* in, uint8_t *start, uint8_t *p, uint8_t *end, size_t len, enum lws_callback_reasons reason);

    int generateResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

    int generateJSONResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

    int generateFailResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

  private:
    int calculateHttpRequest(void* buffer_data, void* in);

    // parses url to check if logs are asked
    bool hasLogs(string url);

};

#endif
