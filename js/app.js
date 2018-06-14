IoTApp = function(app) {

  // initializing objects
  app.internal = {};
  app.external = {};
  // initializing internal with agent
  Agent(app.internal);
  ExternalApi(app.external);
}
