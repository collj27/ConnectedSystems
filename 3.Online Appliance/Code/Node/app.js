// Components required to run Node and Express (the web server)
var createError = require('http-errors');
var express = require('express');
var path = require('path');
var cookieParser = require('cookie-parser');
var logger = require('morgan');
var session = require('express-session')
var request = require("request");
var schedule = require('node-schedule');

// Required for the SQLite3 database
var sqlite3 = require('sqlite3');
// Database location inside the Node app
var db = new sqlite3.Database('./db/rpiwebacdb.db');

// Route to the main (only) page of the app
var indexRouter = require('./routes/index');

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

app.use('/', indexRouter);


// Post from 'save' button to set on/off schedule
app.post('/sched_form', function(req, res){
    //Get information out of the schedule form, from index.hbs
    var ontime = req.body.formTimeOn;
    var offtime = req.body.formTimeOff;
    // Format the form data to the format required in the database
    let data = [ontime.substring(0,2),ontime.substring(3,5),offtime.substring(0,2),offtime.substring(3,5)];
    console.log(data);
    // SQL statement to insert the form values into the database
    let sql = 'UPDATE sched SET on_hour = ?, on_minute = ?, off_hour = ?, off_minute = ? WHERE schedule = 1';
    // Actual database insert/update
    db.run(sql, data, function(err) {
      if (err) {
        return console.error(err.message);
      }
      console.log(`Row(s) updated: ${this.changes}`);
      if (ontime) {
        // If the database insert is successful and the schedule is not null, actually schedule the Node job to turn on/off the AC
        scheduleAc('schedule', function(status) {
          if (status == 'success') { res.redirect('/'); }  // Send the main page back to the user's browser (refresh it)
          else { return console.log("this should never get logged..."); }
        });
      } else {
        // If the database insert is successful and the schedule is null, delete any existing scheduled Node jobs
        scheduleAc('cancel', function(status) {
          if (status == 'success') { res.redirect('/'); }  // Send the main page back to the user's browser (refresh it)
          else { return console.log("this should never get logged..."); }
        });
      }
    });
});

// Global function to turn the AC on or off (by calling the ESP to move the servo)
// And update the current AC status in the database
function changeAcState(state, callback) {
  var result = 'success';
  // Actual GET call to the ESP which triggers the servo
  request('http://192.168.1.113/echo', function(err, res, body) {
    if(err) {
      console.log("Error to follow ");
      console.error(err);
      result = 'error';
      callback(result);
    } else { // If the ESP call is successful, update the database with the current state (as indicated in the app)
        console.log("BODY2: " + body);
        let data = ['off'];
        if (state == 'true') {
          data = ['on'];
        }
        let sql = 'UPDATE power SET state = ?';
        db.run(sql, data, function(err) {
          if (err) {
            console.error("SQL Error: " + err.message);
            result = 'error';
            callback(result);
          }
          console.log(`Row(s) updated: ${this.changes}`);
          callback(result); //Sends success back to ajax call in client
        });
      }
  });
}

// Global function to schedule Node to turn the AC off or on (or cancel the schedule)
// 'what' is asking for what you want to do: 'cancel' or 'schedule'
function scheduleAc(what, callback) {
  var result = 'success';
  // If they already exist, cancel the existing scheduled jobs
  var my_job = schedule.scheduledJobs['ac_on'];
  if (my_job) { my_job.cancel(); }
  var my_job2 = schedule.scheduledJobs['ac_on'];
  if (my_job2) { my_job2.cancel(); }
  // If this is a 'schedule', lookup the job schedule in the database and schedule the jobs
  if (what == 'schedule') {
    // Check for time schedule in db; if exists, schedule jobs
    db.get("SELECT on_hour, on_minute, off_hour, off_minute FROM sched WHERE schedule=1", function(err,row) {
      if (row.on_hour) {
        console.log("schedule in db, scheduling job");
        var on_rule = new schedule.RecurrenceRule();
        on_rule.hour = row.on_hour;
        on_rule.minute = row.on_minute;
        var off_rule = new schedule.RecurrenceRule();
        off_rule.hour = row.off_hour;
        off_rule.minute = row.off_minute;
        // This schedules the Node job using the timing rule above
        // The function at the end of the line is what will happen when the job runs on schedule
        var k = schedule.scheduleJob('ac_on',on_rule,function(){
          // Request to turn on AC here, after checking current status in DB
          db.get('SELECT state AS state FROM power', function(err,row) {
            if (err) { return console.log("SQL Error checking power state; will NOT attempt to turn on AC"); }
            if (row.state == 'off') {
              // Turn on the AC
              changeAcState('true', function(status) {
                if (status == 'success') { console.log('AC turned on'); }
                else { console.log("Error encountered - AC NOT turned on"); }
              });
            } else {
              console.log('Scheduled to turn the AC on, but it is already on, so no action taken.');
            }
          });
        });
        // This schedules the Node job using the timing rule above
        // The function at the end of the line is what will happen when the job runs on schedule
        var l = schedule.scheduleJob('ac_off',off_rule,function(){
          // Request to turn off AC here, after checking current status in DB
          db.get('SELECT state AS state FROM power', function(err,row) {
            if (err) { return console.log("SQL Error checking power state; will NOT attempt to turn off AC"); }
            if (row.state == 'on') {
              // Turn off the AC
              changeAcState('false', function(status) {
                if (status == 'success') { console.log('AC turned off'); }
                else { console.log("Error encountered - AC NOT turned off"); }
              });
            } else {
              console.log('Scheduled to turn the AC off, but it is already off, so no action taken.');
            }
          });
        });
        callback(result);
      }
      else {
        console.log("no schedule in db");
        callback(result);
      }
    });
  } else {
    callback(result);
  }
}

