#include "application.h"

#if defined (__cplusplus)
extern "C" {
#endif

  #include <dirent.h>

#if defined (__cplusplus)
}
#endif

#include <algorithm>
#include <regex>
#include <iostream>

using std::regex;
using std::cout;
using std::endl;

#include "node_module_search.h"
#include "util.h"
#include "file_ops.h"
#include "logs/app_log.h"
#include "logs/read_app_log.h"
#include "device.h"

#define NDEBUG

#ifdef NDEBUG
    #define DBOUT( x ) cout << x  << "\n"
#else
    #define DBOUT( x )
#endif

const array<string,4> JSApplication::APP_STATES_CHAR = { {
  Constant::String::INITIALIZING,
  Constant::String::CRASHED,
  Constant::String::RUNNING,
  Constant::String::PAUSED} };
int JSApplication::next_id_ = 0;
map<string,string> JSApplication::options_;
map<duk_context*, JSApplication*> JSApplication::applications_;
map<duk_context*, thread*> JSApplication::app_threads_;
mutex *JSApplication::mtx = new mutex();

JSApplication::JSApplication(const char* path) {
  // adding app_path to global app paths
  app_path_ = string(path);
  if(app_path_.length() > 0 && app_path_.at(app_path_.length()-1) != '/')
    app_path_ += "/";

  // setting id for the application
  id_ = next_id_;
  next_id_++;

  init();

  for(unsigned int i = 0; i < application_interfaces_.size(); ++i) {
      Device::getInstance().registerAppApi(application_interfaces_.at(i), swagger_fragment_);
  }

  string app_payload = getAppAsJSON();
  bool app_is = Device::getInstance().appExists(std::to_string(getId()));

  if(!app_is)
    Device::getInstance().registerApp(app_payload);
}

