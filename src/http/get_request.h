#ifndef GET_REQUEST_H_INCLUDED
#define GET_REQUEST_H_INCLUDED

#include "http_request.h"

class GetRequest : public HttpRequest {
  public:

    int handleHttpRequest(struct lws *wsi, void* buffer_data, void* in);

    int generateResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

    int generateFailResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

};

#endif
