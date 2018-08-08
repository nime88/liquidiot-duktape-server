
// Takes object as parameter and will tome function will be
// overritten later by the actual app
Agent = function (iotApp){

    // public functions that will be overriden by applications
    iotApp.$task = function(){
        throw new Error("task function should be defined.");
    };

    iotApp.$initialize = function (initCompleted){
        initCompleted();
    };

    iotApp.$terminate = function(terminateCompleted){
        terminateCompleted();
    };

    // private properties
    var timer = true;
    var repeat = true;
    var interval = 1000;

    // public functions that will be called by applications
    iotApp.$configureInterval = function(_repeat, _interval) {
        repeat = _repeat;
        interval = _interval;
    };

    // private functions
    // interface between the application and main program
    var setTaskInterval = function(task, repeat, interval) {
      var interval_id;

      if(!repeat) {
        // only execute task() once if not on repeat
        try {
          interval_id = setTimeout(task,interval);
        } catch(e) {
          console.error("Setting the interval failed. " + e);
        }
      } else  {
        try {
          interval_id = setInterval(task, interval);
        } catch(e) {
          console.error("Setting the interval failed. " + e);
        }
      }

      return interval;
    };

    // public functions that will be called by runtime environemnt
    iotApp.start = function() {
      iotApp.$initialize(function(){});
      var wrapped = function(){iotApp.$task(function(){});};
      setTaskInterval(wrapped,repeat,interval);
    };
};