void JSApplication::init() {
  DBOUT("init()");

  while(!mtx->try_lock()) { poll(NULL,0,1); }

  // creating new duk heap for the js application
  duk_context_ = duk_create_heap_default();
  if (!duk_context_) {
    throw "Duk context could not be created.";
  }

  if(options_.size() == 0) {
    map<string, map<string,string> > full_config = get_config(duk_context_);

    if(full_config.find(Constant::Attributes::GENERAL) != full_config.end())
      options_ = full_config.at(Constant::Attributes::GENERAL);
  }

  // setting app state to initializing
  updateAppState(APP_STATES::INITIALIZING);

  DBOUT ("Inserting application");
  // adding this application to static apps
  applications_.insert(pair<duk_context*, JSApplication*>(duk_context_,this));

  DBOUT ("Duktape initializations");
  DBOUT ("Duktape initializations: print alert");
  duk_push_global_object(duk_context_);
  duk_push_string(duk_context_, "print");
  duk_push_c_function(duk_context_, cb_print, DUK_VARARGS);
  duk_def_prop(duk_context_, -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_WRITABLE | DUK_DEFPROP_SET_CONFIGURABLE);
  duk_push_string(duk_context_, "alert");
  duk_push_c_function(duk_context_, cb_alert, DUK_VARARGS);
  duk_def_prop(duk_context_, -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_WRITABLE | DUK_DEFPROP_SET_CONFIGURABLE);
  duk_pop(duk_context_);

  DBOUT ("Duktape initializations: console");
  // initializing console (enables console in js)
  duk_console_init(duk_context_, DUK_CONSOLE_PROXY_WRAPPER /*flags*/);

  DBOUT ("Duktape initializations: pushin object");
  // initializing node.js require
  duk_push_object(duk_context_);
  DBOUT ("Pushing resolve module");
  duk_push_c_function(duk_context_, cb_resolve_module, DUK_VARARGS);
  duk_put_prop_string(duk_context_, -2, "resolve");
  DBOUT ("Pushing load module");
  duk_push_c_function(duk_context_, cb_load_module, DUK_VARARGS);
  duk_put_prop_string(duk_context_, -2, "load");
  duk_module_node_init(duk_context_);

  DBOUT ("Adding response resolution");

  /* Set global 'ResolveResponse'. */
  duk_push_global_object(duk_context_);
  duk_push_c_function(duk_context_, cb_resolve_app_response, 1);
  duk_put_prop_string(duk_context_, -2, "ResolveResponse");
  duk_pop(duk_context_);

  duk_push_global_object(duk_context_);
  duk_push_c_function(duk_context_, cb_load_swagger_fragment, 1);
  duk_put_prop_string(duk_context_, -2, "LoadSwaggerFragment");
  duk_pop(duk_context_);

  DBOUT ("Creating new eventloop");
  // registering/creating the eventloop
  eventloop_ = new EventLoop(duk_context_);

  // loading the event loop javascript functions (setTimeout ect.)
  int evlLen;
  char* evlSource = load_js_file(Constant::Paths::EVENT_LOOP_JS,evlLen);
  duk_push_string(duk_context_, evlSource);
  if (duk_peval(duk_context_) != 0) {
    printf("Failed to evaluate custom_eventloop.js: %s\n", duk_safe_to_string(duk_context_, -1));
  }
  duk_pop(duk_context_);

  int tmpLen;
  char* headerSource = load_js_file(Constant::Paths::APPLICATION_HEADER_JS,tmpLen);

  // evaluating ready made headers
  duk_push_string(duk_context_, headerSource);
  if(duk_peval(duk_context_) != 0) {
    printf("Script error: %s\n", duk_safe_to_string(duk_context_, -1));
  }
  duk_pop(duk_context_);

  // evaluating necessary js files that main.js will need
  char* agentSource = load_js_file(Constant::Paths::AGENT_JS,tmpLen);
  char* apiSource = load_js_file(Constant::Paths::API_JS,tmpLen);
  char* appSource = load_js_file(Constant::Paths::APP_JS,tmpLen);
  char* requestSource = load_js_file(Constant::Paths::REQUEST_JS,tmpLen);
  char* responseSource = load_js_file(Constant::Paths::RESPONSE_JS,tmpLen);

  duk_push_string(duk_context_, agentSource);
  if (duk_peval(duk_context_) != 0) {
    printf("Failed to evaluate agent.js: %s\n", duk_safe_to_string(duk_context_, -1));
  }
  duk_pop(duk_context_);

  duk_push_string(duk_context_, apiSource);
  if (duk_peval(duk_context_) != 0) {
    printf("Failed to evaluate api.js: %s\n", duk_safe_to_string(duk_context_, -1));
  }
  duk_pop(duk_context_);

  duk_push_string(duk_context_, appSource);
  if (duk_peval(duk_context_) != 0) {
    printf("Failed to evaluate app.js: %s\n", duk_safe_to_string(duk_context_, -1));
  }
  duk_pop(duk_context_);

  duk_push_string(duk_context_, requestSource);
  if (duk_peval(duk_context_) != 0) {
    printf("Failed to evaluate request.js: %s\n", duk_safe_to_string(duk_context_, -1));
  }
  duk_pop(duk_context_);

  duk_push_string(duk_context_, responseSource);
  if (duk_peval(duk_context_) != 0) {
    printf("Failed to evaluate response.js: %s\n", duk_safe_to_string(duk_context_, -1));
  }
  duk_pop(duk_context_);

  /** reding the source code (main.js) for the app **/

  int source_len;

  parsePackageJS();

  string main_file = app_path_ + "/" + main_;

  DBOUT ("JSApplication(): Loading liquidiot file");
  // reading the package.json file to memory
  string liquidiot_file = app_path_ + string(Constant::Paths::LIQUID_IOT_JSON);
  string liquidiot_js = load_js_file(liquidiot_file.c_str(),source_len);

  DBOUT ("JSApplication(): Reading json to attr");
  map<string,vector<string> > liquidiot_json_attr = read_liquidiot_json(duk_context_, liquidiot_js.c_str());
  if(liquidiot_json_attr.find(Constant::Attributes::APP_INTERFACES) != liquidiot_json_attr.end()) {
    application_interfaces_ = liquidiot_json_attr.at(Constant::Attributes::APP_INTERFACES);
  }

  DBOUT ("JSApplication(): Creating full source code");
  string temp_source =  string(load_js_file(main_file.c_str(),source_len)) +
    "\napp = {};\n"
    "IoTApp(app);\n"
    "module.exports(app);\n"
    "if(typeof app != \"undefined\")\n\t"
    "LoadSwaggerFragment(JSON.stringify(app.external.swagger));"
    "app.internal.start();\n";

  // executing initialize code
  source_code_ = new char[temp_source.length()+1];
  strcpy(source_code_,temp_source.c_str());

  DBOUT ("JSApplication(): Making it main node module");
  // pushing this as main module
  duk_push_string(duk_context_, source_code_);

  if(duk_module_node_peval_main(duk_context_, main_.c_str()) != 0) {
    printf("Failed to evaluate main module: %s\n", duk_safe_to_string(duk_context_, -1));
  }

  DBOUT ("Inserting application thread");
  // initializing the thread, this must be the last initialization
  ev_thread_ = new thread(&JSApplication::run,this);
  app_threads_.insert(pair<duk_context*, thread*>(duk_context_,ev_thread_));

  mtx->unlock();
}

