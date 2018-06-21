#include "http_client.h"

#if defined (__cplusplus)
extern "C" {
#endif

  #include "globals.h"
  #include <signal.h>

#if defined (__cplusplus)
}
#endif

const size_t HttpClient::BUFFER_SIZE = 256;
struct lws *HttpClient::client_wsi;

const struct lws_protocols HttpClient::protocols[] = {
  { "http", http_client_callback, sizeof(struct HttpClient::user_buffer_data), HttpClient::BUFFER_SIZE, 1, new struct HttpClient::user_buffer_data, 0},
  { NULL, NULL, 0, 0, 0, NULL, 0 } /* terminator */
};

void HttpClient::run(ClientRequestConfig *config) {
  struct lws_context_creation_info info;
  struct lws_client_connect_info connect_info;
  struct lws_context *context;

  const char *p;
  int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

  signal(SIGINT, sigint_handler);

  lws_set_log_level(logs, NULL);
  lwsl_user("LWS minimal http client\n");

  memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
  // info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = protocols;

  context = lws_create_context(&info);

  if (!context) {
    lwsl_err("lws init failed\n");
    return;
  }

  memset(&connect_info, 0, sizeof connect_info); /* otherwise uninitialized garbage */
  connect_info.context = context;

  connect_info.port = 3000;
  connect_info.address = "localhost";
  connect_info.path = "/devices";

  connect_info.host = connect_info.address;
  connect_info.origin = connect_info.address;
  connect_info.method = "GET";

  connect_info.alpn = "http/1.1";

  connect_info.protocol = protocols[0].name;

  connect_info.pwsi = &client_wsi;

  lws_client_connect_via_info(&connect_info);

  while (n >= 0 && client_wsi && !interrupted)
    n = lws_service(context, 1000);

  lws_context_destroy(context);

  return;
}

int HttpClient::http_client_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
  struct HttpClient::user_buffer_data *dest_buffer = (struct HttpClient::user_buffer_data *)user;

  char buf[LWS_PRE + 1024], *start = &buf[LWS_PRE], *p = start,
  *end = &buf[sizeof(buf) - 1];
  uint8_t **up, *uend;
  uint32_t r;
  int n;

  switch (reason) {

    /* because we are protocols[0] ... */
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: {
      lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char *)in : "(null)");
      client_wsi = NULL;
      break;
    }

    case LWS_CALLBACK_CLOSED_CLIENT_HTTP: {
      client_wsi = NULL;
      lws_cancel_service(lws_get_context(wsi));
      break;
    }

    /* ...callbacks related to receiving the result... */

    case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP: {
      dest_buffer->connected_status = lws_http_client_http_response(wsi);
      lwsl_user("Connected with server response: %d\n", dest_buffer->connected_status);
      break;
    }

    case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ: {
      lwsl_user("RECEIVE_CLIENT_HTTP_READ: read %d\n", (int)len);
      lwsl_hexdump_notice(in, len);
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
      lwsl_user("LWS_CALLBACK_COMPLETED_CLIENT_HTTP\n");

      if (client_wsi == wsi)
        client_wsi = NULL;

      lws_cancel_service(lws_get_context(wsi));
      break;
    }

    default:
      break;
  }

  return lws_callback_http_dummy(wsi, reason, user, in, len);
}
