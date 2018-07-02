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
using std::to_string;

#include "node_module_search.h"
#include "util.h"
#include "file_ops.h"
#include "logs/app_log.h"
#include "logs/read_app_log.h"
#include "device.h"
#include "duk_util.h"

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
vector<string> JSApplication::app_names_;
recursive_mutex JSApplication::static_mutex_;

JSApplication::JSApplication(const char* path) {
  {
    std::lock_guard<recursive_mutex> static_lock(getStaticMutex());

    // adding app_path to global app paths
    setAppPath(path);

    if(getAppPath().length() > 0 && getAppPath().at(getAppPath().length()-1) != '/')
      setAppPath(getAppPath()+"/");

    init();

    unsigned int ai_len = getAppInterfaces().size();
    for(unsigned int i = 0; i < ai_len; ++i) {
        Device::getInstance().registerAppApi(getAppInterfaces().at(i), getSwaggerFragment());
    }

    string app_payload = getAppAsJSON();
    bool app_is = Device::getInstance().appExists(to_string(getAppId()));

    if(!app_is) {
      Device::getInstance().registerApp(app_payload);
    }
  }
}

void JSApplication::init() {
  {
    std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
    DBOUT("init()");

    // creating new duk heap for the js application
    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_context_ = duk_create_heap(NULL, NULL, NULL, this, custom_fatal_handler);
    }

    if (!getContext()) {
      throw "Duk context could not be created.";
    }

    if(getOptions().size() == 0) {
      map<string, map<string,string> > full_config = get_config(getContext());

      if(full_config.find(Constant::Attributes::GENERAL) != full_config.end())
        setOptions(full_config.at(Constant::Attributes::GENERAL));
    }

    // setting app state to initializing
    updateAppState(APP_STATES::INITIALIZING);

    DBOUT ("Inserting application");
    // adding this application to static apps
    addApplication(this);

    DBOUT ("Duktape initializations");
    DBOUT ("Duktape initializations: print alert");
    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_global_object(getContext());
      duk_push_string(getContext(), "print");
      duk_push_c_function(getContext(), cb_print, DUK_VARARGS);
      duk_def_prop(getContext(), -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_WRITABLE | DUK_DEFPROP_SET_CONFIGURABLE);
      duk_push_string(getContext(), "alert");
      duk_push_c_function(getContext(), cb_alert, DUK_VARARGS);
      duk_def_prop(getContext(), -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_WRITABLE | DUK_DEFPROP_SET_CONFIGURABLE);
      duk_pop(getContext());
    }

    DBOUT ("Duktape initializations: console");
    // initializing console (enables console in js)
    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_console_init(getContext(), DUK_CONSOLE_PROXY_WRAPPER /*flags*/);
    }

    DBOUT ("Duktape initializations: pushin object");
    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      // initializing node.js require
      duk_push_object(getContext());
      DBOUT ("Pushing resolve module");
      duk_push_c_function(getContext(), cb_resolve_module, DUK_VARARGS);
      duk_put_prop_string(getContext(), -2, "resolve");
      DBOUT ("Pushing load module");
      duk_push_c_function(getContext(), cb_load_module, DUK_VARARGS);
      duk_put_prop_string(getContext(), -2, "load");
      duk_module_node_init(getContext());
    }

    DBOUT ("Adding response resolution");

    /* Set global 'ResolveResponse'. */
    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_global_object(getContext());
      duk_push_c_function(getContext(), cb_resolve_app_response, 1);
      duk_put_prop_string(getContext(), -2, "ResolveResponse");
      duk_pop(getContext());
    }

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_global_object(getContext());
      duk_push_c_function(getContext(), cb_load_swagger_fragment, 1);
      duk_put_prop_string(getContext(), -2, "LoadSwaggerFragment");
      duk_pop(getContext());
    }

    DBOUT ("Creating new eventloop");
    // registering/creating the eventloop
    createEventLoop();

    // loading the event loop javascript functions (setTimeout ect.)
    int evlLen;
    char* evlSource = load_js_file(Constant::Paths::EVENT_LOOP_JS,evlLen);

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), evlSource);
      if (duk_peval(getContext()) != 0) {
        printf("Failed to evaluate custom_eventloop.js: %s\n", duk_safe_to_string(getContext(), -1));
      }
      duk_pop(getContext());
    }

    int tmpLen;
    char* headerSource = load_js_file(Constant::Paths::APPLICATION_HEADER_JS,tmpLen);

    // evaluating ready made headers
    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), headerSource);
      if(duk_peval(getContext()) != 0) {
        printf("Script error: %s\n", duk_safe_to_string(getContext(), -1));
      }
      duk_pop(getContext());
    }

    // evaluating necessary js files that main.js will need
    char* agentSource = load_js_file(Constant::Paths::AGENT_JS,tmpLen);
    char* apiSource = load_js_file(Constant::Paths::API_JS,tmpLen);
    char* appSource = load_js_file(Constant::Paths::APP_JS,tmpLen);
    char* requestSource = load_js_file(Constant::Paths::REQUEST_JS,tmpLen);
    char* responseSource = load_js_file(Constant::Paths::RESPONSE_JS,tmpLen);

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), agentSource);
      if (duk_peval(getContext()) != 0) {
        printf("Failed to evaluate agent.js: %s\n", duk_safe_to_string(getContext(), -1));
      }
      duk_pop(getContext());
    }

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), apiSource);
      if (duk_peval(getContext()) != 0) {
        printf("Failed to evaluate api.js: %s\n", duk_safe_to_string(getContext(), -1));
      }
      duk_pop(getContext());
    }

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), appSource);
      if (duk_peval(getContext()) != 0) {
        printf("Failed to evaluate app.js: %s\n", duk_safe_to_string(getContext(), -1));
      }
      duk_pop(getContext());
    }

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), requestSource);
      if (duk_peval(getContext()) != 0) {
        printf("Failed to evaluate request.js: %s\n", duk_safe_to_string(getContext(), -1));
      }
      duk_pop(getContext());
    }

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), responseSource);
      if (duk_peval(getContext()) != 0) {
        printf("Failed to evaluate response.js: %s\n", duk_safe_to_string(getContext(), -1));
      }
      duk_pop(getContext());
    }

    /** reding the source code (main.js) for the app **/

    int source_len;

    parsePackageJS();

    string main_file = getAppPath() + "/" + getAppMain();

    DBOUT ("JSApplication(): Loading liquidiot file");
    // reading the package.json file to memory
    string liquidiot_file = getAppPath() + string(Constant::Paths::LIQUID_IOT_JSON);
    string liquidiot_js = load_js_file(liquidiot_file.c_str(),source_len);

    DBOUT ("JSApplication(): Reading json to attr");
    map<string,vector<string> > liquidiot_json_attr = read_liquidiot_json(getContext(), liquidiot_js.c_str());
    if(liquidiot_json_attr.find(Constant::Attributes::APP_INTERFACES) != liquidiot_json_attr.end()) {
      setAppInterfaces(liquidiot_json_attr.at(Constant::Attributes::APP_INTERFACES));
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
    setJSSource(temp_source.c_str(), temp_source.length());

    DBOUT ("JSApplication(): Making it main node module");
    // pushing this as main module
    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), getJSSource());

      if(duk_module_node_peval_main(getContext(), getAppMain().c_str()) != 0) {
        printf("Failed to evaluate main module: %s\n", duk_safe_to_string(getContext(), -1));
      }
    }

    DBOUT ("Inserting application thread");
    // initializing the thread, this must be the last initialization
    createEventLoopThread();
    addApplicationThread(this);
  }
}