void JSApplication::clean() {
  // updating app state before locking
  updateAppState(APP_STATES::PAUSED, true);

  while(!mtx->try_lock()) { poll(NULL,0,1); }

  DBOUT ( "clean()" );
  // if the application is running we need to stop the thread
  eventloop_->setRequestExit(true);
  // waiting for thread to stop the execution (come back from poll)
  DBOUT ("clean(): waiting for thread over" );
  try {
    // ev_thread_->detach(); // detaching a not-a-thread
    while (ev_thread_->joinable()) {
      mtx->unlock();
      // waiting eventloop poll to finish
      poll(NULL,0,eventloop_->getCurrentTimeout());
      if(ev_thread_->joinable())
        ev_thread_->join();
        while(!mtx->try_lock()) { poll(NULL,0,1); }
    }
    // ev_thread_->detach();
  } catch (const std::system_error& e) {
    std::cout << "Caught a system_error\n";
    if(e.code() == std::errc::invalid_argument)
      std::cout << "The error condition is std::errc::invalid_argument\n";
      std::cout << "the error description is " << e.what() << '\n';
  }

  cout << "clean(): killing thread was successful" << endl;

  cout << "clean(): thread waiting over and calling terminate" << endl;

  duk_push_string(duk_context_, "app.internal.terminate(function(){});");
  if(duk_peval(duk_context_) != 0) {
    printf("Script error: %s\n", duk_safe_to_string(duk_context_, -1));
  }

  delete ev_thread_;
  ev_thread_ = 0;
  app_threads_.erase(duk_context_);

  delete eventloop_;
  eventloop_ = 0;

  cout << "clean(): destoying duk heap" << endl;
  duk_destroy_heap(duk_context_);

  cout << "clean(): cleaning the app out" << duk_context_ << endl;

  applications_.erase(duk_context_);
  duk_context_ = 0;
  mtx->unlock();
}

string JSApplication::getLogs() {
  string logs;

  logs = ReadAppLog(getAppPath().c_str()).getLogsString();

  return logs;
}