// Post when 'Power' button is changed in app
app.post('/power_form', function(req, res){
  var state = req.body.state;
  // Call the function to actually turn the AC on/off and update the Database
  // The structure below forces the job to wait for the results of the function before letting the
  // browser know if the power changes was successful
  changeAcState(state, function(status) {
    if (status == 'success') { res.end(); }
    else { return console.log("/power_form error"); }
  });
});

// Get request from the main web page of the app
// Queries the database for temperature values to populate the chart on the page
// Called after the page is loaded so this data request doesn't slow the page load
app.get('/degrees', function(req, res){
  // Array variables to hold the data sent back to the web page
  var dbtemps = [];
  var dbac = [];
  var dbtimes = [];
  var data = [];
  // Actual database call to lookup the last 12 hours of data
  db.all("SELECT strftime('%H:%M',time) as time, degree, ac FROM degree WHERE time > datetime('now','-12 hours','localtime') ORDER BY time", function (err, rows, fields) {
    if (err) console.log(err);
    for (var i = 0; i < rows.length; i++)
    {
      dbtemps.push(rows[i].degree);
      dbac.push(rows[i].ac);
      dbtimes.push(rows[i].time);
    }
    data.push(dbtimes);
    data.push(dbtemps);
    data.push(dbac);
    // Return the data as an array to the web page
    res.setHeader('Content-Type','application/json');
    res.status(200).send(data);
  });
});

// When Node and Express are up and running, schedule the Node jobs to:
// 1. Pull temps from the ESP's attached temperature sensor and insert them into the db
// 2. If there is a scheduled on/off time in teh database, schedule those jobs as well
app.listen(function () {
  console.log("App started");
  // Schedule Node job to get temperature from sensor every 30 minutes
  var rule = new schedule.RecurrenceRule();
  rule.minute = [0,30];
  // This schedules the Node job using the timing rule above
  // The function at the end of the line is what will happen when the job runs on schedule
  var j = schedule.scheduleJob('get_degrees',rule,function(){
    // Actual GET call to the ESP to get the current reading from the attached temp sensor
    request('http://192.168.1.113/temp', function(err, res, body) {
      if(err) {
        return console.log("GET Temp Error: " + err);
      }
      console.log("body: " + body);
      let data = [body, body];
      // SQL code to insert the current temp reading into the database
      let sql = 'INSERT INTO degree (degree,ac) VALUES (?,?)';
      // Check the database for the current status of the AC (on or off)
      // If it's on, store an additional value so the graph can indicate when the AC is on
      db.get('SELECT state AS state FROM power', function(err,row) {
        if (row.state == 'off') {
          data = [body];
          sql = 'INSERT INTO degree (degree) VALUES (?)';
        }
        // Actually insert temp data into the db
        db.run(sql, data, function(err) {
          if (err) {
            return console.error("SQL Error: " + err.message);
          }
          console.log(`Row(s) inserted: ${this.changes} w/ temp: ` + body);
        });
      });
    });
  });
  // Call function to schedule the Node jobs to turn the AC on or off (if the scheudle exists in the db)
  scheduleAc('schedule', function(status) {
    if (status == 'success') { console.log("all jobs scheduled"); }
    else { console.log("this should never get logged..."); }
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

// Actually send all of the data to Express to run the webserver
module.exports = app;
