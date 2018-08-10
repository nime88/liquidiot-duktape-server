#include "app_request.h"

#include <regex>
using std::regex;

#include <iostream>

#include "app_response.h"
#include "constant.h"

int AppRequest::handleHttpRequest(struct lws *wsi, void* buffer_data, void* in, uint8_t *start, uint8_t *p, uint8_t *end, size_t len, enum lws_callback_reasons reason) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  if(request_type_ == request_types::UNDEFINED) {
    if(lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI)) {
      request_type_ = getRequestTypeByName("GET");
    }
    else if(lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)) {
      request_type_ = getRequestTypeByName("POST");
    }
    else if(lws_hdr_total_length(wsi, WSI_TOKEN_PUT_URI)) {
      request_type_ = getRequestTypeByName("PUT");
    }
    else if(lws_hdr_total_length(wsi, WSI_TOKEN_DELETE_URI)) {
      request_type_ = getRequestTypeByName("DELETE");
    }
    else if(lws_hdr_total_length(wsi, WSI_TOKEN_HEAD_URI)) {
      request_type_ = getRequestTypeByName("HEAD");
    }
  }

  switch (reason) {
    case LWS_CALLBACK_HTTP: {

      // content-length WSI_TOKEN_HTTP_CONTENT_LENGTH
      if(lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH)) {
        char buf_cl[100];
        // if there is body content we store it and function accordingly
        lws_hdr_copy(wsi, buf_cl,100,WSI_TOKEN_HTTP_CONTENT_LENGTH);
        headers_.insert(pair<string,string>(Constant::Attributes::AR_HEAD_CONTENT_LENGTH,buf_cl));
      }

      // content-type WSI_TOKEN_HTTP_CONTENT_TYPE
      if(lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE)) {
        char buf_ct[1000];
        // if there is body content we store it and function accordingly
        lws_hdr_copy(wsi, buf_ct,1000,WSI_TOKEN_HTTP_CONTENT_TYPE);
        headers_.insert(pair<string,string>(Constant::Attributes::AR_HEAD_CONTENT_TYPE,buf_ct));
      }

      // connection WSI_TOKEN_CONNECTION
      if(lws_hdr_total_length(wsi, WSI_TOKEN_CONNECTION)) {
        char buf_con[100];
        // if there is body content we store it and function accordingly
        lws_hdr_copy(wsi, buf_con, 100, WSI_TOKEN_CONNECTION);
        headers_.insert(pair<string,string>(Constant::Attributes::AR_HEAD_CONNECTION,buf_con));
      }

      // host WSI_TOKEN_HOST
      if(lws_hdr_total_length(wsi, WSI_TOKEN_HOST)) {
        char buf_host[1000];
        // if there is body content we store it and function accordingly
        lws_hdr_copy(wsi, buf_host, 1000, WSI_TOKEN_HOST);
        headers_.insert(pair<string,string>(Constant::Attributes::AR_HEAD_HOST,buf_host));
      }

      // protocol WSI_TOKEN_PROTOCOL
      if(lws_hdr_total_length(wsi, WSI_TOKEN_PROTOCOL)) {
        char buf_protocol[100];
        // if there is body content we store it and function accordingly
        lws_hdr_copy(wsi, buf_protocol, 100, WSI_TOKEN_PROTOCOL);
        protocol_ = buf_protocol;
      }


      if(getRequestType() == getRequestTypeByName("GET")) {
        body_args_ = parseUrlArgs(wsi,buffer_data);
      }

      if(getRequestType() == getRequestTypeByName("POST") && headers_.find(Constant::Attributes::AR_HEAD_CONTENT_LENGTH) != headers_.end()) {
        return 0;
      }

      if(headers_.find(Constant::Attributes::AR_HEAD_CONTENT_LENGTH) == headers_.end() && app_) {
        AppResponse *response = app_->getResponse(this);

        if(response) {
          dest_buffer->large_str = response->getContent();
          optimizeResponseString(dest_buffer->large_str, buffer_data);

          return response->generateResponseHeaders(wsi, buffer_data, start, p, end);
        }
      }

      break;
    }

    case LWS_CALLBACK_HTTP_BODY: {
      if (!dest_buffer->spa) {
        const char * const buf[] = {};
        dest_buffer->spa = lws_spa_create(wsi,buf,0, 2048, NULL, NULL);
        if (!dest_buffer->spa) {
          dest_buffer->error_msg = "Failed to create request (probably server side problem)";
          // return 0;
        }
      }

      /* let it parse the POST data */
      if (dest_buffer->spa && lws_spa_process(dest_buffer->spa, (const char*)in, (int)len)) {
        if(dest_buffer->error_msg.length() > 0) {
          return 0;
        }
        return 1;
      } else if(!dest_buffer->spa) {
        // if there is not native parser we just store input and hadle it later
        input_body_ += (char *)in;
      }

      return 0;
    }

    case LWS_CALLBACK_HTTP_BODY_COMPLETION: {
      // cout << "Full input" << endl << input_body_.substr(0,stoi(headers_.at("content-length"))) << endl;
      // cout << "The length: " << input_body_.length() << endl;
      body_args_ = parseBodyAttributes(input_body_.substr(0,stoi(headers_.at(Constant::Attributes::AR_HEAD_CONTENT_LENGTH))));
      AppResponse *response = app_->getResponse(this);

      if(response) {
        dest_buffer->large_str = response->getContent();
        optimizeResponseString(dest_buffer->large_str, buffer_data);

        return response->generateResponseHeaders(wsi, buffer_data, start, p, end);
      }

      break;
    }

    case LWS_CALLBACK_HTTP_WRITEABLE: {
      // writing request when asked to
      return writeHttpRequest(wsi, buffer_data, in, len);
    }

    case LWS_CALLBACK_HTTP_DROP_PROTOCOL: {
      return handleDropProtocol(buffer_data);
    }

    default: {
      break;
    }
  }

  return lws_callback_http_dummy(wsi, reason, buffer_data, in, len);
}