string JSApplication::getLogsAsJSON() {
  string json_logs;
  string logs = ReadAppLog(getAppPath().c_str()).getLogsString();

  std::smatch m;
  std::regex e( R"(\[(\w{3}\s+\w{3}\s+\d{2}\s+\d{2}:\d{2}:\d{2}\s+\d{4})\]\s\[(\w+)\]\s([A-Za-z\d\s._:]*)(\n|\n$)+?)" );

  json_logs += "[\n";
  while (std::regex_search (logs,m,e, std::regex_constants::match_any)) {
    string row = "{";
    string timestamp = m[1];
    string type = m[2];
    string message = m[3];

    row += "\"timestamp\":\"" + timestamp + "\",";
    row += "\"type\":\"" + type + "\",";
    row += "\"msg\":\"" + message + "\"";

    row += "},\n";
    logs = m.suffix().str();
    json_logs += row;
  }
  //cleaning up the end
  json_logs.erase(json_logs.length()-2,2);
  json_logs += "\n]";

  return json_logs;
}

AppResponse *JSApplication::getResponse(AppRequest *request) {
  map<string,string> headers = request->getHeaders();
  map<string,string> body_args = request->getBodyArguments();

  // formatting the headers
  string js_headers = "request.headers = { ";
  for(map<string,string>::iterator it = headers.begin(); it != headers.end(); it++) {
    js_headers += "\"" + it->first + "\" : \"" + it->second + "\",\n";
  }
  js_headers += "};\n";

  // formatting body arguments
  string js_body_args = "request.params = { ";
  for(map<string,string>::iterator it = body_args.begin(); it != body_args.end(); it++) {
    js_body_args += "\"" + it->first + "\" : \"" + it->second + "\",\n";
  }
  js_body_args += "};\n";

  string sourceCode = "var request = {};\n"
    "Request(request);\n"
    "request.request_type = \"" + string(request->requestTypeToString(request->getRequestType())) + "\";\n"
    + js_headers + js_body_args +
    "request.protocol = \"" + request->getProtocol() + "\";\n" +
    "var response = {};\n"
    "Response(response);\n"
    "app.external." + request->getAI() + "(request,response);\n";

  // initializing just in case
  if(app_response_) {
    delete app_response_;
    app_response_ = 0;
  }

  duk_push_string(duk_context_, sourceCode.c_str());
  if (duk_peval(duk_context_) != 0) {
    printf("Failed to evaluate response: %s\n", duk_safe_to_string(duk_context_, -1));
  }
  duk_pop(duk_context_);

  return app_response_;
}

bool JSApplication::setAppState(APP_STATES state) {
  // trivial case
  if(state == app_state_) return true;

  if(state == APP_STATES::PAUSED && app_state_ == APP_STATES::RUNNING) {
    return pause();
  }

  else if(state == APP_STATES::RUNNING && app_state_ == APP_STATES::PAUSED) {
    return start();
  }

  return false;
}

bool JSApplication::pause() {
  if(app_state_ == APP_STATES::PAUSED) return true;

  while (!mtx->try_lock()){ poll(NULL,0,1); }
  DBOUT ( "pause()" );
  mtx->unlock();
  updateAppState(APP_STATES::PAUSED, true);

  return true;
}

bool JSApplication::start() {
  if(app_state_ == APP_STATES::RUNNING) return true;

  while (!mtx->try_lock()){ poll(NULL,0,1); }
  mtx->unlock();

  updateAppState(APP_STATES::RUNNING, true);

  return true;
}

void JSApplication::run() {
  updateAppState(APP_STATES::RUNNING, true);
  int rc = duk_safe_call(duk_context_, EventLoop::eventloop_run, NULL, 0 /*nargs*/, 1 /*nrets*/);
  if (rc != 0) {
    updateAppState(APP_STATES::CRASHED, true);
    fprintf(stderr, "eventloop_run() failed: %s\n", duk_to_string(duk_context_, -1));
    fflush(stderr);
  }
  duk_pop(duk_context_);

}

void JSApplication::reload() {
  if(app_state_ == APP_STATES::RUNNING) {
    cout << "Cleaning app" << endl;
    // clearing application of all previous states and executions
    clean();
    cout << "Initializing app" << endl;
    // initialize app
    // while(!getMutex()->try_lock()) {}
    init();
  }
}

