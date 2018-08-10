module.exports = function($app, $router, $request, console){
var max_reserve = 1e7;
var arr = [];

$app.$configureInterval(true, 3000);

$app.$initialize = function(initCompleted){
    for( var i = 0; i < max_reserve; ++i) {
        arr.push("Some string");
    }
    initCompleted();
};

$app.$task = function(taskCompleted) {
    if(typeof process != 'undefined')
        console.log(JSON.stringify(process.memoryUsage()));
    taskCompleted();
};

$app.$terminate = function(terminateCompleted){
    terminateCompleted();
};
}