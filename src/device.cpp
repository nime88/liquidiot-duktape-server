#include "device.h"

#include "file_ops.h"
#include "util.h"

#include <fstream>
#include <algorithm>
#include <regex>

#include <iostream>
using std::cout;
using std::endl;

using std::pair;
using std::ofstream;
using std::regex;

Device *Device::instance_ = 0;
mutex *Device::mtx_ = new mutex();

Device::Device():id_("") {
  // As device_config is read only once we don't store or retain the duktape context
  duk_context_ = duk_create_heap_default();
  const char* file_path = "device_config.json";

  // Next few checks prevent some undefined behaviour thus throwing out of here
  // if they fail
  if (!duk_context_) {
    throw "Duk context could not be created.";
  }

  if(is_file(file_path) != FILE_TYPE::PATH_TO_FILE) {
    throw "File " + string(file_path) + " doesn't exist.";
  }

  int source_len;
  char *config_source = load_js_file(file_path,source_len);

  read_raw_json(duk_context_, config_source, raw_data_);

  if(raw_data_.find("_id") != raw_data_.end()) {
    id_ = raw_data_.at("_id");
  }

  if(raw_data_.find("name") != raw_data_.end()) {
    name_ = raw_data_.at("name");
  }

  if(raw_data_.find("manufacturer") != raw_data_.end()) {
    manufacturer_ = raw_data_.at("manufacturer");
  }

  if(raw_data_.find("location") != raw_data_.end()) {
    location_ = raw_data_.at("location");
  }

  if(raw_data_.find("url") != raw_data_.end()) {
    url_ = raw_data_.at("url");
  }

  if(raw_data_.find("host") != raw_data_.end()) {
    host_ = raw_data_.at("host");
  }

  if(raw_data_.find("port") != raw_data_.end()) {
    port_ = raw_data_.at("port");
  }

  printf("Device exists \n");
  bool dexists = deviceExists();
  printf("Device exists %d\n",dexists);

  if(!dexists) {
    if(raw_data_.find("_id") != raw_data_.end()) {
      id_ = "";
      raw_data_.erase("_id");
    }

    printf("Sending Device Info\n");
    sendDeviceInfo();
  }

}

bool Device::sendDeviceInfo() {
  // making sure we have nothing to worry about
  exitClientThread();

  while(getMutex()->try_lock()) { poll(NULL,NULL,1); }
  // creating first connection to
  crconfig_ = new ClientRequestConfig();

  crconfig_->setRawPayload(getDeviceInfoAsJSON());
  crconfig_->setRequestType("POST");
  crconfig_->setRequestPath("/devices");
  crconfig_->setDeviceUrl(url_.c_str());
  crconfig_->setDeviceHost(host_.c_str());

  if(!http_client_)
    http_client_ = new HttpClient();

  printf("Just making sure\n");
  http_client_thread_ = new thread(&HttpClient::run,http_client_,crconfig_);

  if(http_client_thread_->joinable())
    http_client_thread_->join();

  getMutex()->unlock();

  if(crconfig_->getResponseStatus()  == 200) {
    setDeviceId(crconfig_->getResponse());
    return true;
  }

  return false;
}

bool Device::deviceExists() {
  exitClientThread();

  while(getMutex()->try_lock()) { poll(NULL,0,1); }

  if(!crconfig_) {
    // creating first connection to
    crconfig_ = new ClientRequestConfig();
  }

  crconfig_->setRawPayload(getDeviceInfoAsJSON());
  crconfig_->setRequestType("GET");
  string srpath = (string("/devices/id/")+id_);
  const char * rpath = srpath.c_str();
  crconfig_->setRequestPath(rpath);
  crconfig_->setDeviceUrl(url_.c_str());
  crconfig_->setDeviceHost(host_.c_str());

  if(!http_client_) {
    http_client_ = new HttpClient();
  }

  http_client_thread_ = new thread(&HttpClient::run,http_client_,crconfig_);

  if(http_client_thread_->joinable())
    http_client_thread_->join();

  getMutex()->unlock();

  // cleaning the response out of spaces
  string resp = crconfig_->getResponse();
  resp.erase(std::remove(resp.begin(),
    resp.end(), ' '), resp.end());

  return resp != "null" && crconfig_->getResponseStatus() == 200;
}

