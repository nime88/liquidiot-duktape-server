#include <catch2/catch.hpp>

#include <string>

#include "duktape_js_object.h"

TEST_CASE( "DUK_JS None type tests.", "[js object tests]" ) {
  duk_js::None none;
  CHECK( none.toString() == "None" );
  CHECK( none.at("anythjing") == none );
  CHECK( none.toString() == none.val() );
  CHECK( none == none.at("anythjing"));
  CHECK( none.at("anythjing").getType() == duk_js::JSVariant::TYPE::None);
}

TEST_CASE( "DUK_JS Undefined type tests.", "[js object tests]" ) {
  duk_js::Undefined undefined;
  CHECK( undefined.toString() == "undefined" );
  CHECK( undefined.at("anythjing") == undefined );
  CHECK( undefined.toString() == undefined.val() );
  CHECK( undefined == undefined.at("anythjing") );
  CHECK( undefined.at("anythjing").getType() == duk_js::JSVariant::TYPE::Undefined );
}

TEST_CASE("DUK_JS Null type tests.", "[js object tests]") {
  duk_js::Null null;

  CHECK( null.toString() == "Null" );
  CHECK( null.at("anythjing") == null );
  CHECK( NULL == null.val() );
  CHECK( null == null.at("anythjing") );
  CHECK( null.at("anythjing").getType() == duk_js::JSVariant::TYPE::Null );
}

TEST_CASE("DUK_JS Boolean type tests", "[js object tests]") {
  duk_js::Boolean boolean;
  boolean = true;
  CHECK( boolean == true );
  CHECK( true == boolean );
  CHECK( boolean.toString() == "true");
  CHECK( boolean.val() == true );
  boolean = false;
  CHECK( boolean == false );
  CHECK( false == boolean );
  CHECK( boolean.toString() == "false");
  duk_js::Boolean boolean_2 = true;
  CHECK(boolean != boolean_2);
  boolean = true;
  CHECK(boolean == boolean_2);

  CHECK( boolean.at("anythjing") == boolean );
  CHECK( boolean == boolean.at("anythjing") );
  CHECK( boolean.at("anythjing").getType() == duk_js::JSVariant::TYPE::Boolean );
}
