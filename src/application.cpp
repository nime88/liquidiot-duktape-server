#include "application.h"

#include <algorithm>

#include "node_module_search.h"
#include "util.h"

// #define NDEBUG

#ifdef NDEBUG
    #define DBOUT( x ) cout << x  << "\n"
#else
    #define DBOUT( x )
#endif

int JSApplication::next_id_ = 0;
map<string,string> JSApplication::options_ = load_config("config.cfg");
map<duk_context*, JSApplication*> JSApplication::applications_;
map<duk_context*, thread*> JSApplication::app_threads_;
mutex *JSApplication::mtx = new mutex();

JSApplication::JSApplication(const char* path){
  // adding app_path to global app paths
  app_path_ = string(path);

  // setting id for the application
  id_ = next_id_;
  next_id_++;

  init();
}

void JSApplication::init() {
  mtx->lock();
  // setting app state to initializing
  app_state_ = APP_STATES::INITIALIZING;

  // creating new duk heap for the js application
  duk_context_ = duk_create_heap_default();
  if (!duk_context_) {
    throw "Duk context could not be created.";
  }

  DBOUT ("Inserting application");
  // adding this application to static apps
  applications_.insert(pair<duk_context*, JSApplication*>(duk_context_,this));

  DBOUT ("Duktape initializations");
  DBOUT ("Duktape initializations: print alert");
  // initializing print alerts (enables print and alert functions in js)
  duk_print_alert_init(duk_context_, 0 /*flags*/);

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

  DBOUT ("Creating new eventloop");
  // registering/creating the eventloop
  eventloop_ = new EventLoop(duk_context_);

  // loading the event loop javascript functions (setTimeout ect.)
  int evlLen;
  char* evlSource = load_js_file("./custom_eventloop.js",evlLen);
  duk_push_string(duk_context_, evlSource);
  if (duk_peval(duk_context_) != 0) {
    printf("Failed to evaluate custom_eventloop.js: %s\n", duk_safe_to_string(duk_context_, -1));
  }
  duk_pop(duk_context_);

  /** reding the source code (main.js) for the app **/

  int source_len;

  DBOUT ("JSApplication(): Loading package file");
  // reading the package.json file to memory
  string package_file = app_path_ + string("/package.json");
  package_js_ = load_js_file(package_file.c_str(),source_len);

  DBOUT ("JSApplication(): Reading package to attr");
  map<string,string> json_attr = read_json_file(duk_context_, package_js_);

  main_ = json_attr.at("main");
  string main_file = app_path_ + "/" + main_;


  DBOUT ("JSApplication(): Creating full source code");
  int source_len2;
  string temp_source = string(load_js_file("./application_header.js",source_len2)) + string(load_js_file(main_file.c_str(),source_len)) + "\ninitialize();";

  // executing initialize code
  source_code_ = new char[temp_source.length()+1];
  strcpy(source_code_,temp_source.c_str());

  DBOUT ("JSApplication(): Making it main node module");
  // pushing this as main module
  duk_push_string(duk_context_, source_code_);
  duk_module_node_peval_main(duk_context_, json_attr.at("main").c_str());

  DBOUT ("Inserting application thread");
  // initializing the thread, this must be the last initialization
  ev_thread_ = new thread(&JSApplication::run,this);
  app_threads_.insert(pair<duk_context*, thread*>(duk_context_,ev_thread_));

  mtx->unlock();
}

