/*
 *  Request API
 */

Request = function(request) {
  request.request_type = "GET";

  request.headers = {};
  request.protocol = "";
  //request.path = "";
  // raw body
  // request.data = "";
  // parsed url parameters
  request.params = {};

  // tries to fetch body parameter
  request.getParam = function(name) {
    return request.params[name];
  }

  // returns the value of the header asked for
  request.getHeader = function(name) {
    return headers[name];
  }
}
