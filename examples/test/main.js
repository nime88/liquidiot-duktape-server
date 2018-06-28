module.exports = function(testi){

  var repeat = true;
  var interval = 3000;
  var event_counter = 0;

  testi.internal.configureInterval(repeat, interval);

  testi.internal.initialize = function(initCompleted){
    console.log("Initialize");
    initCompleted();
  };

  testi.internal.task = function(taskCompleted) {
    event_counter = event_counter + 1;
    console.log("Doing the task " + event_counter);
    taskCompleted();
  };

  testi.internal.terminate = function(terminateCompleted){
    console.log("Terminating");
    terminateCompleted();
  };
  
  // creating new api (has been defined in liquidiot.json so runtime will create/evaluate it)
  testi.external.testApi = function(request,response) {
      
      response.setHeader("Content-type", "text/html");
      response.statusCode = 200;
      
      response.write("Event counter: " + event_counter + "<br>" + 
        "Mr gdfg: " + request.params.gdfg + "<br>" + "What?: " + request.params.wut
      );
      response.end();
      
  };
  
  // swagger fragment is only used to register external apis to device manager
  // thus having no actual functionality in the runtime
  testi.external.swagger = {
    "swagger": "2.0",
    "info": {
        "description": "Application will count stuff",
        "version": "1.0.0",
        "title": "Test"
    },
    "x-device-capability": "",
    "paths": {
        "/testApi": {
            "post": {
                "tags": [],
                "description": "Shows counter",
                "produces": [
                    "text/html"
                ],
                "parameters": ["gdfg","wut"],
                "responses": {
                    "200": {
                        "description": "status message"
                    }
                }
            }
        }
    }
  }
}
