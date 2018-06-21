#ifndef CLIENT_REQUEST_CONFIG_H_INCLUDED
#define CLIENT_REQUEST_CONFIG_H_INCLUDED

#include <string>

using std::string;

class ClientRequestConfig {
  public:
    ClientRequestConfig() {}

    void setRawPayload(string raw_data) { raw_payload_ = raw_data; }
    string getRawPayload() { return raw_payload_; }

    void setRawReturnData(string raw_data) { raw_return_data_ = raw_data; }
    string getRawReturnData() { return raw_return_data_; }

  private:
    string raw_payload_;
    string raw_return_data_;
};

#endif
