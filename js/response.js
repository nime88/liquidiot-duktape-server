/*
 *  Response API
 */

Response = function(response) {
  response.headers = {};

  response.statusCode = 200;
  response.statusMessage = "";
  response.body = "";

  // finalizes and sends everything to be processed
  response.end = function() {
    ResolveResponse(response);
  };

  // sets a single header
  response.setHeader = function(name, value) {
    response.headers[name] = value;
  };

  response.getHeaderNames = function() {
    return Object.keys(response.headers);
  };

  response.getHeaders = function() {
    return response.headers;
  };

  response.hasHeader = function(name) {
    if (name in response.headers) return true;
    return false;
  };

  response.removeHeader = function(name) {
    if(hasHeader(name))
      delete response.headers[name];
  };

  // as dividing stuff into chunks is handled on backend just pass full data
  response.write = function(body) {
    response.body = body;
  };

};
