#include "application.h"

#include <algorithm>
#include <regex>

using std::regex;
using std::endl;
using std::to_string;

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
namespace fs = boost::filesystem;

#include "node_module_search.h"
#include "util.h"
#include "file_ops.h"
#include "logs/app_log.h"
#include "logs/read_app_log.h"
#include "duk_util.h"
#include "globals.h"

// KAAAPPAA
#include "duktape_interface.h"
#include "duktape_js_object.h"

const array<string,4> JSApplication::APP_STATES_CHAR = { {
  Constant::String::INITIALIZING,
  Constant::String::CRASHED,
  Constant::String::RUNNING,
  Constant::String::PAUSED} };
int JSApplication::next_id_ = 0;
map<string,string> JSApplication::options_;
map<duk_context*, JSApplication*> JSApplication::applications_;
vector<string> JSApplication::app_names_;
recursive_mutex JSApplication::static_mutex_;

JSApplication::JSApplication(const char* path):
  eventloop_(0), duk_context_(0), package_json_(0), source_code_(0), app_path_(""),
  name_(""), version_(""), description_(""), author_(""), licence_(""), main_(""),
  id_(-1), app_state_(APP_STATES::INITIALIZING), application_interfaces_(), rest_api_paths_(),
  raw_package_json_data_(), app_response_(0), swagger_fragment_(""), is_ready_(false),duktape_interface_(0) {
  defaultConstruct(path);
}

JSApplication::JSApplication(const char* path, int id):
  eventloop_(0), duk_context_(0), package_json_(0), source_code_(0), app_path_(""),
  name_(""), version_(""), description_(""), author_(""), licence_(""), main_(""),
  id_(id), app_state_(APP_STATES::INITIALIZING), application_interfaces_(), rest_api_paths_(),
  raw_package_json_data_(), app_response_(0), swagger_fragment_(""), is_ready_(false),duktape_interface_(0) {
  setAppId(std::to_string(id_));
  defaultConstruct(path);
}

JSApplication::~JSApplication() {
  if( duk_context_ != 0) {
    duk_destroy_heap(duk_context_);
    duk_context_ = 0;
  }

  application_interfaces_.clear();
  rest_api_paths_.clear();
  raw_package_json_data_.clear();

  if( app_response_ != 0) {
    delete app_response_;
    app_response_ = 0;
  }

  INFOOUT("Application " << getAppName() << " closed.");
}

void JSApplication::defaultConstruct(const char* path) {
  {
    INFOOUT("Creating new application to " << path);
    DBOUT("JSApplication()");
    // making sure we are not disturbed during the execution of construction
    std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());

    // adding app_path to global app paths
    string exec_path = Device::getInstance().getExecPath() + "/";
    setAppPath(path);

    if(getAppPath().length() > 0 && getAppPath().at(getAppPath().length()-1) != '/')
      setAppPath(getAppPath()+"/");

    DBOUT("JSApplication(): app path: " << getAppPath());
    if(is_file((exec_path + getAppPath()).c_str()) != FILE_TYPE::PATH_TO_DIR) {
      ERROUT("JSApplication(): File " << getAppPath() << " not a directory.");
      JSApplication::shutdownApplication(this);
      throw "Not directory";
    }

    DBOUT("JSApplication(): init()");
    if(!init()) {
      ERROUT("JSApplication(): Failed to initialize");
      JSApplication::shutdownApplication(this);
      throw "JSApplication(): Failed to initialize";
    }
  }
}

