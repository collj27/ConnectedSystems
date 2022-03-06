// This controller actually handles passing data into the main page of the app
var request = require("request");

var sqlite3 = require('sqlite3');
var db = new sqlite3.Database('././db/rpiwebacdb.db');

exports.index = function(req, res) {
  // These variables will be passed into the page when it is sent to the client
  var tempv = 0;
  var state = '';
  var schedon = '0';
  var schedoff = '0';
  var sched_saved;
  // Here we get the current on/off state from the database
  db.get('SELECT state AS state FROM power', function(err,row) {
    if (row.state == 'on') state = 'checked';
    console.log("state:" + row.state);
    request('http://192.168.1.113/temp', function(err, resp, body) {
      if(err) {
        console.log("GET Temp Error: " + err);
      } else {
        console.log("body temp: " + body);
        tempv = body;
        // Here we get the on/off schedule from the database (if it exists)
        db.get("SELECT on_hour, on_minute, off_hour, off_minute FROM sched WHERE schedule=1", function(err,row) {
          if (row.on_hour) {
            schedon = row.on_hour + ':' + row.on_minute;
            schedoff = row.off_hour + ':' + row.off_minute;
            sched_saved = 'yes';
            // This line actually renders the page to the client, passing in the variable data gathered above
            res.render('index', { title: 'Smart A/C - Quest3 Group 11', temp: tempv, ac_status: state, sched_on: schedon, sched_off: schedoff, sched_saved: sched_saved });
          }
          else {
            console.log("sched on: " + schedon);
            // This line actually renders the page to the client, passing in the variable data gathered above
            res.render('index', { title: 'Smart A/C - Quest3 Group 11', temp: tempv, ac_status: state, sched_on: schedon, sched_off: schedoff, sched_saved: sched_saved });
          }
        });
      }
    });
  });
};
