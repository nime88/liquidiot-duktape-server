module.exports = function(testi2){
  var event_counter = 0;
  var repeat = true;
  var interval = 6000;

  testi2.internal.configureInterval(repeat, interval);

  testi2.internal.initialize = function(initCompleted){
    console.log("Initialize");
    initCompleted();
  };

  testi2.internal.task = function(taskCompleted) {
    event_counter = event_counter + 1
    console.log("Test2 application doing the task " + event_counter);
    taskCompleted();
  };

  testi2.internal.terminate = function(terminateCompleted){
    console.log("Terminating");
    terminateCompleted();
  };
}