void JSApplication::clean() {
  // updating app state before locking
  updateAppState(APP_STATES::PAUSED, true);

  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());

    DBOUT ( "clean()" );
    // if the application is running we need to stop the thread
    getEventLoop()->setRequestExit(true);
    // waiting for thread to stop the execution (come back from poll)
    DBOUT ("clean(): waiting for thread over" );
    try {
      while (getEventLoopThread()->joinable()) {
        // waiting eventloop poll to finish
        poll(NULL,0,getEventLoop()->getCurrentTimeout());
        if(getEventLoopThread()->joinable())
          getEventLoopThread()->join();
      }
    } catch (const std::system_error& e) {
      std::cerr << "Caught a system_error\n";
      if(e.code() == std::errc::invalid_argument)
        std::cerr << "The error condition is std::errc::invalid_argument\n";
        std::cerr << "the error description is " << e.what() << '\n';
    }

    cout << "clean(): killing thread was successful" << endl;

    cout << "clean(): thread waiting over and calling terminate" << endl;

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), "app.internal.terminate(function(){});");
      if(duk_peval(getContext()) != 0) {
        printf("Script error: %s\n", duk_safe_to_string(getContext(), -1));
      }
    }

    deleteEventLoopThread();

    {
      std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
      app_threads_.erase(getContext());
    }

    deleteEventLoop();

    cout << "clean(): destoying duk heap" << endl;
    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_destroy_heap(getContext());
    }

    cout << "clean(): cleaning the app out" << getContext() << endl;
    {
      std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
      applications_.erase(getContext());
    }

    duk_context_ = 0;
  }
}

