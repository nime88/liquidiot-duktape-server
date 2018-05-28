/* This is the application header for the application including some convenience
stuff */

process = {};
require("./process.js");

global = {};

// interface between the application and main program
var setTaskInterval = function(repeat, interval) {
  var interval;

  if(!repeat) {
    // only execute task() once if not on repeat
    print("Executing task once...");
    task();
  } else  {
    print("Setting interval in header...");
    try {
      interval = setInterval(task, interval);
      print("Set the interval " + interval);
    } catch(e) {
      print("Setting the interval failed. " + e);
    }
  }

  return interval;
};
