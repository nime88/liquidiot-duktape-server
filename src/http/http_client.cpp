#include "http_client.h"

#if defined (__cplusplus)
extern "C" {
#endif

  #include "globals.h"
  #include <signal.h>

#if defined (__cplusplus)
}
#endif

#include "device.h"

#include <algorithm>

const size_t HttpClient::BUFFER_SIZE = 256;
int HttpClient::status_;
struct lws *HttpClient::client_wsi;
ClientRequestConfig *HttpClient::crconfig_;

const struct lws_protocols HttpClient::protocols[] = {
  { "http", http_client_callback, sizeof(struct HttpClient::user_buffer_data), HttpClient::BUFFER_SIZE, 1, new struct HttpClient::user_buffer_data, 0},
  { NULL, NULL, 0, 0, 0, NULL, 0 } /* terminator */
};

void HttpClient::run(ClientRequestConfig *config) {
  struct lws_context_creation_info info;
  struct lws_client_connect_info connect_info;
  struct lws_context *context;
  crconfig_ = config;

  int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
  // Add: LLL_INFO for debug

  signal(SIGINT, sigint_handler);

  lws_set_log_level(logs, NULL);
  lwsl_user("Starting http client for the RR server.\n");

  memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = protocols;
  info.gid = -1;
  info.uid = -1;

  context = lws_create_context(&info);

  if (!context) {
    lwsl_err("lws init failed\n");
    return;
  }

  memset(&connect_info, 0, sizeof connect_info); /* otherwise uninitialized garbage */
  connect_info.context = context;
  // connect_info.ssl_connection = LCCSCF_USE_SSL;

  connect_info.port = atoi(crconfig_->getRRPort());
  connect_info.address = crconfig_->getRRHost();
  // connect_info.ssl_connection |= LCCSCF_ALLOW_SELFSIGNED;

  connect_info.alpn = "http/1.1";

  connect_info.path = crconfig_->getRequestPath();
  connect_info.host = connect_info.address;
  connect_info.origin = connect_info.address;
  connect_info.method = crconfig_->getRequestType().c_str();

  connect_info.protocol = protocols[0].name;

  connect_info.pwsi = &client_wsi;

  if (!lws_client_connect_via_info(&connect_info)) {
    lws_context_destroy(context);
    return;
  }

  while (n >= 0 && client_wsi && !interrupted)
    n = lws_service(context, 1000);

  lws_context_destroy(context);

  return;
}

int HttpClient::http_client_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
  struct HttpClient::user_buffer_data *dest_buffer = (struct HttpClient::user_buffer_data *)user;

  if(dest_buffer && !dest_buffer->config) {
    dest_buffer->config = crconfig_;
  }

  switch (reason) {

    /* because we are protocols[0] ... */
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: {
      lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char *)in : "(null)");
      client_wsi = NULL;
      break;
    }

    case LWS_CALLBACK_CLOSED_CLIENT_HTTP: {
      client_wsi = NULL;
      status_ = lws_http_client_http_response(wsi);
      lws_cancel_service(lws_get_context(wsi));
      break;
    }

    /* ...callbacks related to receiving the result... */

    case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP: {
      status_ = lws_http_client_http_response(wsi);
      break;
    }

    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ: {
      dest_buffer->raw_data = (char *)in;
      dest_buffer->raw_data.erase(std::remove(dest_buffer->raw_data.begin(),
        dest_buffer->raw_data.end(), '\"'), dest_buffer->raw_data.end());
      crconfig_->setResponse(dest_buffer->raw_data);
      crconfig_->setResponseStatus(status_);
      return 0; /* don't passthru */
    }

    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP: {
      char buffer[1024 + LWS_PRE];
      char *px = buffer + LWS_PRE;
      int lenx = sizeof(buffer) - LWS_PRE;

      if (lws_http_client_read(wsi, &px, &lenx) < 0)
        return -1;

      return 0; /* don't passthru */
    }

    case LWS_CALLBACK_COMPLETED_CLIENT_HTTP: {
      client_wsi = NULL;

      lws_cancel_service(lws_get_context(wsi));
      break;
    }

    /* ...callbacks related to generating the POST... */
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {

      if(dest_buffer && dest_buffer->config &&
        (dest_buffer->config->getRequestType() == "POST" || dest_buffer->config->getRequestType() == "PUT")) {
        	uint32_t r;
          unsigned char **up = (unsigned char **)in, *uend = (*up) + len;

          dest_buffer->len = dest_buffer->config->getRawPayload().length();

          char buffer[1000];
          int blen = lws_snprintf(buffer, sizeof(buffer) - 1, "application/json");

          if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE, (unsigned char *)buffer, blen, up, uend))
            return 1;

          char cl_buffer[20];
          int pl_len = lws_snprintf(cl_buffer, sizeof(cl_buffer) - 1, "%d", (unsigned int)dest_buffer->config->getRawPayload().length());

          if(lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_LENGTH, (unsigned char *)cl_buffer, pl_len, up, uend))
              return 1;

          lws_client_http_body_pending(wsi, 1);
          lws_callback_on_writable(wsi);
        }

        break;
      }

      case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE: {
        if(dest_buffer && dest_buffer->config &&
          (dest_buffer->config->getRequestType() == "POST" || dest_buffer->config->getRequestType() == "PUT")) {

          string full_payload = dest_buffer->config->getRawPayload();

          if(dest_buffer->buffer_idx > full_payload.length()) return 0;

          string write_buffer = full_payload.substr(dest_buffer->buffer_idx, HttpClient::BUFFER_SIZE);
          dest_buffer->buffer_idx += (int)HttpClient::BUFFER_SIZE;

          if(dest_buffer->buffer_idx < full_payload.length() && write_buffer.length() == HttpClient::BUFFER_SIZE) {
            char *out = (char *)malloc(sizeof(char)*(LWS_SEND_BUFFER_PRE_PADDING + write_buffer.length() + LWS_SEND_BUFFER_POST_PADDING));
            memcpy (out + LWS_SEND_BUFFER_PRE_PADDING, write_buffer.c_str(), write_buffer.length() );
            if (lws_write( wsi, (uint8_t *)out + LWS_SEND_BUFFER_PRE_PADDING, write_buffer.length(), LWS_WRITE_HTTP ) < 0) {
              free(out);
              lwsl_err("Error sending\n");
              return -1;
            }

            free(out);
            lws_callback_on_writable(wsi);
          } else {
            lws_client_http_body_pending(wsi, 0);

            if (lws_write(wsi, (uint8_t *)write_buffer.c_str(), write_buffer.length(), LWS_WRITE_HTTP_FINAL) < 0) {
              lwsl_err("Error sending\n");
              return -1;
            }

          }

          return 0;
        }

        break;
      }

      default:
        break;
		}

  return lws_callback_http_dummy(wsi, reason, user, in, len);
}
