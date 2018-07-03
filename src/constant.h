#ifndef CONSTANTS_H_INCLUDED
#define CONSTANTS_H_INCLUDED



class Constant {
public:
  static const char * CONFIG_PATH;

  class String {
    public:
      // application state strings
      static const char * INITIALIZING;
      static const char * CRASHED;
      static const char * RUNNING;
      static const char * PAUSED;

      // http protocols
      static const char * PROTOCOL_HTTP;

      // logs
      static const char * LOG_TIMESTAMP;
      static const char * LOG_ASSERT_ERROR;
      static const char * LOG_NORMAL;
      static const char * LOG_TRACE_ERROR;
      static const char * LOG_INFO;
      static const char * LOG_WARNING;
      static const char * LOG_ERROR;
      static const char * LOG_FATAL_ERROR;

      // misc http
      static const char * REQ_TYPE_TEXT_HTML;
      static const char * REQ_TYPE_APP_JSON;

    private:
      String();
  };

  class Attributes {
    public:
      // main config attributes
      static const char * GENERAL;
      static const char * MANAGER_SERVER;
      static const char * DEVICE;

      // general config attributes
      static const char * CORE_LIB_PATH;

      // device config attributes
      static const char * DEVICE_ID;
      static const char * DEVICE_NAME;
      static const char * DEVICE_MANUFACTURER;
      static const char * DEVICE_LOCATION;
      static const char * DEVICE_URL;
      static const char * DEVICE_HOST;
      static const char * DEVICE_PORT;

      // application attributes
      static const char * APP_INTERFACES;
      static const char * APP_NAME;
      static const char * APP_VERSION;
      static const char * APP_DESCRIPTION;
      static const char * APP_MAIN;
      static const char * APP_ID;
      static const char * APP_STATE;
      static const char * APP_AUTHOR;
      static const char * APP_LICENCE;

      // RR server attributes
      static const char * RR_HOST;
      static const char * RR_PORT;

      // liquidiot.json attributes
      static const char * LIOT_DEV_CAP;
      static const char * LIOT_APP_INTERFACES;

      // app request headers
      static const char * AR_HEAD_CONTENT_LENGTH;
      static const char * AR_HEAD_CONTENT_TYPE;
      static const char * AR_HEAD_CONNECTION;
      static const char * AR_HEAD_HOST;

    private:
      Attributes();
  };

  class Paths {
    public:
      // general
      static const char * APPLICATIONS_ROOT;
      static const char * TEMP_FOLDER;

      // executable js files
      static const char * EVENT_LOOP_JS;
      static const char * APPLICATION_HEADER_JS;
      static const char * AGENT_JS;
      static const char * API_JS;
      static const char * APP_JS;
      static const char * REQUEST_JS;
      static const char * RESPONSE_JS;

      // local app path files
      static const char * LIQUID_IOT_JSON;
      static const char * PACKAGE_JSON;

      // urls
      static const char * HTTP_FILE_ROOT;
      static const char * INDEX_FILE_NAME;
      static const char * FILE_404;
      static const char * APP_MOUNT_POINT;
      static const char * DEV_ROOT_URL;
      static const char * REGISTER_APP_URL;
      static const char * UPDATE_APP_INFO_URL;
      static const char * DELETE_APP_URL;

      // logs
      static const char * LOG_PATH;
    private:
      Paths();
  };

private:
  Constant();
  template <typename T>
  static void ignore(T &&)
  { }

};

#endif