bool JSApplication::createEventLoopThread() {
  if(ev_thread_) return false;

  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    ev_thread_ = new thread(&JSApplication::run,this);
  }

  return ev_thread_ ? true : false;
}

bool JSApplication::deleteEventLoopThread() {
  if(getEventLoopThread()) {
    {
      std::lock_guard<recursive_mutex> app_lock(getAppMutex());
      delete ev_thread_;
      ev_thread_ = 0;
    }
    return true;
  }
  return false;
}

EventLoop* JSApplication::createEventLoop() {
  if(getEventLoop()) return getEventLoop();

  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    eventloop_ = new EventLoop(getContext());
  }

  return getEventLoop();
}

bool JSApplication::deleteEventLoop() {
  if(getEventLoop()) {
    {
      std::lock_guard<recursive_mutex> app_lock(getAppMutex());
      delete eventloop_;
      eventloop_ = 0;
    }
    return true;
  }
  return false;
}

void JSApplication::setJSSource(const char * source, int source_len) {
  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    if(source_code_) {
      delete source_code_;
      source_code_ = 0;
    }
    source_code_ = new char[source_len+1];
    strcpy(source_code_,source);
  }
}

void JSApplication::setAppName(const string& name) {
  setRawPackageJSONDataField(Constant::Attributes::APP_NAME, name);
  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    name_ = name;
  }
}

void JSApplication::setAppVersion(const string& version) {
  setRawPackageJSONDataField(Constant::Attributes::APP_VERSION, version);
  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    version_ = version;
  }
}

void JSApplication::setAppDesciption(const string& description) {
  setRawPackageJSONDataField(Constant::Attributes::APP_DESCRIPTION, description);
  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    description_ = description;
  }
}

void JSApplication::setAppAuthor(const string& author) {
  setRawPackageJSONDataField(Constant::Attributes::APP_AUTHOR, author);
  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    author_ = author;
  }
}

void JSApplication::setAppLicence(const string& licence) {
  setRawPackageJSONDataField(Constant::Attributes::APP_LICENCE, licence);
  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    licence_ = licence;
  }
}

void JSApplication::setAppMain(const string& main) {
  setRawPackageJSONDataField(Constant::Attributes::APP_MAIN, main);
  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    main_ = main;
  }
}

void JSApplication::setAppId(const string& id) {
  setRawPackageJSONDataField(Constant::Attributes::APP_ID, id);
  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    id_ = stoi(id);
  }
}

bool JSApplication::setAppState(APP_STATES state) {
  // trivial case
  if(state == getAppState()) return true;

  if(state == APP_STATES::PAUSED && getAppState() == APP_STATES::RUNNING) {
    {
      std::lock_guard<recursive_mutex> app_lock(getAppMutex());
      app_state_ = state;
    }
    return pause();
  }

  else if(state == APP_STATES::RUNNING && getAppState() == APP_STATES::PAUSED) {
    {
      std::lock_guard<recursive_mutex> app_lock(getAppMutex());
      app_state_ = state;
    }
    return start();
  }

  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    app_state_ = state;
  }
  return false;
}

