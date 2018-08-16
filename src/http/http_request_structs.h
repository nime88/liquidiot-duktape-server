#ifndef HTTP_REQUEST_ST_INCLUDED_H
#define HTTP_REQUEST_ST_INCLUDED_H

#include <memory>
using std::shared_ptr;

#include "get_request.h"
#include "post_request.h"
#include "delete_request.h"
#include "put_request.h"
#include "app_request.h"

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
    AppRequest app_request;

    int request_callback_index = -1;
};

#endif
