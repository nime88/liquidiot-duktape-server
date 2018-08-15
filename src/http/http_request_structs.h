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
    shared_ptr<string> large_str = shared_ptr<string>(new string);
    int buffer_idx;
    shared_ptr<string> request_url = shared_ptr<string>(new string);
    shared_ptr<string> error_msg = shared_ptr<string>(new string);
    shared_ptr<GetRequest> get_request = shared_ptr<GetRequest>(new GetRequest);
    shared_ptr<PostRequest> post_request = shared_ptr<PostRequest>(new PostRequest);
    shared_ptr<DeleteRequest> delete_request = shared_ptr<DeleteRequest>(new DeleteRequest);
    shared_ptr<PutRequest> put_request = shared_ptr<PutRequest>(new PutRequest);
    shared_ptr<AppRequest> app_request = shared_ptr<AppRequest>(new AppRequest);

    int request_callback_index = -1;
};

#endif