void JSApplication::setRawPackageJSONDataField(const string& key, const string& value) {
  if(getRawPackageJSONData().find(key) != getRawPackageJSONData().end()) {
    {
      std::lock_guard<recursive_mutex> app_lock(getAppMutex());
      raw_package_json_data_.at(key) = value;
    }
    return;
  }

  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    raw_package_json_data_.insert(pair<string,string>(key,value));
  }
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

  {
    std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
    duk_push_string(getContext(), sourceCode.c_str());
    if (duk_peval(getContext()) != 0) {
      printf("Failed to evaluate response: %s\n", duk_safe_to_string(getContext(), -1));
    }
    duk_pop(getContext());
  }

  return app_response_;
}

void JSApplication::setResponseObj(AppResponse *response) {
  // locking thread as it's possible that this is called ny something else
  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    if(app_response_) {
      delete app_response_;
      app_response_ = 0;
    }
    app_response_ = response;
  }
}

void JSApplication::addApplication( JSApplication *app ) {
  if(!app) return;

  if( getApplications().find( app->getContext()) != getApplications().end() ) return;

  {
    std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
    applications_.insert(pair<duk_context*,JSApplication*>(app->getContext(),app));
  }
}

bool JSApplication::deleteApplication(JSApplication *app) {
  // have to delete app before we clean (otherwise can really find it)
  {
    std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
    applications_.erase(app->getContext());
  }
  app->clean();
  if(delete_files(app->getAppPath().c_str()))
    return 1;

  return 0;
}

void JSApplication::addApplicationThread( JSApplication *app ) {
  if(!app) return;

  if( getApplicationThreads().find( app->getContext() ) != getApplicationThreads().end() ) return;

  {
    std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
    app_threads_.insert(pair<duk_context*,thread*>(app->getContext(),app->getEventLoopThread()));
  }
}