bool JSApplication::init() {
  {
    std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());

    setIsReady(false);
    DBOUT("init()");

    // creating new duk heap for the js application
    if(!getContext()){
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_context_ = duk_create_heap(NULL, NULL, NULL, this, custom_fatal_handler);
    }

    duktape_interface_ = std::shared_ptr<DuktapeInterface>(new DuktapeInterface(getContext()));
    duktape_interface_->setContextPath(getAppPath());

    if (!getContext()) {
      // throw "Duk context could not be created.";
      ERROUT("Duk context could not be created.");
      AppLog(getAppPath().c_str()) << AppLog::getTimeStamp() << " [" << Constant::String::LOG_FATAL_ERROR << "] " << "Duk context could not be created." << "\n";
      return false;
    }

    if(getOptions().size() == 0) {
      map<string,map<string,string> > full_config;
      try {
        full_config = get_config(getContext(), Device::getInstance().getExecPath());
      } catch (char const * e) {
        ERROUT("init(): Application failed to read config: " << e);
        AppLog(getAppPath().c_str()) << AppLog::getTimeStamp() << " [" << Constant::String::LOG_FATAL_ERROR << "] " << "Application failed to read config." << "\n";
        return false;
      }

      if(full_config.find(Constant::Attributes::GENERAL) != full_config.end())
        setOptions(full_config.at(Constant::Attributes::GENERAL));
    }

    // setting app state to initializing
    updateAppState(APP_STATES::INITIALIZING, false);

    DBOUT ("Inserting application");
    // adding this application to static apps
    addApplication(this);

    static bool clp_init = false;
    if(!clp_init && getOptions().find(Constant::Attributes::CORE_LIB_PATH) != getOptions().end()) {
        DuktapeInterface::setCoreLibPath(getOptions().at(Constant::Attributes::CORE_LIB_PATH));
        clp_init = true;
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

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_global_object(getContext());
      duk_push_c_function(getContext(), cb_register_app_api, 1);
      duk_put_prop_string(getContext(), -2, "RegisterAIPath");
      duk_pop(getContext());
    }

    DBOUT ("init(): Creating new eventloop");
    // registering/creating the eventloop
    createEventLoop();

    DBOUT ("init(): Loading eventloop js");
    // loading the event loop javascript functions (setTimeout ect.)
    int evlLen;
    string temp_path = Device::getInstance().getExecPath() + "/" + string(Constant::Paths::EVENT_LOOP_JS);
    char* evlSource = load_js_file(temp_path.c_str(),evlLen);
    if(evlLen <= 1) {
      ERROUT("init(): file read error");
      delete evlSource;
      evlSource = 0;
      return false;
    }

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), evlSource);
      if (duk_peval(getContext()) != 0) {
        ERROUT("Failed to evaluate custom_eventloop.js: " << duk_safe_to_string(getContext(), -1));
        return false;
      }
      duk_pop(getContext());
    }


    DBOUT ("init(): Loading application header js");
    int tmpLen;
    temp_path = Device::getInstance().getExecPath() + "/" + string(Constant::Paths::APPLICATION_HEADER_JS);
    char* headerSource = load_js_file(temp_path.c_str(),tmpLen);
    if(tmpLen <= 1) {
      ERROUT("init(): file read error");
      delete headerSource;
      headerSource = 0;
      return false;
    }

    // evaluating ready made headers
    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), headerSource);
      if(duk_peval(getContext()) != 0) {
        ERROUT("Script error: " << duk_safe_to_string(getContext(), -1));
        return false;
      }
      duk_pop(getContext());
    }

    DBOUT ("init(): Misc js loads");
    // evaluating necessary js files that main.js will need
    temp_path = Device::getInstance().getExecPath() + "/" + string(Constant::Paths::AGENT_JS);
    char* agentSource = load_js_file(temp_path.c_str(),tmpLen);
    if(tmpLen <= 1) {
      ERROUT("init(): file read error");
      delete agentSource;
      agentSource = 0;
      return false;
    }
    temp_path = Device::getInstance().getExecPath() + "/" + string(Constant::Paths::REQUEST_JS);
    char* requestSource = load_js_file(temp_path.c_str(),tmpLen);
    if(tmpLen <= 1) {
      ERROUT("init(): file read error");
      delete requestSource;
      requestSource = 0;
      return false;
    }
    temp_path = Device::getInstance().getExecPath() + "/" + string(Constant::Paths::RESPONSE_JS);
    char* responseSource = load_js_file(temp_path.c_str(),tmpLen);
    if(tmpLen <= 1) {
      ERROUT("init(): file read error");
      delete responseSource;
      responseSource = 0;
      return false;
    }

    temp_path = Device::getInstance().getExecPath() + "/" + string(Constant::Paths::ROUTER_JS);
    char* routerSource = load_js_file(temp_path.c_str(),tmpLen);
    if(tmpLen <= 1) {
      ERROUT("init(): file read error");
      delete routerSource;
      routerSource = 0;
      return false;
    }

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), agentSource);
      if (duk_peval(getContext()) != 0) {
        ERROUT("Failed to evaluate agent.js: " << duk_safe_to_string(getContext(), -1));
        return false;
      }
      duk_pop(getContext());
    }

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), requestSource);
      if (duk_peval(getContext()) != 0) {
        ERROUT("Failed to evaluate request.js: " << duk_safe_to_string(getContext(), -1));
        return false;
      }
      duk_pop(getContext());
    }

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), responseSource);
      if (duk_peval(getContext()) != 0) {
        ERROUT("Failed to evaluate response.js: " << duk_safe_to_string(getContext(), -1));
        return false;
      }
      duk_pop(getContext());
    }

    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      duk_push_string(getContext(), routerSource);
      if (duk_peval(getContext()) != 0) {
        ERROUT("Failed to evaluate router.js: " << duk_safe_to_string(getContext(), -1));
        return false;
      }
      duk_pop(getContext());
    }

    /** reding the source code (main.js) for the app **/

    int source_len;

    parsePackageJS();

    string main_file = Device::getInstance().getExecPath() + "/" + getAppPath() + "/" + getAppMain();

    DBOUT ("init(): Loading liquidiot file");
    // reading the package.json file to memory
    string liquidiot_file = Device::getInstance().getExecPath() + "/" + getAppPath() + string(Constant::Paths::LIQUID_IOT_JSON);
    char * tmp_liquidiot_js = load_js_file(liquidiot_file.c_str(),source_len);
    string liquidiot_js(tmp_liquidiot_js);
    if(source_len <= 1) {
      ERROUT("init(): file read error");
      return false;
    }

    DBOUT ("init(): Reading json to attr");
    {
      std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
      map<string,vector<string> > liquidiot_json_attr;
      try {
        liquidiot_json_attr = read_liquidiot_json(getContext(), liquidiot_js.c_str());
      } catch (char const * e) {
        ERROUT("init(): Failed to read liquidiot.json: " << e);
        AppLog(getAppPath().c_str()) << AppLog::getTimeStamp() << " [" << Constant::String::LOG_FATAL_ERROR << "] " << "Failed to read liquidiot.json." << "\n";
        return false;
      }

      if(liquidiot_json_attr.find(Constant::Attributes::APP_INTERFACES) != liquidiot_json_attr.end()) {
        setAppInterfaces(liquidiot_json_attr.at(Constant::Attributes::APP_INTERFACES));
      }
    }

    DBOUT ("init(): Creating full source code");
    char * tmp = load_js_file(main_file.c_str(),source_len);
    string temp_source =  string(tmp) +
      "\napp = {};\n"
      "Agent(app);\n"
      "router = {};\n"
      "Router(router);\n"
      "module.exports(app,router,0,console);\n"
      "app.start();\n";

    // executing initialize code
    setJSSource(temp_source.c_str(), temp_source.length());

    js_instance_ = std::thread(&JSApplication::instantiate,this);
  }

  return true;
}

