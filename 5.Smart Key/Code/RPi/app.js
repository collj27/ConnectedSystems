var express = require('express');
var path = require('path');
var cookieParser = require('cookie-parser');
var logger = require('morgan');
var session = require('express-session');
var request = require('request');  // Required to make GET requests to ESP "hub"

// Required for the SQLite3 database
var sqlite3 = require('sqlite3');
// Database location inside the Node app
var db = new sqlite3.Database('./db/q5db.db');

// Routes to the various pages of the app
var indexRouter = require('./routes/index');
var hubRouter = require('./routes/hub');
var fobRouter = require('./routes/fob');

// Actually starts an instance of the web server
var app = express();

// Use the session middleware to track objects between pages when necessary
app.use(session({ secret: 'keyboard cat', cookie: { maxAge: 60000 }, resave: true,saveUninitialized: true}))

// View engine setup (boilerplate code)
app.set('views', path.join(__dirname, 'views'));
app.set('view engine', 'hbs');

app.use(logger('dev'));
app.use(express.json());
app.use(express.urlencoded({ extended: false }));
app.use(cookieParser());
app.use(express.static(path.join(__dirname, 'public')));

// Adds URL reoutes to the Express web server and directs the route to its proper "router" file
app.use('/', indexRouter);
app.use('/hub', hubRouter);
app.use('/fob', fobRouter);

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

// Actually send all of the data to Express to run the webserver
module.exports = app;
