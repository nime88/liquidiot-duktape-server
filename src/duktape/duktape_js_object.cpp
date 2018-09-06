#include "duktape_js_object.h"


const char* duk_js::Boolean::VALUES[] = {"true","false"};

bool duk_js::operator==(const bool a, const duk_js::Boolean& b) {
    return a == b.val();
}

bool duk_js::operator!=(const bool a, const duk_js::Boolean& b){
    return a != b.val();
}

void duk_js::Object::assign(const Object& val) {
  data_changed_ = val.data_changed_;
  str_value_ = val.str_value_;
  error_ = val.error_;

  clear_values();

  for (const auto& value : val.values_) {
    switch (value.second->getType()) {
      case TYPE::None: {
        duk_js::None *temp = new duk_js::None(*dynamic_cast<duk_js::None*>(value.second));
        values_.push_back(std::pair<std::string,JSVariant*>(value.first, temp));
        continue;
      }
      case TYPE::Undefined: {
        duk_js::Undefined *temp = new duk_js::Undefined(*dynamic_cast<duk_js::Undefined*>(value.second));
        values_.push_back(std::pair<std::string,JSVariant*>(value.first, temp));
        continue;
      }
      case TYPE::Null: {
        duk_js::Null *temp = new duk_js::Null(*dynamic_cast<duk_js::Null*>(value.second));
        values_.push_back(std::pair<std::string,JSVariant*>(value.first, temp));
        continue;
      }
      case TYPE::Boolean: {
        duk_js::Boolean *temp = new duk_js::Boolean(*dynamic_cast<duk_js::Boolean*>(value.second));
        values_.push_back(std::pair<std::string,JSVariant*>(value.first, temp));
        continue;
      }
      case TYPE::Number: {
        duk_js::Number *temp = new duk_js::Number(*dynamic_cast<duk_js::Number*>(value.second));
        values_.push_back(std::pair<std::string,JSVariant*>(value.first, temp));
        continue;
      }
      case TYPE::String: {
        duk_js::String *temp = new duk_js::String(*dynamic_cast<duk_js::String*>(value.second));
        values_.push_back(std::pair<std::string,JSVariant*>(value.first, temp));
        continue;
      }
      case TYPE::Object: {
        duk_js::Object *temp = new duk_js::Object(*dynamic_cast<duk_js::Object*>(value.second));
        values_.push_back(std::pair<std::string,JSVariant*>(value.first, temp));
        continue;
      }
    }
  }
}
