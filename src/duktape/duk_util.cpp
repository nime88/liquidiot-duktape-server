#include "duk_util.h"

#include "application.h"
#include "app_log.h"
#include "constant.h"

void custom_fatal_handler(void *udata, const char *msg) {
  JSApplication *app = (JSApplication*) udata;

  const char * fmsg;
  if(msg) fmsg = msg;
  else fmsg = "";

  AppLog(app->getAppPath().c_str()) << AppLog::getTimeStamp() << " [" << Constant::String::LOG_FATAL_ERROR << "] " << fmsg << "\n";

  app->clean();
  abort();
}
