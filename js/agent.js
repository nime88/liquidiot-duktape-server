
// Takes object as parameter and will tome function will be
// overritten later by the actual app
Agent = function (iotApp){

    // public functions that will be overriden by applications
    iotApp.task = function(){
        throw new Error("task function should be defined.");
    };

    iotApp.initialize = function (initCompleted){
        initCompleted();
    };

    iotApp.terminate = function(terminateCompleted){
        terminateCompleted();
    };

    // private properties
    var timer = true;
    var repeat = true;
    var interval = 1000;

    // public functions that will be called by applications
    iotApp.configureInterval = function(_repeat, _interval) {
        repeat = _repeat;
        interval = _interval;
    };

    // private functions
    // interface between the application and main program
    var setTaskInterval = function(task, repeat, interval) {
      var interval_id;

      if(!repeat) {
        // only execute task() once if not on repeat
        console.log("Executing task once...");
        interval_id = setTimeout(task,interval);
      } else  {
        print("Setting interval in header...");
        try {
          interval_id = setInterval(task, interval);
          console.log("Set the interval " + interval_id);
        } catch(e) {
          console.log("Setting the interval failed. " + e);
        }
      }

      return interval;
    };

    // public functions that will be called by runtime environemnt
    iotApp.start = function() {
      iotApp.initialize(function(){});
      var wrapped = function(){iotApp.task(function(){});};
      setTaskInterval(wrapped,repeat,interval);
    };
};
