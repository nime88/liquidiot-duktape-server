module.exports = function($app, $router, $request, console){
var max_reserve = 1e6;
var counter = 0;
var arr = [];

var memory_measurement_values = {
    "max_rss" : 0,
    "avg_rss" : 0,
    "max_heap_total" : 0,
    "avg_heap_total" : 0,
    "max_heap_used" : 0,
    "avg_heap_used" : 0,
    "max_external" : 0,
    "avg_external" : 0
};

$app.$configureInterval(true, 10000);

$app.$initialize = function(initCompleted){
    initCompleted();
};

$app.$task = function(taskCompleted) {
    if(arr.length === 0) {
        for(var i = 0; i < max_reserve; ++i) {
            arr.push(10);
        }
    }
    if(typeof process != 'undefined') {
        counter = counter + 1; 
        var current_mem = process.memoryUsage();
        
        if(memory_measurement_values.max_rss < current_mem.rss) {
            memory_measurement_values.max_rss = current_mem.rss;
        }
        
        if(memory_measurement_values.avg_rss === 0) {
            memory_measurement_values.avg_rss = current_mem.rss;
        } else {
            memory_measurement_values.avg_rss = memory_measurement_values.avg_rss + 
            (1 / (counter + 1)) * (current_mem.rss - memory_measurement_values.avg_rss);
        }
        
        if(memory_measurement_values.max_heap_total < current_mem.heapTotal) {
            memory_measurement_values.max_heap_total = current_mem.heapTotal;
        }
        
        if(memory_measurement_values.avg_heap_total === 0) {
            memory_measurement_values.avg_heap_total = current_mem.heapTotal;
        } else {
            memory_measurement_values.avg_heap_total = memory_measurement_values.avg_heap_total + 
            (1 / (counter + 1)) * (current_mem.heapTotal - memory_measurement_values.avg_heap_total);
        }
        
        if(memory_measurement_values.max_heap_used < current_mem.heapUsed) {
            memory_measurement_values.max_heap_used = current_mem.heapUsed;
        }
        
        if(memory_measurement_values.avg_heap_used === 0) {
            memory_measurement_values.avg_heap_used = current_mem.heapUsed;
        } else {
            memory_measurement_values.avg_heap_used = memory_measurement_values.avg_heap_used + 
            (1 / (counter + 1)) * (current_mem.heapUsed - memory_measurement_values.avg_heap_used);
        }
        
        if(memory_measurement_values.max_external < current_mem.external) {
            memory_measurement_values.max_external = current_mem.external;
        }
        
        if(memory_measurement_values.avg_external === 0) {
            memory_measurement_values.avg_external = current_mem.external;
        } else {
            memory_measurement_values.avg_external = memory_measurement_values.avg_external + 
            (1 / (counter + 1)) * (current_mem.external - memory_measurement_values.avg_external);
        }
        
        console.log("Iterations " +  counter);
        console.log("Max rss " + Math.round(memory_measurement_values.max_rss / 1024));
        console.log("Avg rss " + Math.round(memory_measurement_values.avg_rss / 1024));
        console.log("Max heapTotal " + Math.round(memory_measurement_values.max_heap_total / 1024));
        console.log("Avg heapTotal " + Math.round(memory_measurement_values.avg_heap_total / 1024));
        console.log("Max heapUsed " + Math.round(memory_measurement_values.max_heap_used / 1024));
        console.log("Avg heapUsed " + Math.round(memory_measurement_values.avg_heap_used / 1024));
        console.log("Max external " + Math.round(memory_measurement_values.max_external / 1024));
        console.log("Avg external " + Math.round(memory_measurement_values.avg_external / 1024));
    }
    taskCompleted();
};

$app.$terminate = function(terminateCompleted){
    terminateCompleted();
};


}
