#ifndef CLIENT_REQUEST_CONFIG_H_INCLUDED
#define CLIENT_REQUEST_CONFIG_H_INCLUDED

#include <string>

using std::string;

class ClientRequestConfig {
  public:
    ClientRequestConfig() {}

    // this will be the response we're getting from the client
    void setResponse(string response) { response_ = response; }
    string getResponse() { return response_; }

    void setResponseStatus(int status) { response_status_ = status; }
    int getResponseStatus() { return response_status_; }

    void setRawPayload(string raw_data) { raw_payload_ = raw_data; }
    string getRawPayload() { return raw_payload_; }

    void setRawReturnData(string raw_data) { raw_return_data_ = raw_data; }
    string getRawReturnData() { return raw_return_data_; }

    void setRequestType(string request_type) { request_type_ = request_type; }
    string getRequestType() { return request_type_; }

    void setRequestPath(const char * path) { request_path_ = path; }
    const char * getRequestPath() { return request_path_; }

    void setDeviceUrl(const char * url) { device_url_ = url; }
    const char * getDeviceUrl() { return device_url_; }

    void setDeviceHost(const char * host) { device_host_ = host; }
    const char * getDeviceHost() { return device_host_; }

    void setDevicePort(const char * port) { device_port_ = port; }
    const char * getDevicePort() { return device_port_; }


  private:
    string raw_payload_;
    string raw_return_data_;
    string request_type_;
    const char * request_path_;
    const char * device_url_;

    const char * device_host_;
    const char * device_port_;

    string response_;
    int response_status_;
};

#endif