bool JSApplication::deleteApp() {
  // have to delete app before we clean (otherwise can really find it)
  applications_.erase(getContext());
  clean();
  if(delete_files(getAppPath().c_str()))
    return 1;

  return 0;
}

void JSApplication::updateAppState(APP_STATES state, bool update_client) {
  app_state_ = state;

  if(update_client) {
    // Device::getInstance().updateApp(std::to_string(getId()),getAppAsJSON());
  }
}

string JSApplication::getDescriptionAsJSON() {
  while(!getMutex()->try_lock()){ poll(NULL,0,1); }
  string full_status;
  duk_context *ctx = getContext();

  duk_push_object(ctx);

  // name
  if(name_.length() > 0) {
    duk_push_string(ctx,name_.c_str());
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_NAME);
  }

  // version
  if(version_.length() > 0) {
    duk_push_string(ctx,version_.c_str());
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_VERSION);
  }

  // description
  if(description_.length() > 0) {
    duk_push_string(ctx,description_.c_str());
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_DESCRIPTION);
  }

  // main
  if(main_.length() > 0) {
    duk_push_string(ctx,main_.c_str());
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_MAIN);
  }

  // id
  if(id_ >= 0) {
    duk_push_int(ctx, id_);
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_ID);
  }

  // status
  if(app_state_ >= 0 && app_state_ < 4) {
    duk_push_string(ctx,APP_STATES_CHAR.at(app_state_).c_str());
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_STATE);
  }

  full_status = duk_json_encode(ctx, -1);
  duk_pop(ctx);

  getMutex()->unlock();

  return full_status;
}

string JSApplication::getAppAsJSON() {
  if(!getContext()) return "";

  while(!getMutex()->try_lock()){ poll(NULL,0,1); }

  string full_status;
  duk_context *ctx = getContext();

  duk_push_object(ctx);
  /* [... obj ] */

  // name
  duk_push_string(ctx,name_.c_str());
  duk_put_prop_string(ctx, -2, Constant::Attributes::APP_NAME);

  // version
  duk_push_string(ctx,version_.c_str());
  duk_put_prop_string(ctx, -2, Constant::Attributes::APP_VERSION);

  // description
  duk_push_string(ctx,description_.c_str());
  duk_put_prop_string(ctx, -2, Constant::Attributes::APP_DESCRIPTION);

  // author
  duk_push_string(ctx,description_.c_str());
  duk_put_prop_string(ctx, -2, Constant::Attributes::APP_AUTHOR);

  // licence
  duk_push_string(ctx,description_.c_str());
  duk_put_prop_string(ctx, -2, Constant::Attributes::APP_LICENCE);

  // classes ( apparently references to this do not exist anymore )
  // int arr_idx = duk_push_array(ctx);
  //
  // for(unsigned int i = 0; i < application_interfaces_.size(); ++i) {
  //   string ai_str = application_interfaces_.at(i);
  //
  //   duk_push_string(ctx, ai_str.c_str());
  //   duk_put_prop_index(ctx, arr_idx, i);
  // }
  //
  // duk_put_prop_string(ctx, -2, "classes");

  // id
  if(id_ >= 0) {
    duk_push_int(ctx, id_);
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_ID);
  }

  // status
  if(app_state_ >= 0 && app_state_ < 4) {
    duk_push_string(ctx,APP_STATES_CHAR.at(app_state_).c_str());
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_STATE);
  }

  // applicationInterfaces ** Note ** This is not defined by api description O_o
  // however this seems to replace classes
  int arr_idx = duk_push_array(ctx);

  for(unsigned int i = 0; i < application_interfaces_.size(); ++i) {
    string ai_str = application_interfaces_.at(i);

    duk_push_string(ctx, ai_str.c_str());
    duk_put_prop_index(ctx, arr_idx, i);
  }
  duk_put_prop_string(ctx, -2, Constant::Attributes::APP_INTERFACES);

  full_status = duk_json_encode(ctx, -1);
  duk_pop(ctx);

  getMutex()->unlock();

  return full_status;
}