int JSApplication::getNextId(bool generate_new) {
  if(!generate_new) return next_id_;

  {
    std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
    bool ok = false;

    while(!ok) {
      next_id_++;
      ok = true;
      for(const pair<duk_context*,JSApplication*> &item : getApplications()) {
        if( item.second->getAppId() == next_id_ ) {
          ok = false;
        }
      }
    }
  }

  return next_id_;
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

bool JSApplication::pause() {
  DBOUT ( "pause()" );
  if(getAppState() == APP_STATES::PAUSED) return true;

  updateAppState(APP_STATES::PAUSED, true);

  return true;
}

bool JSApplication::start() {
  DBOUT ( "start()" );
  if(getAppState() == APP_STATES::RUNNING) return true;

  updateAppState(APP_STATES::RUNNING, true);

  return true;
}

void JSApplication::run() {
  updateAppState(APP_STATES::RUNNING, true);

  {
    std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
    int rc = duk_safe_call(getContext(), EventLoop::eventloop_run, NULL, 0 /*nargs*/, 1 /*nrets*/);
    if (rc != 0) {
      updateAppState(APP_STATES::CRASHED, true);
      fprintf(stderr, "eventloop_run() failed: %s\n", duk_to_string(getContext(), -1));
      fflush(stderr);
    }
    duk_pop(getContext());
  }
}

void JSApplication::reload() {
  // TODO explore if this check is necessary
  if(getAppState() == APP_STATES::RUNNING) {
    // clearing application of all previous states and executions
    clean();
    // initialize app
    init();
  }
}

void JSApplication::updateAppState(APP_STATES state, bool update_client) {
  setAppState(state);

  if(update_client) {
    // TODO
    // Device::getInstance().updateApp(std::to_string(getId()),getAppAsJSON());
  }
}

string JSApplication::getDescriptionAsJSON() {
  string full_status;
  duk_context *ctx = getContext();

  {
    std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
    duk_push_object(ctx);

    // name
    if(getAppName().length() > 0) {
      duk_push_string(ctx,getAppName().c_str());
      duk_put_prop_string(ctx, -2, Constant::Attributes::APP_NAME);
    }

    // version
    if(getAppVersion().length() > 0) {
      duk_push_string(ctx,getAppVersion().c_str());
      duk_put_prop_string(ctx, -2, Constant::Attributes::APP_VERSION);
    }

    // description
    if(getAppDescription().length() > 0) {
      duk_push_string(ctx,getAppDescription().c_str());
      duk_put_prop_string(ctx, -2, Constant::Attributes::APP_DESCRIPTION);
    }

    // main
    if(getAppMain().length() > 0) {
      duk_push_string(ctx,getAppMain().c_str());
      duk_put_prop_string(ctx, -2, Constant::Attributes::APP_MAIN);
    }

    // id
    if(getAppId() >= 0) {
      duk_push_int(ctx, getAppId());
      duk_put_prop_string(ctx, -2, Constant::Attributes::APP_ID);
    }

    // status
    if(getAppState() >= 0 && getAppState() < 4) {
      duk_push_string(ctx,APP_STATES_CHAR.at(getAppState()).c_str());
      duk_put_prop_string(ctx, -2, Constant::Attributes::APP_STATE);
    }

    full_status = duk_json_encode(ctx, -1);
    duk_pop(ctx);
  }

  return full_status;
}

string JSApplication::getAppAsJSON() {
  if(!getContext()) return "";

  string full_status;
  duk_context *ctx = getContext();

  {
    std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
    duk_push_object(ctx);
    /* [... obj ] */

    // name
    duk_push_string(ctx,getAppName().c_str());
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_NAME);

    // version
    duk_push_string(ctx,getAppVersion().c_str());
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_VERSION);

    // description
    duk_push_string(ctx,getAppDescription().c_str());
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_DESCRIPTION);

    // author
    duk_push_string(ctx,getAppAuthor().c_str());
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_AUTHOR);

    // licence
    duk_push_string(ctx,getAppLicence().c_str());
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_LICENCE);

    // id
    if(id_ >= 0) {
      duk_push_int(ctx, getAppId());
      duk_put_prop_string(ctx, -2, Constant::Attributes::APP_ID);
    }

    // status
    if(getAppState() >= 0 && getAppState() < 4) {
      duk_push_string(ctx,APP_STATES_CHAR.at(getAppState()).c_str());
      duk_put_prop_string(ctx, -2, Constant::Attributes::APP_STATE);
    }

    // applicationInterfaces ** Note ** This is not defined by api description O_o
    // however this seems to replace classes
    int arr_idx = duk_push_array(ctx);
    unsigned int ai_len = getAppInterfaces().size();
    for(unsigned int i = 0; i < ai_len; ++i) {
      duk_push_string(ctx, getAppInterfaces().at(i).c_str());
      duk_put_prop_index(ctx, arr_idx, i);
    }
    duk_put_prop_string(ctx, -2, Constant::Attributes::APP_INTERFACES);

    full_status = duk_json_encode(ctx, -1);
    duk_pop(ctx);
  }

  return full_status;
}

