/* This is the application header for the application including some convenience
stuff */

// defining some better error messages to locate some of the errors
Duktape.errThrow = function (e) {
    if (!(e instanceof Error)) {
        return e;  // only touch errors
    }

    if ('timestamp' in e) {
      return e;  // only touch once
    }

    e.timestamp = new Date();

    print(e);
    print(e.stack);
    print(e.timestamp);

    EventLoop.requestExit();

    return e;
}

// interface between the application and main program
var setTaskInterval = function(repeat, interval) {
  var interval;

  if(!repeat) {
    // only execute task() once if not on repeat
    print("Executing task once...");
    setTimeout(task,interval);
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
