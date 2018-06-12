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

    print(e.stack);

    EventLoop.requestExit();

    return e;
}

// interface between the application and main program
setTaskInterval = function(repeat, interval) {
  var interval_id;

  if(!repeat) {
    // only execute task() once if not on repeat
    print("Executing task once...");
    interval_id = setTimeout(task,interval);
  } else  {
    print("Setting interval in header...");
    try {
      interval_id = setInterval(task, interval);
      print("Set the interval " + interval_id);
    } catch(e) {
      print("Setting the interval failed. " + e);
    }
  }

  return interval;
};
