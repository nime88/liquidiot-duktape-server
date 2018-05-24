/* This is the application header for the application including some convenience
stuff */

// interface between the application and main program
var setTaskInterval = function(repeat, interval) {
  if(!repeat) {
    task();
  } else  {
    print("Setting interval in header...");
    setInterval(task, interval);
  }
};
