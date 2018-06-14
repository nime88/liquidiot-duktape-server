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

    console.err(e.stack);

    EventLoop.requestExit();

    return e;
}