duk_ret_t JSApplication::cb_resolve_module(duk_context *ctx) {
  JSApplication *app = JSApplication::getApplications().at(ctx);

  {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());

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
    string path = app->getAppPath() + "/";

    // if buffer is requested just use native duktape implemented buffer
    if(requested_id_str == "buffer") {
      duk_push_string(ctx, requested_id);
      return 1;  /*nrets*/
    }

    if(requested_id_str.size() > 0) {
      // First checking core modules
      if (requested_id_str.at(0) != '.' && requested_id_str.at(0) != '/' &&
          getOptions().find(Constant::Attributes::CORE_LIB_PATH) != getOptions().end()) {
        string core_lib_path = getOptions().at(Constant::Attributes::CORE_LIB_PATH);
        if(core_lib_path.size() > 0) {
          if (node::is_core_module(core_lib_path + "/" + string(requested_id))) {
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
  }

  return 1;  /*nrets*/
}

duk_ret_t JSApplication::cb_load_module(duk_context *ctx) {
  JSApplication *app = JSApplication::getApplications().at(ctx);
  {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());
    /*
     *  Entry stack: [ resolved_id exports module ]
     */

    const char *resolved_id = duk_require_string(ctx, 0);
    duk_get_prop_string(ctx, 2, "filename");
    const char *filename = duk_require_string(ctx, -1);
    (void)filename;

    if (string(resolved_id) == "buffer") {
      duk_push_string(ctx,"module.exports.Buffer = Buffer;");
      return 1;
    }

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
  }

  return 1;  /*nrets*/
}

duk_ret_t JSApplication::cb_print (duk_context *ctx) {
  JSApplication *app = JSApplication::getApplications().at(ctx);
  {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());
    duk_idx_t nargs = duk_get_top(ctx);

    duk_push_string(ctx, " ");
    duk_insert(ctx, 0);
    duk_concat(ctx, nargs);

    // writing to log
    AppLog(app->getAppPath().c_str()) << AppLog::getTimeStamp() << " [Print] " << duk_require_string(ctx, -1) << endl;
  }

  return 0;
}

duk_ret_t JSApplication::cb_alert (duk_context *ctx) {
  JSApplication *app = JSApplication::getApplications().at(ctx);
  {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());

    duk_idx_t nargs = duk_get_top(ctx);

    duk_push_string(ctx, " ");
    duk_insert(ctx, 0);
    duk_concat(ctx, nargs);

    // writing to log
    AppLog(app->getAppPath().c_str()) << AppLog::getTimeStamp() << " [Alert] " << duk_require_string(ctx, -1) << endl;
  }

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
  {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());

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
  }

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
  {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());
    const char * sw_frag = duk_require_string(ctx,0);
    app->setSwaggerFragment(sw_frag);

    duk_pop(ctx);
    /* [ ... ] */
  }

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

const vector<string>& JSApplication::listApplicationNames() {

  DIR *dir;
  struct dirent *ent;

  if ((dir = opendir (Constant::Paths::APPLICATIONS_ROOT)) != NULL) {
    // clearing old application names
    app_names_.clear();

    /* print all the files and directories within directory */
    while ((ent = readdir (dir)) != NULL) {
      string temp_name = ent->d_name;
      if(temp_name != "." && temp_name != "..") {
        app_names_.push_back(ent->d_name);
      }
    }
    closedir (dir);
  } else {
    /* could not open directory */
    // perror ("");
    return app_names_;
  }

  return app_names_;
}

void JSApplication::getJoinThreads() {
    for (  map<duk_context*, thread*>::const_iterator it=getApplicationThreads().begin(); it!=getApplicationThreads().end(); ++it) {
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
  setPackageJSON(load_js_file(package_file.c_str(),sourceLen));

  DBOUT ("parsePackageJS(): Reading package to attr");
  map<string,string> package_json_attr = read_package_json(getContext(), getPackageJSON());
  setRawPackageJSONData(package_json_attr);

  if(package_json_attr.find(Constant::Attributes::APP_NAME) != package_json_attr.end())
    setAppName( package_json_attr.at(Constant::Attributes::APP_NAME ));

  if(package_json_attr.find(Constant::Attributes::APP_VERSION) != package_json_attr.end())
    setAppVersion(package_json_attr.at(Constant::Attributes::APP_VERSION));

  if(package_json_attr.find(Constant::Attributes::APP_DESCRIPTION) != package_json_attr.end())
    setAppDesciption(package_json_attr.at(Constant::Attributes::APP_DESCRIPTION));

  if(package_json_attr.find(Constant::Attributes::APP_AUTHOR) != package_json_attr.end())
    setAppAuthor(package_json_attr.at(Constant::Attributes::APP_AUTHOR));

  if(package_json_attr.find(Constant::Attributes::APP_LICENCE) != package_json_attr.end())
    setAppLicence(package_json_attr.at(Constant::Attributes::APP_LICENCE));

  if(package_json_attr.find(Constant::Attributes::APP_MAIN) != package_json_attr.end())
    setAppMain(package_json_attr.at(Constant::Attributes::APP_MAIN));

  if(package_json_attr.find(Constant::Attributes::APP_ID) != package_json_attr.end()) {
    setAppId(package_json_attr.at(Constant::Attributes::APP_ID));
  } else {
    // setting id for the application
    setAppId(to_string(getNextId(false)));
    getNextId(true);
  }

}