void JSApplication::clean() {
  mtx->lock();
    DBOUT ( "clean()" );
  // if the application is running we need to stop the thread
  app_state_ = APP_STATES::PAUSED;
  eventloop_->setRequestExit(true);
  // waiting for thread to stop the execution (come back from poll)
  DBOUT ("clean(): waiting for thread over" );
  try {
    // ev_thread_->detach(); // detaching a not-a-thread
    while (ev_thread_->joinable()) {
      mtx->unlock();
      // waiting eventloop poll to finish
      poll(NULL,NULL,eventloop_->getCurrentTimeout());
      if(ev_thread_->joinable())
        ev_thread_->join();
      mtx->lock();
    }
    // ev_thread_->detach();
  } catch (const std::system_error& e) {
    std::cout << "Caught a system_error\n";
    if(e.code() == std::errc::invalid_argument)
      std::cout << "The error condition is std::errc::invalid_argument\n";
      std::cout << "the error description is " << e.what() << '\n';
  }

  cout << "clean(): killing thread was successful" << endl;
  delete ev_thread_;
  ev_thread_ = 0;
  app_threads_.erase(duk_context_);

  delete eventloop_;
  eventloop_ = 0;

  cout << "clean(): thread waiting over and calling terminate" << endl;

  duk_push_string(duk_context_, "terminate();");
  if(duk_peval(duk_context_) != 0) {
    printf("Script error: %s\n", duk_safe_to_string(duk_context_, -1));
  }

  cout << "clean(): destoying duk heap" << endl;
  duk_destroy_heap(duk_context_);

  cout << "clean(): cleaning the app out" << duk_context_ << endl;

  applications_.erase(duk_context_);
  duk_context_ = 0;
  mtx->unlock();
}


void JSApplication::run() {
  app_state_ = APP_STATES::RUNNING;
  int rc = duk_safe_call(duk_context_, EventLoop::eventloop_run, NULL, 0 /*nargs*/, 1 /*nrets*/);
  if (rc != 0) {
    app_state_ = APP_STATES::CRASHED;
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

string JSApplication::getDescriptionAsJSON() {
  while(!getMutex()->try_lock()){}
  string full_status;
  duk_context *ctx = getContext();

  duk_push_object(ctx);

  // name
  if(name_.length() > 0) {
    duk_push_string(ctx,name_.c_str());
    duk_put_prop_string(ctx, -2, "name");
  }

  // version
  if(version_.length() > 0) {
    duk_push_string(ctx,version_.c_str());
    duk_put_prop_string(ctx, -2, "version");
  }

  // description
  if(description_.length() > 0) {
    duk_push_string(ctx,description_.c_str());
    duk_put_prop_string(ctx, -2, "description");
  }

  // main
  if(main_.length() > 0) {
    duk_push_string(ctx,main_.c_str());
    duk_put_prop_string(ctx, -2, "main");
  }

  // id
  if(id_ >= 0) {
    duk_push_int(ctx, id_);
    duk_put_prop_string(ctx, -2, "id");
  }

  // status
  if(app_state_ >= 0 && app_state_ < 4) {
    duk_push_string(ctx,APP_STATES_CHAR.at(app_state_).c_str());
    duk_put_prop_string(ctx, -2, "state");
  }

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

  // TODO if require string fails we have no way of knowing
  requested_id = duk_require_string(ctx, 0);
  parent_id = duk_require_string(ctx, 1);

  string requested_id_str = string(requested_id);

  // getting the known application/module path
  JSApplication *app = JSApplication::applications_.at(ctx);
  int app_id = app->getId();
  string path = app->getAppPath() + "/";

  // if buffer is requested just use native duktape implemented buffer
  if(requested_id_str == "buffer") {
    duk_push_string(ctx, requested_id);
    return 1;  /*nrets*/
  }

  if(requested_id_str.size() > 0) {
    // First checking core modules
    if (requested_id_str.at(0) != '.' && requested_id_str.at(0) != '/' &&
        options_.find("core_lib_path") != options_.end()) {
      string core_lib_path = options_.at("core_lib_path");
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

  cout << "Req id: " << requested_id << endl;
  cout << "Parent id: " << parent_id << endl;
  cout << "Resolved id 1: " << requested_id_str << endl;

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

        map<string,string> json_attr = read_json_file(ctx, package_js);

        // loading main file to source
        path = string(resolved_id) + json_attr.at("main");
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

bool JSApplication::applicationExists(const char* path) {
  vector<string> names = listApplicationNames();

  for(int i = 0; i < names.size(); ++i) {
    if(string(path) == names.at(i))
      return true;
  }

  return false;
}

vector<string> JSApplication::listApplicationNames() {
  vector<string> names;

  DIR *dir;
  struct dirent *ent;

  if ((dir = opendir ("applications")) != NULL) {
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
