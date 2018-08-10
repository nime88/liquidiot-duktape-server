module.exports = function Agent(iotApp, emitter){
    
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
    }
    
    // private functions
    var createInterval = function(f, param, interval) {
        setTimeout( function() {f(param);}, interval );
    }
    
    var s = function() {
        createInterval(iotApp.$task, function(restartMainMessage){
            if(restartMainMessage){
                console.log(restartMainMessage);
            }
            if(timer) {
                s();
            } else {
                iotApp.$terminate(function(stopExecutionMessage){
                    if(stopExecutionMessage){
                        console.log(stopExecutionMessage);
                    }
                    emitter.emit('paused');
                });
            }
        }, interval);
    };
    
    // public functions that will be called by runtime environemnt
    iotApp.start = function() {
        iotApp.$initialize(function(startMainMessage){
            if(startMainMessage){
                console.log(startMainMessage);
            }
            timer = true;
            iotApp.$task(function(restartMainMessage){
                if(restartMainMessage){
                    console.log(restartMainMessage);
                }
                console.log("app-started-without-error");
                if(emitter){
                  emitter.emit('started');
                }
                if(repeat) {
                    s();
                }
            });
        });
    };
    
    iotApp.stop = function() {
        timer = false;
        if(!repeat) {
            iotApp.$terminate(function(stopExecutionMessage){
                if(stopExecutionMessage){
                    console.log(stopExecutionMessage);
                }
                emitter.emit('paused');
            });
        }
    };
}