duk_ret_t JSApplication::cb_resolve_module(duk_context *ctx) {
  /*
   *  Entry stack: [ requested_id parent_id ]
   */

  const char *requested_id;
  const char *parent_id;
  (void)parent_id;

  // TODO if require string fails we have no way of knowing
  requested_id = duk_require_string(ctx, 0);
  parent_id = duk_require_string(ctx, 1);

  string requested_id_str = string(requested_id);

  // getting the known application/module path
  JSApplication *app = JSApplication::applications_.at(ctx);
  string path = app->getAppPath() + "/";

  // if buffer is requested just use native duktape implemented buffer
  if(requested_id_str == "buffer") {
    duk_push_string(ctx, requested_id);
    return 1;  /*nrets*/
  }

  if(requested_id_str.size() > 0) {
    // First checking core modules
    if (requested_id_str.at(0) != '.' && requested_id_str.at(0) != '/' &&
        options_.find(Constant::Attributes::CORE_LIB_PATH) != options_.end()) {
      string core_lib_path = options_.at(Constant::Attributes::CORE_LIB_PATH);
      if(core_lib_path.size() > 0) {
        if (node::is_core_module(core_lib_path + "/" + string(requested_id))) {
          cout << "Core Module found" << endl;
          requested_id_str = node::get_core_module(core_lib_path + "/" + string(requested_id));
          duk_push_string(ctx, requested_id_str.c_str());
          return 1;
        } else if(is_file((core_lib_path + "/" + requested_id).c_str()) == FILE_TYPE::PATH_TO_DIR) {
          requested_id_str = (core_lib_path + "/" + requested_id + "/");
          duk_push_string(ctx, requested_id_str.c_str());
          return 1;
        }
      }
    }

    // Then checking if path is system root
    if(requested_id_str.at(0) == '/') {
      path = "";
    }

    if(requested_id_str.substr(0,2) == "./" || requested_id_str.at(0) ==  '/' || requested_id_str.substr(0,3) == "../") {
      if(is_file((path + requested_id).c_str()) == FILE_TYPE::PATH_TO_FILE) {
        requested_id_str = path + requested_id;
      }
      else if(is_file((path + requested_id).c_str()) == FILE_TYPE::PATH_TO_DIR) {
        requested_id_str = path + requested_id;
      }
    }
  }

  duk_push_string(ctx, requested_id_str.c_str());
  return 1;  /*nrets*/
}

duk_ret_t JSApplication::cb_load_module(duk_context *ctx) {
    /*
     *  Entry stack: [ resolved_id exports module ]
     */

    const char *resolved_id = duk_require_string(ctx, 0);
    duk_get_prop_string(ctx, 2, "filename");
    const char *filename = duk_require_string(ctx, -1);

    if (string(resolved_id) == "buffer") {
      duk_push_string(ctx,"module.exports.Buffer = Buffer;");
      return 1;
    }

    printf("load_cb: id:'%s', filename:'%s'\n", resolved_id, filename);

    char *module_source = 0;

    // if resolved id is an actual file we try to load it as javascript
    if(is_file(resolved_id) == FILE_TYPE::PATH_TO_FILE) {
      module_source = node::load_as_file(resolved_id);
      if(!module_source) {
        (void) duk_type_error(ctx, "cannot find module: %s", resolved_id);
        return 1;
      }
    }
    // if resolved to directory we try to load it as such
    else if(is_file(resolved_id) == FILE_TYPE::PATH_TO_DIR) {
      // first we have to read the package.json
      string path = string(resolved_id);
      path = path + "package.json";

      if(is_file(path.c_str()) == FILE_TYPE::PATH_TO_FILE) {
        int sourceLen;
        char* package_js = load_js_file(path.c_str(),sourceLen);

        map<string,string> json_attr = read_package_json(ctx, package_js);

        // loading main file to source
        path = string(resolved_id) + json_attr.at(Constant::Attributes::APP_MAIN);
        module_source = node::load_as_file(path.c_str());
        if(!module_source) {
          (void) duk_type_error(ctx, "cannot find module: %s", resolved_id);
          return 1;
        }
      }
    }

    if(!module_source) {
      (void) duk_type_error(ctx, "cannot find module: %s", resolved_id);
      return 1;
    }

    string source = module_source;

    if(source.size() > 0) {
      // pushing the source code to heap as the require system wants that
      duk_push_string(ctx, module_source);
      // if (duk_peval(ctx) != 0) {
      //     printf("Script error 134: %s\n", duk_safe_to_string(ctx, -1));
      // }
    } else {
      (void) duk_type_error(ctx, "cannot find module: %s", resolved_id);
    }

    return 1;  /*nrets*/
}

