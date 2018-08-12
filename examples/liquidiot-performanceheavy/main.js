module.exports = function($app, $router, $request, console){
var time_0;
var time_1;
var fin_time = 0;

var counter = 0;

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

var performance_measurement_values = {
    "min_exec" : -1,
    "max_exec" : -1,
    "avg_exec" : -1
};

var performance_set = [];

$app.$configureInterval(true, 1000);

$app.$initialize = function(initCompleted){
    initCompleted();
};

$app.$task = function(taskCompleted){
    console.log("Task started");
    if(typeof performance != 'undefined')
        time_0 = performance.now();
    else
        time_0 = process.hrtime()
    
    var arr = [];
    for(var i = 0; i < 100; ++i) {
        arr.push(i);
    }
    
    for(i = 0; i < 1e5; ++i) {
        arr.reverse();
    }
    
    if(typeof performance != 'undefined') {
        time_1 = performance.now();
        fin_time = time_1 - time_0;
    } else {
        time_1 = process.hrtime();
        fin_time = (time_1[1] - time_0[1]) / 1e6;
    }
    
    console.log("Task ended");
    console.log("Execution took " + fin_time + " milliseconds.");
    if(fin_time < 0) {
        fin_time = performance_measurement_values.avg_exec;
    }
    performance_set.push(fin_time);
    counter = counter + 1;
    console.log("Iterations " +  counter);
    
    if(performance_measurement_values.max_exec < 0 ||
        performance_measurement_values.max_exec < fin_time) {
        performance_measurement_values.max_exec = fin_time;
    }
    
    if(performance_measurement_values.min_exec < 0 || 
        performance_measurement_values.min_exec > fin_time) {
        performance_measurement_values.min_exec = fin_time;
    }
    
    if(performance_measurement_values.avg_exec < 0) {
        performance_measurement_values.avg_exec = fin_time;
    } else {
        performance_measurement_values.avg_exec = performance_measurement_values.avg_exec + 
        (1 / (counter + 1)) * (fin_time - performance_measurement_values.avg_exec);
    }
    
    console.log("Max exec time " + Math.round(performance_measurement_values.max_exec));
    console.log("Min exec time " + Math.round(performance_measurement_values.min_exec));
    console.log("Avg exec time " + Math.round(performance_measurement_values.avg_exec));
    var variance = getVariance(performance_set, performance_measurement_values.avg_exec);
    console.log("Variance " + variance);
    console.log("Confidence interval [" + 
        (performance_measurement_values.avg_exec - (Math.sqrt(variance) / Math.sqrt(counter))) + 
        ", " + (performance_measurement_values.avg_exec + (Math.sqrt(variance) / Math.sqrt(counter))) + 
        "]");
    
    if(typeof process != 'undefined') {
       
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

function getVariance(arr, mean) {
    return arr.reduce(function(pre, cur) {
        pre = pre + Math.pow((cur - mean), 2);
        return pre;
    }, 0)
}

}
