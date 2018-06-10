#ifndef PUT_REQUEST_H_INCLUDED
#define PUT_REQUEST_H_INCLUDED

#include "http_request.h"

class PutRequest : public HttpRequest {
    public:

      int handleHttpRequest(struct lws *wsi, void* buffer_data, void* in, uint8_t *start, uint8_t *p, uint8_t *end, size_t len, enum lws_callback_reasons reason);

      int generateResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

      int generateFailResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

    private:

      int calculateHttpRequest(struct lws *wsi, void* buffer_data, void* in);

      // defining post parameters
      static const char * const put_param_names[];

      enum put_enum_param_names {
        EPN_STATUS=0,
      };
};

#endif