void JSApplication::instantiate() {
  std::lock_guard<recursive_mutex> app_lock(getAppMutex());
  std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
  DBOUT (__func__ << "(): Making it main node module");
  // pushing this as main module
  {
    std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());

    getEventLoop()->evloopRegister(getContext());

    const char * js_source = getJSSource();
    duk_push_string(getContext(), js_source);

    if(duk_module_node_peval_main(getContext(), getAppMain().c_str()) != 0) {
      ERROUT("Failed to evaluate main module: " << duk_safe_to_string(getContext(), -1));
      AppLog(getAppPath().c_str()) << AppLog::getTimeStamp() << " [" << Constant::String::LOG_ERROR << "] " << duk_safe_to_string(getContext(), -1) << "\n";
      return;
    }
    //duk_pop_2(getContext());
    //ERROUT(duk_safe_to_string(getContext(), -1));
  }

  DBOUT (__func__ << "(): Inserting application thread");
  // initializing the thread, this must be the last initialization
  createEventLoopThread();


  DBOUT(__func__ << "(): registerAppApi()");
  // unsigned int ai_len = getAppInterfaces().size();
  // for(unsigned int i = 0; i < ai_len; ++i) {
  //     Device::getInstance().registerAppApi(getAppInterfaces().at(i), getSwaggerFragment());
  // }

  DBOUT(__func__ << "(): getAppAsJSON()");
  string app_payload = getAppAsJSON();
  bool app_is = Device::getInstance().appExists(to_string(getAppId()));

  DBOUT(__func__ << "(): registerApp()");
  if(!app_is) {
    Device::getInstance().registerApp(app_payload);
  }

  DBOUT(__func__ << "(): pauseApp()");
  pause();
  DBOUT(__func__ << "(): startApp()");
  start();
  DBOUT(__func__ << "(): OK");
  INFOOUT("Application " << getAppName() << " started.");
}

