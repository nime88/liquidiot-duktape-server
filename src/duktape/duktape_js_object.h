#ifndef DUKTAPE_JS_OBJECT_H
#define DUKTAPE_JS_OBJECT_H

#include "duktape.h"

#include <string>
#include <sstream>
#include <vector>

namespace duk_js {

class JSVariant {
 public:
   JSVariant():context_(0),value_("") {}
   virtual ~JSVariant() {}
   inline void setContext(duk_context *ctx) { context_ = ctx; }
   inline duk_context* getContext() { return context_; }
   enum TYPE { None, Undefined, Null, Boolean, Number, String, Object };

   virtual const std::string& toString() { return value_; }
   virtual const JSVariant& at(const std::string&) { return *this; };
   inline const TYPE& getType() const { return type_; }
   inline const std::string& getValue() const { return value_; }

 protected:
   inline void setType(const TYPE &type) { type_ = type; }

 private:
   duk_context *context_;
   TYPE type_;
   std::string value_;
};

template<typename T>
static bool operator==(const duk_js::JSVariant& a, const T& b) {
    return a.getType() == b.getType() && a.getValue() == b.getValue();
}
template<typename T>
static bool operator!=(const duk_js::JSVariant& a, const T& b) {
    return a.getType() != b.getType() || a.getValue() != b.getValue();
}

class None : public JSVariant {
  public:
    None():value_("None") {
      setType(TYPE::None);
    }
    ~None() {}

    inline const std::string& toString() const {
      return value_;
    }

    inline const JSVariant& at(const std::string&) const {
      return *this;
    }

    inline const std::string& val() const {
      return value_;
    }

    inline bool operator==(const None& rhs) const {
      return (getType() == rhs.getType()) && (getValue() == rhs.getValue());
    }

    inline bool operator==(const JSVariant& rhs) const {
      return (this->getType() == rhs.getType()) && (this->getValue() == rhs.getValue());
    }

  private:
    std::string value_;
};

class Undefined : public JSVariant {
  public:
    Undefined():value_("undefined") {
      setType(TYPE::Undefined);
    }
    ~Undefined() {}

    inline const std::string& toString() {
      return value_;
    }

    inline const JSVariant& at(const std::string&) {
      return *this;
    }

    inline const std::string& val() const {
      return value_;
    }

    inline bool operator==(const Undefined& rhs) const {
      return (getType() == rhs.getType()) && (getValue() == rhs.getValue());
    }

    inline bool operator==(const JSVariant& rhs) const {
      return (this->getType() == rhs.getType()) && (this->getValue() == rhs.getValue());
    }

  private:
    std::string value_;

};

class Null : public JSVariant {
  public:
    Null():value_("Null") {
      setType(TYPE::Null);
    }
    ~Null() {}

    inline const std::string& toString() const {
      return value_;
    }

    inline const JSVariant& at(const std::string&) const {
      return *this;
    }

    inline void* val() const {
      return NULL;
    }

    inline bool operator==(const Null& rhs) const {
      return (getType() == rhs.getType()) && (getValue() == rhs.getValue());
    }

    inline bool operator==(const JSVariant& rhs) const {
      return (this->getType() == rhs.getType()) && (this->getValue() == rhs.getValue());
    }

  private:
    std::string value_;

};

class Boolean : public JSVariant {
  private:
    static const char* VALUES[];
  public:
    Boolean():str_value_(VALUES[1]), value_(false) {
      setType(TYPE::Boolean);
    }
    ~Boolean() {}

    inline const std::string& toString() const {
      return str_value_;
    }

    inline const JSVariant& at(const std::string&) const {
      return *this;
    }

    inline bool val() const {
      return value_;
    }

    template <typename T1>
    inline Boolean& operator= (const T1& val) {
      setValue(val);
      return *this;
    }

    inline Boolean& operator= (const Boolean& val) {
      setValue(val.val());
      return *this;
    }

    template <typename T2>
    inline Boolean(const T2& val) {
      setType(TYPE::Boolean);
      setValue(val);
    }

    inline Boolean(const Boolean& val) {
      setType(TYPE::Boolean);
      setValue(val.val());
    }

    inline bool operator==(const Boolean& rhs) const {
      return (getType() == rhs.getType()) && (val() == rhs.val());
    }

    inline bool operator==(const JSVariant& rhs) const {
      return (getType() == rhs.getType()) && (getValue() == rhs.getValue());
    }

