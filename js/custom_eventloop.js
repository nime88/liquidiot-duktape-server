/*
 *  C eventloop example (c_eventloop.c).
 *
 *  ECMAScript code to initialize the exposed API (setTimeout() etc) when
 *  using the C eventloop.
 *
 *  https://developer.mozilla.org/en-US/docs/Web/JavaScript/Timers
 */

/*
 *  Timer API
 */

function setTimeout(func, delay) {
    var cb_func;
    var bind_args;
    var timer_id;

    // Delay can be optional at least in some contexts, so tolerate that.
    // https://developer.mozilla.org/en-US/docs/Web/API/WindowOrWorkerGlobalScope/setTimeout
    if (typeof delay !== 'number') {
        if (typeof delay === 'undefined') {
            delay = 0;
        } else {
            throw new TypeError('invalid delay');
        }
    }

    if (typeof func === 'string') {
        // Legacy case: callback is a string.
        cb_func = eval.bind(this, func);
    } else if (typeof func !== 'function') {
        throw new TypeError('callback is not a function/string');
    } else if (arguments.length > 2) {
        // Special case: callback arguments are provided.
        bind_args = Array.prototype.slice.call(arguments, 2);  // [ arg1, arg2, ... ]
        bind_args.unshift(this);  // [ global(this), arg1, arg2, ... ]
        cb_func = func.bind.apply(func, bind_args);
    } else {
        // Normal case: callback given as a function without arguments.
        cb_func = func;
    }

    timer_id = EventLoop.createTimer(cb_func, delay, true /* oneshot */);
    return timer_id;
}

function clearTimeout(timer_id) {
    if (typeof timer_id !== 'number') {
        throw new TypeError('timer ID is not a number');
    }
    var success = EventLoop.deleteTimer(timer_id);  /* retval ignored */
}

function setInterval(func, delay) {
    var cb_func;
    var bind_args;
    var timer_id;

    if (typeof delay !== 'number') {
        if (typeof delay === 'undefined') {
            delay = 0;
        } else {
            throw new TypeError('invalid delay');
        }
    }

    if (typeof func === 'string') {
        // Legacy case: callback is a string.
        cb_func = eval.bind(this, func);
    } else if (typeof func !== 'function') {
        throw new TypeError('callback is not a function/string');
    } else if (arguments.length > 2) {
        // Special case: callback arguments are provided.
        bind_args = Array.prototype.slice.call(arguments, 2);  // [ arg1, arg2, ... ]
        bind_args.unshift(this);  // [ global(this), arg1, arg2, ... ]
        cb_func = func.bind.apply(func, bind_args);
    } else {
        // Normal case: callback given as a function without arguments.
        cb_func = func;
    }

    timer_id = EventLoop.createTimer(cb_func, delay, false /* repeats */);

    return timer_id;
}

function clearInterval(timer_id) {
    if (typeof timer_id !== 'number') {
        throw new TypeError('timer ID is not a number');
    }
    EventLoop.deleteTimer(timer_id);
}

function requestEventLoopExit() {
    EventLoop.requestExit();
}