bool Device::registerAppApi(string class_name, string swagger_fragment) {
  exitClientThread();

  while(getMutex()->try_lock()) { poll(NULL,NULL,1); }

  if(!crconfig_) {
    // creating first connection to
    crconfig_ = new ClientRequestConfig();
  }

  crconfig_->setRawPayload(swagger_fragment);
  crconfig_->setRequestType("PUT");
  string srpath = (string("/apis/")+ class_name);
  const char * rpath = srpath.c_str();
  crconfig_->setRequestPath(rpath);
  crconfig_->setDeviceUrl(url_.c_str());
  crconfig_->setDeviceHost(host_.c_str());

  if(!http_client_) {
    http_client_ = new HttpClient();
  }

  http_client_thread_ = new thread(&HttpClient::run,http_client_,crconfig_);

  if(http_client_thread_->joinable())
    http_client_thread_->join();

  getMutex()->unlock();

  return crconfig_->getResponseStatus() == 200;
}

bool Device::registerApp(string app_payload) {
  if(id_.length() == 0) return false;

  exitClientThread();

  while(getMutex()->try_lock()) { poll(NULL,NULL,1); }

  if(!crconfig_) {
    // creating first connection to
    crconfig_ = new ClientRequestConfig();
  }

  crconfig_->setRawPayload(app_payload);
  crconfig_->setRequestType("POST");
  string srpath = (string("/devices/")+ id_ + "/apps");
  const char * rpath = srpath.c_str();
  crconfig_->setRequestPath(rpath);
  crconfig_->setDeviceUrl(url_.c_str());
  crconfig_->setDeviceHost(host_.c_str());

  if(!http_client_) {
    http_client_ = new HttpClient();
  }

  http_client_thread_ = new thread(&HttpClient::run,http_client_,crconfig_);

  if(http_client_thread_->joinable())
    http_client_thread_->join();

  getMutex()->unlock();

  return crconfig_->getResponseStatus() == 200;
}

bool Device::updateApp(string app_id, string app_payload) {
  if(id_.length() == 0) return false;

  exitClientThread();

  while(getMutex()->try_lock()) { poll(NULL,NULL,1); }

  if(!crconfig_) {
    // creating first connection to
    crconfig_ = new ClientRequestConfig();
  }

  crconfig_->setRawPayload(app_payload);
  crconfig_->setRequestType("PUT");
  string srpath = (string("/devices/")+ id_ + "/apps/" + app_id);
  const char * rpath = srpath.c_str();
  crconfig_->setRequestPath(rpath);
  crconfig_->setDeviceUrl(url_.c_str());
  crconfig_->setDeviceHost(host_.c_str());

  if(!http_client_) {
    http_client_ = new HttpClient();
  }

  http_client_thread_ = new thread(&HttpClient::run,http_client_,crconfig_);

  if(http_client_thread_->joinable())
    http_client_thread_->join();

  getMutex()->unlock();

  return crconfig_->getResponseStatus() == 200;
}

void Device::saveSettings() {
  string settings = getDeviceInfoAsJSON();

  ofstream dev_file;
  dev_file.open ("device_config.json");
  dev_file << settings;
  dev_file.close();
}

string Device::getDeviceInfoAsJSON() {
  duk_context *ctx = getContext();

  duk_push_object(ctx);

  for ( map<string,string>::iterator it = raw_data_.begin(); it != raw_data_.end(); ++it) {
    duk_push_string(ctx,it->second.c_str());
    duk_put_prop_string(ctx, -2, it->first.c_str());
  }

  string full_info = duk_json_encode(ctx, -1);

  duk_pop(ctx);

  return full_info;
}

void Device::setDeviceId(string id) {
  // cleaning id
  std::smatch m;
  std::regex e( R"(^([A-Za-z0-9]+))" );

  string temp_id = id;
  while (std::regex_search (temp_id,m,e, std::regex_constants::match_any)) {
    id = m[1];
    temp_id = m.suffix().str();
  }

  while(getMutex()->try_lock()) { poll(NULL,NULL,1); }
  id_ = id;

  if(raw_data_.find("_id") != raw_data_.end())
    raw_data_.at("_id") = id_;
  else
    raw_data_.insert(pair<string,string>("_id",id_));

  saveSettings();
  getMutex()->unlock();
}

void Device::exitClientThread() {
  while(getMutex()->try_lock()) { poll(NULL,NULL,1); }
  if(http_client_thread_) {
    if(http_client_thread_->joinable())
      http_client_thread_->join();
    delete http_client_thread_;
    http_client_thread_ = 0;
  }
  getMutex()->unlock();
}
