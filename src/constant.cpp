#include "constant.h"

const char * Constant::CONFIG_PATH = "config.json";

const char * Constant::String::INITIALIZING = "initializing";
const char * Constant::String::CRASHED = "crashed";
const char * Constant::String::RUNNING = "running";
const char * Constant::String::PAUSED = "paused";
const char * Constant::String::PROTOCOL_HTTP = "http";
const char * Constant::String::LOG_TIMESTAMP = "[%a %b %d %H:%M:%S %Y]";
const char * Constant::String::REQ_TYPE_TEXT_HTML = "text/html";
const char * Constant::String::REQ_TYPE_APP_JSON = "application/json";

const char * Constant::Attributes::GENERAL = "general";
const char * Constant::Attributes::MANAGER_SERVER = "manager-server";
const char * Constant::Attributes::DEVICE = "device";
const char * Constant::Attributes::CORE_LIB_PATH = "core_lib_path";
const char * Constant::Attributes::DEVICE_ID = "_id";
const char * Constant::Attributes::DEVICE_NAME = "name";
const char * Constant::Attributes::DEVICE_MANUFACTURER = "manufacturer";
const char * Constant::Attributes::DEVICE_LOCATION = "location";
const char * Constant::Attributes::DEVICE_URL = "url";
const char * Constant::Attributes::DEVICE_HOST = "host";
const char * Constant::Attributes::DEVICE_PORT = "port";
const char * Constant::Attributes::APP_INTERFACES = "applicationInterfaces";
const char * Constant::Attributes::APP_NAME = "name";
const char * Constant::Attributes::APP_VERSION = "version";
const char * Constant::Attributes::APP_DESCRIPTION = "description";
const char * Constant::Attributes::APP_MAIN = "main";
const char * Constant::Attributes::APP_ID = "id";
const char * Constant::Attributes::APP_STATE = "state";
const char * Constant::Attributes::APP_AUTHOR = "author";
const char * Constant::Attributes::APP_LICENCE = "licence";
const char * Constant::Attributes::RR_HOST = "host";
const char * Constant::Attributes::RR_PORT = "port";
const char * Constant::Attributes::LIOT_DEV_CAP = "deviceCapabilities";
const char * Constant::Attributes::LIOT_APP_INTERFACES = "applicationInterfaces";
const char * Constant::Attributes::AR_HEAD_CONTENT_LENGTH = "content-length";
const char * Constant::Attributes::AR_HEAD_CONTENT_TYPE = "content-type";
const char * Constant::Attributes::AR_HEAD_CONNECTION = "connection";
const char * Constant::Attributes::AR_HEAD_HOST = "host";

const char * Constant::Paths::APPLICATIONS_ROOT = "applications";
const char * Constant::Paths::TEMP_FOLDER = "tmp";
const char * Constant::Paths::EVENT_LOOP_JS = "./js/custom_eventloop.js";
const char * Constant::Paths::APPLICATION_HEADER_JS = "./js/application_header.js";
const char * Constant::Paths::AGENT_JS = "./js/agent.js";
const char * Constant::Paths::API_JS = "./js/api.js";
const char * Constant::Paths::APP_JS = "./js/app.js";
const char * Constant::Paths::REQUEST_JS = "./js/request.js";
const char * Constant::Paths::RESPONSE_JS = "./js/response.js";
const char * Constant::Paths::LIQUID_IOT_JSON = "/liquidiot.json";
const char * Constant::Paths::PACKAGE_JSON = "/package.json";
const char * Constant::Paths::HTTP_FILE_ROOT = "./http-files";
const char * Constant::Paths::INDEX_FILE_NAME = "index.html";
const char * Constant::Paths::FILE_404 = "/404.html";
const char * Constant::Paths::APP_MOUNT_POINT = "/app";
const char * Constant::Paths::LOG_PATH = "/logs.txt";