duk_ret_t JSApplication::cb_print (duk_context *ctx) {
  JSApplication *app = JSApplication::getApplications().at(ctx);
  duk_idx_t nargs = duk_get_top(ctx);

  duk_push_string(ctx, " ");
  duk_insert(ctx, 0);
  duk_concat(ctx, nargs);

  // writing to log
  AppLog(app->getAppPath().c_str()) << AppLog::getTimeStamp() << " [Print] " << duk_require_string(ctx, -1) << endl;

  return 0;
}

duk_ret_t JSApplication::cb_alert (duk_context *ctx) {
  JSApplication *app = JSApplication::getApplications().at(ctx);
  duk_idx_t nargs = duk_get_top(ctx);

  duk_push_string(ctx, " ");
  duk_insert(ctx, 0);
  duk_concat(ctx, nargs);

  // writing to log
  AppLog(app->getAppPath().c_str()) << AppLog::getTimeStamp() << " [Alert] " << duk_require_string(ctx, -1) << endl;

  return 0;
}

duk_ret_t JSApplication::cb_resolve_app_response(duk_context *ctx) {
  /* Entry [ ... response ] */

  JSApplication *app = JSApplication::getApplications().at(ctx);
  AppResponse *app_response = new AppResponse();
  map<string,string> headers;
  string content_body = "";
  int status_code = 200;
  string status_message = "";

  // locking thread for duktape operations
  while (!app->getMutex()->try_lock()){ poll(NULL,0,1); }

  // getting headers first
  duk_push_string(ctx, "headers");
  if(duk_get_prop(ctx, 0)) {
    /* [ ... response headers ] */
    duk_enum(ctx, -1, DUK_ENUM_OWN_PROPERTIES_ONLY);

    while (duk_next(ctx, -1 /*enum_idx*/, 1)) {
      /* [ ... response enum key value ] */
      headers.insert(pair<string,string>(duk_to_string(ctx, -2), duk_to_string(ctx, -1)));
      duk_pop_2(ctx);  /* pop key value  */
    }

    duk_pop(ctx); // popping enum object
  } else {
    duk_pop(ctx);
  }

  // getting content body
  /* [ ... response ] */
  duk_push_string(ctx, "body");
  if(duk_get_prop(ctx, 0)) {
    /* [ ... response body ] */
    content_body = duk_to_string(ctx, -1);
    duk_pop(ctx);
  } else {
    duk_pop(ctx);
  }

  // getting status code
  /* [ ... response ] */
  duk_push_string(ctx, "statusCode");
  if(duk_get_prop(ctx, 0)) {
    /* [ ... response status_code ] */
    status_code = duk_to_int(ctx, -1);
    duk_pop(ctx);
  } else {
    duk_pop(ctx);
  }

  // getting status message
  /* [ ... response ] */
  duk_push_string(ctx, "statusMessage");
  if(duk_get_prop(ctx, 0)) {
    /* [ ... response status_message ] */
    status_message = duk_to_string(ctx, -1);
    duk_pop(ctx);
  } else {
    duk_pop(ctx);
  }

  duk_pop(ctx);
  /* [ ... ] */

  app->getMutex()->unlock();

  app_response->setHeaders(headers);
  app_response->setStatusCode(status_code);
  app_response->setStatusMessage(status_message);
  app_response->setContent(content_body);

  app->setResponseObj(app_response);

  return 0;
}

