var createError = require('http-errors');
var express = require('express');
var path = require('path');
var cookieParser = require('cookie-parser');
var logger = require('morgan');
var request = require('request');  // Required to make GET requests to ESP
var querystring = require('querystring');  // Required to pull the querystrings from the GET API

// Actually starts an instance of the web server
var app = express();

// Instatiate the objects needed for socket.io
var server = require('http').Server(app);  // Required for socket.io to work
var io = require('socket.io')(server);  // Handles socket communication with web page

// view engine setup (boilerplate code)
app.set('views', path.join(__dirname, 'views'));
app.set('view engine', 'hbs');
app.use(logger('dev'));
app.use(express.json());
app.use(express.urlencoded({ extended: false }));
app.use(cookieParser());
app.use(express.static(path.join(__dirname, 'public')));

// Global "local" variables used to track stored beacon fragments
app.locals.frag1 = '..........';
app.locals.frag2 = '..........';
app.locals.frag3 = '..........';
app.locals.frag4 = '..........';

// Default URL, loads main web page of app
app.get('/', function(req, res){
  res.render('index', { title: 'Find Beacons/Escape Course - Quest 6, Group 11' });
});

// GET API endpoint with parameters to process sensor data sent from ESP.
// "dist" = the value for the front sensor distance
app.get('/sensor/:dist', function(req, res){
  var reqDist = req.params.dist;  // Should receive distance value as integer
  if (reqDist != null & reqDist != '') {  // If a sensor value is received, process it
    io.emit('sensor', reqDist);  // Send sensor value to the web page
    res.sendStatus(200);  // And send a "success" message to the ESP
  } else {
    res.sendStatus(500);  // No value received, send a "error" message to the ESP
  }
});

// GET API endpoint with parameter to process beacon fragment data sent from ESP.
// "id_frag" in querystring = the id value and text fragment coming from the beacon
app.get('/beacon/', function(req, res){
  // Access the provided 'id_frag' query parameter
  let reqIdFrag = req.query.id_frag;  // Should receive beacon fragment data
  if (reqIdFrag != null & reqIdFrag != '') {  // If data is received, process it
    var reqId = reqIdFrag.substring(0, 1);  // Should be beacon id value 1-4
    // Only send the beacon value to the web page if it has not already been "locked"
    if (reqId == '1' && app.locals.frag1 == '..........') io.emit('beacon', reqIdFrag);  // Send data value to the web page
    else if (reqId == '2' && app.locals.frag2 == '..........') io.emit('beacon', reqIdFrag);  // Send data value to the web page
    else if (reqId == '3' && app.locals.frag3 == '..........') io.emit('beacon', reqIdFrag);  // Send data value to the web page
    else if (reqId == '4' && app.locals.frag4 == '..........') io.emit('beacon', reqIdFrag);  // Send data value to the web page
    res.sendStatus(200);  // And send a "success" message to the ESP
  } else {
    res.sendStatus(500);  // No data received, send a "error" message to the ESP
  }
});

// GET API endpoint sent from ESP when car motion has stopped.
app.get('/stop', function(req, res){
  io.emit('stop');  // Send stop notice to the web page
  res.sendStatus(200);  // And send a "success" message to the ESP
});

// Function called when motion command is sent from web page via socket.io
// Sends POST request to ESP to initiate indicated car motion
function updateEsp(direction) {
  // Setup variables for POST to ESP to update car's motion
  var clientServerOptions = {
      uri: 'http://192.168.1.104/command',
      body: direction,
      method: 'POST',
      headers: {
          'Content-Type': 'text/plain'
      }
  }
  // Actually send the POST request
  request(clientServerOptions, function (error, response) {
      console.log(error,response);
  });
}

// Setup socket.io
io.on('connection', function(socket){
  // When a motion command is selected on the web page, send it to the ESP on the car
  socket.on('motion', function(direction){
    updateEsp(direction);
    console.log('motion: ' + direction);
  });
  // When a beacon fragement is "locked" on the web page, store it in local variables
  socket.on('fragLock', function(data){
    switch(data.id) {
      case '1':
        app.locals.frag1 = data.frag;
        break;
      case '2':
        app.locals.frag2 = data.frag;
        break;
      case '3':
        app.locals.frag3 = data.frag;
        break;
      case '4':
        app.locals.frag4 = data.frag;
        break;
      default:
        console.log('incorrect id sent to fragLock socket...');
    }
    console.log('fragLock id: ' + data.id + ', frag: ' + data.frag);
  });
});

// Boilerplate: catch 404 and forward to error handler
app.use(function(req, res, next) {
  next(createError(404));
});

// Boilerplate: error handler
app.use(function(err, req, res, next) {
  // set locals, only providing error in development
  res.locals.message = err.message;
  res.locals.error = req.app.get('env') === 'development' ? err : {};

  // render the error page
  res.status(err.status || 500);
  res.render('error');
});

// Actually send all of the data to Express to run the webserver, including the socket.io server
module.exports = {app: app, server: server};