bool JSApplication::createEventLoopThread() {

  {
    if(ev_thread_.joinable()) ev_thread_.join();
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    ev_thread_ = thread(&JSApplication::run,this);
  }

  return true;
}

bool JSApplication::deleteEventLoopThread() {
  std::lock_guard<recursive_mutex> app_lock(getAppMutex());

  try {
    if(getEventLoopThread().joinable()) {
      ev_thread_.join();
    }
  } catch (const std::system_error& e) {
    ERROUT("The error in joining ev thread: " << e.what());
  }

  return true;
}

EventLoop* JSApplication::createEventLoop() {
  if(getEventLoop()) return getEventLoop();

  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    EventLoop::createNewEventLoop(getContext(),0);
    eventloop_ = EventLoop::getEventLoop(getContext());
  }

  return getEventLoop();
}

bool JSApplication::deleteEventLoop() {
  if(getEventLoop()) {
    {
      std::lock_guard<recursive_mutex> app_lock(getAppMutex());
      EventLoop::deleteEventLoop(getContext());
      eventloop_ = 0;
    }
    return true;
  }
  return false;
}

void JSApplication::setJSSource(const char * source, int source_len) {
  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    if(source_code_ != 0) {
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
  DBOUT("setAppState()");
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
    DBOUT("setAppState(): Setting to running");
    {
      std::lock_guard<recursive_mutex> app_lock(getAppMutex());
      app_state_ = state;
    }
    getAppMutex().unlock();

    if(app_state_ == APP_STATES::RUNNING) {
      getCVMutex().unlock();
      getEventLoopCV().notify_all();
    }

    DBOUT("setAppState(): Running");

    return start();
  }

  {
    std::lock_guard<recursive_mutex> app_lock(getAppMutex());
    app_state_ = state;
  }
  getAppMutex().unlock();

  if(app_state_ == APP_STATES::RUNNING) {
    getCVMutex().unlock();
    getEventLoopCV().notify_all();
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
  DBOUT(__func__);
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
    "router.call_api(\"" + request->getAI() + "\");\n";

  {

    std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
    duk_push_string(getContext(), sourceCode.c_str());
    if (duk_peval(getContext()) != 0) {
      ERROUT("Failed to evaluate response: " << duk_safe_to_string(getContext(), -1));
    }
    duk_pop(getContext());
  }

  DBOUT(__func__ << ": OK");

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

  if( getApplications().find( app->getContext() ) != getApplications().end() ) return;

  {
    std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
    applications_.insert(pair<duk_context*,JSApplication*>(app->getContext(),app));
  }
}

bool JSApplication::deleteApplication(JSApplication *app) {
  std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
  DBOUT("deleteApplication()");
  {
    std::lock_guard<recursive_mutex> app_lock(app->getAppMutex());
    app->pause();
    Device::getInstance().deleteApp(std::to_string(app->getAppId()));

    DBOUT("deleteApplication(): Deleting files");
    delete_files((Device::getInstance().getExecPath() + "/" + app->getAppPath()).c_str()); /* There is a possibility of failure but we ignore it */
  }

  DBOUT ( "deleteApplication(): joining app thread" );
  try {
    std::lock_guard<recursive_mutex> app_lock(app->getAppMutex());
    {
      std::lock_guard<mutex> req_lock(app->getCVMutex());
      app->getEventLoop()->setRequestExit(true);
    }
    app->getCVMutex().unlock();
    app->getEventLoopCV().notify_all();
    DBOUT ( "deleteApplication(): wait for app" );
    {
      std::unique_lock<mutex> cv_lock(app->getCVMutex());
      app->getEventLoopCV().wait_for(cv_lock, std::chrono::seconds(1),[app]{
        return app->isReady()||interrupted;
      });
    }
    DBOUT ( "deleteApplication(): joining app" );
    if(app->getEventLoopThread().joinable())
      app->getEventLoopThread().join();
  } catch(std::system_error& e) {
    if(std::errc::invalid_argument == e.code()) {
      ERROUT("App thread was too slow to join. Good luck.");
    } else {
      ERROUT("Unexpected error: " << e.what());
    }
  }

  // delting the eventloop
  app->deleteEventLoop();

  DBOUT ( "deleteApplication(): cleaning the app out of ds" );
  applications_.erase(app->getContext());

  DBOUT ( "deleteApplication(): destoying duk heap" );
  if(app->getContext()) {
    std::lock_guard<recursive_mutex> duk_lock(app->getDuktapeMutex());
    duk_destroy_heap(app->getContext());
    app->duk_context_ = 0;
  }

  DBOUT("deleteApplication(): actual removal from memory");
  //delete app;
  //app = 0;

  DBOUT("deleteApplication(): Deleting success");
  return 1;
}

bool JSApplication::shutdownApplication(JSApplication *app) {
  std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
  DBOUT("shutdownApplication()");

  DBOUT("shutdownApplication(): deleting from RR manager");
  Device::getInstance().deleteApp(std::to_string(app->getAppId()));

  DBOUT ( "shutdownApplication(): joining app thread" );
  try {
    std::lock_guard<recursive_mutex> app_lock(app->getAppMutex());
    if(app->getEventLoop()){
      std::lock_guard<mutex> req_lock(app->getCVMutex());
      app->getEventLoop()->setRequestExit(true);
    } else {
      app->setIsReady(true);
    }
    app->getCVMutex().unlock();
    app->getEventLoopCV().notify_all();
    DBOUT ( "shutdownApplication(): wait for app" );
    {
      std::unique_lock<mutex> cv_lock(app->getCVMutex());
      app->getEventLoopCV().wait_for(cv_lock, std::chrono::seconds(1),[app]{
        return app->isReady()||interrupted;
      });
    }
    DBOUT ( "shutdownApplication(): joining app");
    if(app->getEventLoopThread().joinable())
      app->getEventLoopThread().join();
  } catch(std::system_error& e) {
    if(std::errc::invalid_argument == e.code()) {
      ERROUT("shutdownApplication(): App thread was too slow to join. Good luck.");
    } else {
      ERROUT("shutdownApplication(): Unexpected error: ");
    }
  }

  app->deleteEventLoop();

  DBOUT ( "shutdownApplication(): cleaning the app out of ds" );
  applications_.erase(app->getContext());

  DBOUT ( "shutdownApplication(): destoying duk heap" );
  if(app->getContext()) {
    std::lock_guard<recursive_mutex> duk_lock(app->getDuktapeMutex());
    duk_destroy_heap(app->getContext());
    app->duk_context_ = 0;
  }

  DBOUT("shutdownApplication(): actual removal from memory");
  //delete app;
  //app = 0;

  DBOUT("shutdownApplication(): Shutting down successful");
  return 1;
}

int JSApplication::getNextId(bool generate_new) {
  if(!generate_new) return next_id_;

  {
    std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
    bool ok = false;

    while(!ok) {
      next_id_++;
      ok = true;
      for(const pair<duk_context*,JSApplication*>& item : getApplications()) {
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
  std::regex e( R"(\[(\w{3}\s+\w{3}\s+\d{2}\s+\d{2}:\d{2}:\d{2}\s+\d{4})\]\s\[(\w+)\]\s([A-Za-z\d\s._:\/!]*)(\n|\n$)+)" );

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
  updateAppState(APP_STATES::RUNNING, false);

  {
    std::lock_guard<recursive_mutex> duktape_lock(getDuktapeMutex());
    int rc = duk_safe_call(getContext(), EventLoop::eventloop_run, NULL, 0 /*nargs*/, 1 /*nrets*/);
    if (rc != 0) {
      updateAppState(APP_STATES::CRASHED, true);
      string message = "eventloop_run() failed: " + string(duk_to_string(getContext(), -1));
      ERROUT(message);
      AppLog(getAppPath().c_str()) << AppLog::getTimeStamp() << " [" << Constant::String::LOG_ERROR << "] " << duk_to_string(getContext(), -1) << endl;
      duk_destroy_heap(duk_context_);
      duk_context_ = 0;
      getStaticMutex().unlock();
      getAppMutex().unlock();
      getCVMutex().unlock();
      getDuktapeMutex().unlock();
      return;
    }
    if(getContext() && rc == 0) {
      duk_pop(getContext());
      DBOUT("JSApplication::run(): Executing the terminate function");
      duk_push_string(getContext(), "app.$terminate(function(){});\n");
      int res = duk_peval(getContext());
      if(res != 0) {
        ERROUT("failed to evaluate terminate: " << duk_to_string(getContext(), -1));
        AppLog(getAppPath().c_str()) << AppLog::getTimeStamp() << " [" << Constant::String::LOG_ERROR << "] " << duk_to_string(getContext(), -1) << endl;
      }
      duk_pop(getContext());
    }
  }

  DBOUT("run(): exited eventloop");

  if(interrupted) {
    DBOUT("run(): interrupt");
    pause();
    // if runtime is terminating deleting app from the manager
    Device::getInstance().deleteApp(std::to_string(getAppId()));
  }

  setIsReady(true);
  getStaticMutex().unlock();
  getAppMutex().unlock();
  getCVMutex().unlock();
  getDuktapeMutex().unlock();
  getEventLoopCV().notify_one();

  DBOUT("run(): completed");
  return;
}

void JSApplication::updateAppState(APP_STATES state, bool update_client) {
  DBOUT("updateAppState(): setting state to " + APP_STATES_CHAR[state]);
  setAppState(state);

  if(update_client) {
    DBOUT("updateAppState(): updating RR manager");
    Device::getInstance().updateApp(to_string(getAppId()),getAppAsJSON());
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
        string key = duk_to_string(ctx, -2);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        headers.insert(pair<string,string>(key, duk_to_string(ctx, -1)));
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

duk_ret_t JSApplication::cb_register_app_api(duk_context *ctx) {
  /* Entry [ ... api_as_string ] */

  JSApplication *app = JSApplication::getApplications().at(ctx);
  {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());
    const char * rest_api = duk_require_string(ctx,0);
    app->addAppAPI(rest_api);

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

  fs::path dpath(Device::getInstance().getExecPath() + "/" + Constant::Paths::APPLICATIONS_ROOT);
  if(fs::is_directory(dpath)) {
    app_names_.clear();
    for(auto& entry : boost::make_iterator_range(fs::directory_iterator(dpath), {})) {
      fs::path temp_file = fs::path(entry);
      if(fs::is_directory(temp_file))
        app_names_.push_back(temp_file.filename().string());
    }
  }

  return app_names_;
}

void JSApplication::getJoinThreads() {
  DBOUT( "getJoinThreads()");

    for (  map<duk_context*, JSApplication*>::const_iterator it=getApplications().begin(); it!=getApplications().end(); ++it) {
      DBOUT( "getJoinThreads(): joining thread: " << it->first );
      try {
        if(it->second->getEventLoopThread().joinable())
          it->second->getEventLoopThread().join();
      } catch(const std::system_error& e) {
        ERROUT("getJoinThreads(): join failed: " << e.what());
      }
    }
}

void JSApplication::notify() {
  for(const pair<duk_context*, JSApplication*>& it : applications_ ) {
    it.second->getEventLoopCV().notify_all();
  }
}

void JSApplication::parsePackageJS() {
  std::lock_guard<recursive_mutex> duk_lock(getDuktapeMutex());
  duk_context *ctx = getContext();
  int sourceLen;

  if(!ctx) return;

  DBOUT ("parsePackageJS(): Loading package file");
  // reading the package.json file to memory
  string package_file = Device::getInstance().getExecPath() + "/" + getAppPath() + string(Constant::Paths::PACKAGE_JSON);
  char * tmp = load_js_file(package_file.c_str(),sourceLen);
  setPackageJSON(tmp);

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
    if(getAppId() < 0) {
      setAppId(to_string(getNextId(false)));
      getNextId(true);
    }
  }

  DBOUT ("parsePackageJS(): OK");
}
