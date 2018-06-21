#include "device.h"

#include "file_ops.h"
#include "util.h"

using std::pair;

Device *Device::instance_ = 0;

Device* Device::getInstance() {
  if(!instance_) {
    instance_ = new Device();
  }

  return instance_;
}

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

  printf("Hello world!\n");
  if(id_.length() == 0) {
    sendDeviceInfo();
    http_client_thread_->join();

  }
}

void Device::sendDeviceInfo() {
  // creating first connection to
  crconfig_ = new ClientRequestConfig();

  // TODO next is to get this JSON to the device manager
  printf("Ya man: \n%s\n",getDeviceInfoAsJSON().c_str());

  if(!http_client_)
    http_client_ = new HttpClient();

  http_client_thread_ = new thread(&HttpClient::run,http_client_,crconfig_);
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
