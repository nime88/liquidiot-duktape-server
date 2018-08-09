/*
 *  Router interface for IDE
 */

 Router = function(router) {
   var request = {};
   var response = {};

   var call_table = {};

   Request(request);
   Response(response);

   router.get = function(application_interface_path, req_resp_func) {
     RegisterAIPath(application_interface_path);
     call_table[application_interface_path] = req_resp_func;
   };

   router.post = function(application_interface_path, req_resp_func) {
     RegisterAIPath(application_interface_path);
     call_table[application_interface_path] = req_resp_func;
   };

   router.put = function(application_interface_path, req_resp_func) {
     RegisterAIPath(application_interface_path);
     call_table[application_interface_path] = req_resp_func;
   };

   router.delete = function(application_interface_path, req_resp_func) {
     RegisterAIPath(application_interface_path);
     call_table[application_interface_path] = req_resp_func;
   };

   router.call_api = function(application_interface_path) {
     call_table[application_interface_path](request,response);
   }


 }
