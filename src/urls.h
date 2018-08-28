#ifndef URLS_H_INCLUDED
#define URLS_H_INCLUDED

#include <regex>
#include <string>

using std::regex;
using std::string;

#include "http_server.h"

struct mount_path {
  // basically how the url is filtered
  regex url_rule;
  // function to call
  int (*call_back)(struct lws*,enum lws_callback_reasons, void*, void*, size_t);
  // name to recognize
  const char* name;

};

const unsigned int URL_MOUNTS_SIZE = 4;

const struct mount_path URL_MOUNTS[] = {
  { regex(R"(^/?$)"), HttpServer::server_rest_api, "root" },
  { regex(R"(^/(\d)+/?$)"), HttpServer::server_rest_api, "app_root" },
  { regex(R"(^/(\d)+/log/?$)"), HttpServer::server_rest_api, "app_logs" },
  { regex(R"(^/(\d)+/api/(\w)+$)"), HttpServer::app_rest_api, "app_api" },
};

#endif
