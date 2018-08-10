module.exports = function($app, $router, $request, console){
var time_0;
var time_1;
var fin_time = 0;

$app.$configureInterval(true, 10000);

$app.$initialize = function(initCompleted){
    initCompleted();
};

$app.$task = function(taskCompleted) {
     if(typeof performance != 'undefined')
        time_0 = performance.now();
    else
        time_0 = process.hrtime()
    console.log("Task started");
    var arr = [];
    for(var i = 0; i < 10; ++i) {
        arr.push(i);
    }
    
    for(i = 0; i < 1e5; ++i) {
        arr.reverse();
    }
    console.log("Task ended");
        
    if(typeof performance != 'undefined') {
        time_1 = performance.now();
        fin_time = time_1 - time_0;
    } else {
        time_1 = process.hrtime();
        fin_time = (time_1[1] - time_0[1]) / 1000;
    }
    console.log("Execution took " + fin_time + " milliseconds.");
    taskCompleted();
};

$app.$terminate = function(terminateCompleted){
    terminateCompleted();
};
}