    inline bool operator!=(const Boolean& rhs) const {
      return val() != rhs.val();
    }

    inline bool operator!=(const JSVariant& rhs) const {
      return (getType() != rhs.getType()) || (getValue() != rhs.getValue());
    }

    template<typename T3>
    inline bool operator==(const T3& rhs) const {
      return val() == rhs;
    }

  private:
    std::string str_value_;
    bool value_;

    inline void setValue(bool val) {
      value_ = val;
      str_value_ = value_ ? VALUES[0] : VALUES[1];
    }
};

extern bool operator==(const bool a, const duk_js::Boolean& b);
extern bool operator!=(const bool a, const duk_js::Boolean& b);

class Number : public JSVariant {
  public:
    Number():str_value_("0"), value_(0) {
      setType(TYPE::Number);
    }
    ~Number() {}

    inline const std::string& toString() const {
      return str_value_;
    }

    inline const JSVariant& at(const std::string&) const {
      return *this;
    }

    inline double val() const {
      return value_;
    }

    template <typename T1>
    inline Number& operator= (const T1& val) {
      setValue(val);
      return *this;
    }

    inline Number& operator= (const Number& val) {
      setValue(val.val());
      return *this;
    }

    template <typename T2>
    inline Number(const T2& val) {
      setType(TYPE::Number);
      setValue(val);
    }

    inline Number(const Number& val) {
      setType(TYPE::Number);
      setValue(val.val());
    }

  private:
    std::string str_value_;
    double value_;

    inline void setValue(double val) {
      value_ = val;
      str_value_ = std::to_string(value_);
    }

};

class String : public JSVariant {
  public:
    String():value_("") {
      setType(TYPE::String);
    }
    ~String() {}

    inline const std::string& toString() const {
      return value_;
    }

    inline const JSVariant& at(const std::string&) const {
      return *this;
    }

    inline const std::string& val() const {
      return value_;
    }

    template <typename T1>
    inline String& operator= (const T1& val) {
      setValue(std::to_string(val));
      return *this;
    }

    inline String& operator= (const String& val) {
      setValue(val.val());
      return *this;
    }

    template <typename T2>
    inline String(const T2& val) {
      setType(TYPE::String);
      setValue(std::to_string(val));
    }

    inline String(const String& val) {
      setType(TYPE::String);
      setValue(val.val());
    }

  private:
    std::string value_;

    inline void setValue(const std::string val) {
      value_ = val;
    }

};

class Object : JSVariant {
  public:
    Object():data_changed_(true),str_value_("") {
      setType(TYPE::Object);
    }

    ~Object() {
      clear_values();
    }

    inline const std::string& toString() {
      if(data_changed_) {
        // TODO
        std::ostringstream oss;
        oss << "{\n";
        str_value_ = oss.str();
        data_changed_ = false;
      }

      return str_value_;
    }

    inline const JSVariant& at(const std::string& key) {
      for( const auto& value : values_ ) {
        if(value.first == key) return *value.second;
      }

      return error_;
    }

    inline Object& operator= (const Object& val) {
      assign(val);
      return *this;
    }

  private:
    bool data_changed_;
    std::string str_value_;
    std::vector<std::pair<std::string, JSVariant*> > values_;
    duk_js::Undefined error_;

    void assign(const Object& val);

    inline void clear_values() {
      for( auto& value : values_ ) {
        switch (value.second->getType()) {
          case TYPE::None: {
            delete dynamic_cast<duk_js::None*>(value.second);
            continue;
          }
          case TYPE::Undefined: {
            delete dynamic_cast<duk_js::Undefined*>(value.second);
            continue;
          }
          case TYPE::Null: {
            delete dynamic_cast<duk_js::Null*>(value.second);
            continue;
          }
          case TYPE::Boolean: {
            delete dynamic_cast<duk_js::Boolean*>(value.second);
            continue;
          }
          case TYPE::Number: {
            delete dynamic_cast<duk_js::Number*>(value.second);
            continue;
          }
          case TYPE::String: {
            delete dynamic_cast<duk_js::String*>(value.second);
            continue;
          }
          case TYPE::Object: {
            delete dynamic_cast<duk_js::Object*>(value.second);
            continue;
          }
        }
      }

      values_.clear();
    }

};

}

#endif // DUKTAPE_JS_OBJECT_H