int AppRequest::generateResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  /* prepare and write http headers */
  if(getHeaders().find(Constant::Attributes::AR_HEAD_CONTENT_TYPE) == getHeaders().end()) {
    if(lws_add_http_common_headers(wsi, HTTP_STATUS_OK, Constant::String::REQ_TYPE_TEXT_HTML, dest_buffer->len, &p, end))
      return 1;
  } else {
    if(lws_add_http_common_headers(wsi, HTTP_STATUS_OK, getHeaders().at(Constant::Attributes::AR_HEAD_CONTENT_TYPE).c_str(), dest_buffer->len, &p, end))
      return 1;
  }

  if (lws_finalize_write_http_header(wsi, start, &p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}

int AppRequest::generateFailResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;
  /* prepare and write http headers */
  if (lws_add_http_common_headers(wsi, HTTP_STATUS_NOT_FOUND, Constant::String::REQ_TYPE_TEXT_HTML, dest_buffer->len, &p, end)) {
    return 1;
  }

  if (lws_finalize_write_http_header(wsi, start, &p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}

map<string,string> AppRequest::parseUrlArgs(struct lws *wsi, void* buffer_data) {
  (void)buffer_data;
  map<string,string> args;
  /* dump the individual URI Arg parameters */
	int n = 0;
  char buf[1024];

  while (lws_hdr_copy_fragment(wsi, buf, sizeof(buf), WSI_TOKEN_HTTP_URI_ARGS, n) > 0) {
    std::smatch m_url;
    std::regex e_url(R"(^(\w+|\d+)=(\w+|\d+)$)");

    string key;
    string value;

    string temp = buf;

    while (std::regex_search (temp, m_url, e_url, std::regex_constants::match_any)) {
      key = m_url[1];
      value = m_url[2];
      break;
    }

    args.insert(pair<string,string>(key,value));
	}

  return args;
}
