module.exports = function($app, $router, $request, console){
var repeat = true;
var interval = 3000;
var counter = 0;

$app.$configureInterval(repeat, interval);

$app.$initialize = function(initCompleted){
    counter = 0;
    console.log("Counter initialized at " + counter);
    initCompleted();
};

$app.$task = function(taskCompleted) {
    counter = counter + 1;
    console.log("Counter is at " + counter);
    taskCompleted();
};

$app.$terminate = function(terminateCompleted){
    console.log("Counting finished");
    terminateCompleted();
};
/**
 * Application Interface: normal_app_api
 */
$router.get("/counter", function(req, res){
    res.setHeader("Content-type", "application/json");
    res.statusCode = 200;
    
    var resp_obj = { "counter" : counter };
    res.write(JSON.stringify(resp_obj));
    res.end();
});
// normal_app_api - end

}