# Overview
## REST API

By default you can see the most up to date API from the root of the program URL (eg. http://localhost:7681/)
or just open http-files/index.html.

## Javascript Application

The javascript application has 3 mandatory files (*liquidiot.json, main.js, package.json*) and *resource/* folder where you can store anything.

### liquidiot.json

Is a json file with 2 fields *deviceCapabilities* and *applicationInterfaces* where currently *deviceCapabilities* is not in any real use.

```
{"deviceCapabilities":[], "applicationInterfaces":["testApi"]}
```
### main.js

Has all the logic of the deployed application and user can fill 3 functions (*app.internal.initialize*, *app.internal.task*, *app.internal.terminate*) that have some native functionality. In addition to this you can define REST API of your own by setting **liquidiot.json** file `{"deviceCapabilities":[], "applicationInterfaces":["testApi"]}` like this and then defining *app.external.testApi* and implementing it. Roughly the same way as it is in **examples/test/main.js**. *response.write* takes raw string input so even direct JSON possible.

```
testi.external.testApi = function(request,response) {
  response.setHeader("Content-type", "text/html");
  response.statusCode = 200;

  response.write("Event counter: " + event_counter + "<br>" +
    "Mr gdfg: " + request.params.gdfg + "<br>" + "What?: " + request.params.wut
  );
  response.end();
};
```

One can take a close look at the possibilities that are currently available by looking at **js/request.js** and **js/response.js** files as they work as interface for the main program.

### package.json

Is a node style package file and it mostly works as expected by node standards with exeptions where fields will not be read if they are not proper type (string will always work and id is integer). Main field is mandatory or one can expect undefined behaviour. It is currently highly recommended to not define id at all or define id in every app deployed.

```
{
  "name":"test_application",
  "version":"1.0.0",
  "description":"Test application",
  "main":"main.js",
  "scripts":"",
  "license":"ICS",
  "id":12345,
  "status":"initializing"
}
```

### Logging

Logs are stored in the applications own root folder and basically every print from javascript console,print,alert are logged inside the file. Next example print comes from JSON prints that the REST API will produce.
```
[
    {
        "timestamp": "Sat Jun 30 11:06:24 2018",
        "type": "Log",
        "msg": "Initialize"
    },
    {
        "timestamp": "Sat Jun 30 11:06:24 2018",
        "type": "Print",
        "msg": "Setting interval in header..."
    },
```

Same inside the logs.txt.

```
[Sat Jun 30 11:06:24 2018] [Log] Initialize
[Sat Jun 30 11:06:24 2018] [Print] Setting interval in header...
```

## Node support

Almost nothing from node will work. That said, there is **require** system in place with the help of duktape. So you can define you own libraries and use forward absolute paths (no '..') or use native libraries just as with node (which there is currently none). Also there is native Buffer support from duktape and it should work properly (although it is not tested at all).

## General Javascript

Javscript engine come with duktape which implements ECMAScript 5/5.1 for the most part and some random properties of ECMAScript 2015 and ECMAScript 2016 but currently the support seems to be very minimal.