duk_ret_t JSApplication::cb_load_swagger_fragment(duk_context *ctx) {
  /* Entry [ ... swagger_as_string ] */

  JSApplication *app = JSApplication::getApplications().at(ctx);
  const char * sw_frag = duk_require_string(ctx,0);
  app->swagger_fragment_ = sw_frag;

  duk_pop(ctx);
  /* [ ... ] */

  return 0;
}

bool JSApplication::applicationExists(const char* path) {
  vector<string> names = listApplicationNames();

  for(unsigned int i = 0; i < names.size(); ++i) {
    if(string(path) == names.at(i))
      return true;
  }

  return false;
}

vector<string> JSApplication::listApplicationNames() {
  vector<string> names;

  DIR *dir;
  struct dirent *ent;

  if ((dir = opendir (Constant::Paths::APPLICATIONS_ROOT)) != NULL) {
    // clearing old application names
    names.clear();

    /* print all the files and directories within directory */
    while ((ent = readdir (dir)) != NULL) {
      string temp_name = ent->d_name;
      if(temp_name != "." && temp_name != "..") {
        names.push_back(ent->d_name);
      }
    }
    closedir (dir);
  } else {
    /* could not open directory */
    // perror ("");
    return names;
  }

  return names;

}

void JSApplication::getJoinThreads() {
    for (  map<duk_context*, thread*>::const_iterator it=app_threads_.begin(); it!=app_threads_.end(); ++it) {
      it->second->join();
    }
}

void JSApplication::parsePackageJS() {
  duk_context *ctx = getContext();
  int sourceLen;

  if(!ctx) return;

  DBOUT ("parsePackageJS(): Loading package file");
  // reading the package.json file to memory
  string package_file = getAppPath() + string(Constant::Paths::PACKAGE_JSON);
  package_js_ = load_js_file(package_file.c_str(),sourceLen);

  DBOUT ("parsePackageJS(): Reading package to attr");
  map<string,string> package_json_attr = read_package_json(duk_context_, package_js_);
  raw_package_js_data_ = package_json_attr;

  if(package_json_attr.find(Constant::Attributes::APP_NAME) != package_json_attr.end())
    name_ = package_json_attr.at(Constant::Attributes::APP_NAME);

  if(package_json_attr.find(Constant::Attributes::APP_VERSION) != package_json_attr.end())
    version_ = package_json_attr.at(Constant::Attributes::APP_VERSION);

  if(package_json_attr.find(Constant::Attributes::APP_DESCRIPTION) != package_json_attr.end())
    description_ = package_json_attr.at(Constant::Attributes::APP_DESCRIPTION);

  if(package_json_attr.find(Constant::Attributes::APP_AUTHOR) != package_json_attr.end())
    author_ = package_json_attr.at(Constant::Attributes::APP_AUTHOR);

  if(package_json_attr.find(Constant::Attributes::APP_LICENCE) != package_json_attr.end())
    licence_ = package_json_attr.at(Constant::Attributes::APP_LICENCE);

  if(package_json_attr.find(Constant::Attributes::APP_MAIN) != package_json_attr.end())
    main_ = package_json_attr.at(Constant::Attributes::APP_MAIN);

  if(package_json_attr.find(Constant::Attributes::APP_ID) != package_json_attr.end())
    id_ = stoi(package_json_attr.at(Constant::Attributes::APP_ID));
}
