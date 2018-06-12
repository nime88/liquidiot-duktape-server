#ifndef POST_REQUEST_H_INCLUDED
#define POST_REQUEST_H_INCLUDED

#include "http_request.h"

class PostRequest : public HttpRequest {
  public:
    int handleHttpRequest(struct lws *wsi, void* buffer_data, void* in, uint8_t *start, uint8_t *p, uint8_t *end, size_t len, enum lws_callback_reasons reason);

    int generateResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

    int generateFailResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end);

  private:
    int parsePostForm(struct lws *wsi, void* buffer_data, void* in, size_t len);

    static int parsePostFormCB(void *data, const char *name, const char *filename, char *buf, int len, enum lws_spa_fileupload_states state);

    int calculateHttpRequest(void* buffer_data, void* in);

    // defining post parameters
    static const char * const post_param_names[];

    enum post_enum_param_names {
    	EPN_FILEKEY=0,
    };
};

#endif
