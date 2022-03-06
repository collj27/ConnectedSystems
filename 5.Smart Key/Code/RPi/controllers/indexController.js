// This controller actually handles passing data into the main page of the app
var request = require("request");

var sqlite3 = require('sqlite3');
var db = new sqlite3.Database('././db/q5db.db');

// Called to load the main page
exports.index = function(req, res) {
  // These variables will be passed into the page when it is sent to the client
  var hubName = 'not set';
  var hubLocation = 'not set';
  var hubId = '0';
  var fobInfo = [];
  var hubUnlocks = 0;
  var hubUnlockText = '';
  // Custom javascript and external javascript just for this page:
  var scripts = [{ script: '/js/indexpg.js' }, { script: 'https://cdn.jsdelivr.net/npm/apexcharts'}];
  // Here we get the Hub name and Location from the database
  db.get('SELECT Hub_Name, Location, Hub_ID FROM Hub', function(err,row) {
    if (row != null && row.Hub_Name != null && row.Hub_Name != '') hubName = row.Hub_Name;
    if (row != null && row.Location != null && row.Location != '') hubLocation = row.Location;
    if (row != null && row.Hub_ID != null && row.Hub_ID != '') hubId = row.Hub_ID;
    // Here we get the Fob info from the database
    db.all(`SELECT Fob_ID, Person_Name, Current_Status FROM Fob`, function(err2, rows, fields) {
      if (err2) console.log(err2);
      for (var i = 0; i < rows.length; i++)
      {
        fobInfo.push(rows[i]);
        if(rows[i].Current_Status == "unlock") hubUnlocks += 1;
      }
      hubUnlockText = 'Unlocked Keys: ' + hubUnlocks;
      // This line actually renders the page to the client, passing in the variable data gathered above
      res.render('index', { title: 'Smart Key Home - Quest 5, Group 11', hub_name: hubName, hub_location: hubLocation, hub_id: hubId, hub_unlocks: hubUnlockText, fobs: fobInfo, scripts: scripts });
    });
  });
};

// GET API endpoint to get the unlock/lock data from the database (if any exists) for display in data table
exports.table_entries = function(req, res) {
  // Array variable to hold the data sent back to the web page
  var data_raw = [];
  // Actual database call to lookup the last 7 days of data
  db.all(`SELECT e.Hub_ID AS Hub, datetime(e.Timestamp, 'unixepoch', 'localtime') AS Time, e.Status AS Type, e.Fob_ID AS Fob, f.Person_Name AS Person
          FROM Entry AS e LEFT OUTER JOIN Fob AS f ON e.Fob_ID = f.Fob_ID WHERE datetime(e.Timestamp, 'unixepoch', 'localtime') >= datetime('now', '-7 days', 'localtime')
          ORDER BY e.Timestamp`, function(err, rows, fields) {
            if (err) console.log(err);
            for (var i = 0; i < rows.length; i++)
            {
              data_raw.push(rows[i]);
            }
            // Return the data as an array to the web page
            res.setHeader('Content-Type','application/json');
            res.status(200).send(data_raw);
  });
};

// GET API endpoint to get the unlock/lock data from the database (if any exists) for display in chart
exports.chart_entries = function(req, res) {
  // Array variables to hold the data sent back to the web page; preloaded with data incase none exists in the database
  var fob1_data = [0,0,0,0,0,0,0,0,0,0,0,0,0];
  var fob2_data = [0,0,0,0,0,0,0,0,0,0,0,0,0];
  var fob3_data = [0,0,0,0,0,0,0,0,0,0,0,0,0];
  var times = [0,1,2,3,4,5,6,7,8,9,10,11,12];
  var data = [];
  // Actual database call to lookup the last 12 hours of data and populate the arrays as necessary
  db.all(`SELECT COUNT(Fob_ID) AS Fob_Count, Fob_ID, Hours FROM (
          SELECT Fob_ID, datetime(Timestamp, 'unixepoch', 'localtime') AS Timestamp, datetime('now', 'localtime') AS now, (strftime('%s',datetime('now', 'localtime')) - strftime('%s', datetime(Timestamp, 'unixepoch', 'localtime')))/3600 AS Hours FROM Entry WHERE Status='unlock' AND datetime(Timestamp, 'unixepoch', 'localtime') >= datetime('now', '-12 hours', 'localtime'))
          GROUP BY Fob_ID, Hours ORDER BY Hours, Fob_ID`, function(err, rows, fields) {
            if (err) console.log(err);
            for (var i = 0; i < rows.length; i++)
            {
              if (rows[i].Fob_ID == '1') {
                fob1_data[rows[i].Hours] = rows[i].Fob_Count;
              }
              if (rows[i].Fob_ID == '2') {
                fob2_data[rows[i].Hours] = rows[i].Fob_Count;
              }
              if (rows[i].Fob_ID == '3') {
                fob3_data[rows[i].Hours] = rows[i].Fob_Count;
              }
            }
            data.push(fob1_data);
            data.push(fob2_data);
            data.push(fob3_data);
            data.push(times);
            console.log("chart_data: " + data);
            // Return the data as an array to the web page
            res.setHeader('Content-Type','application/json');
            res.status(200).send(data);
  });
};

// GET API endpoint to get the current fob statuses from the database for display on page
exports.fob_status = function(req, res) {
  // Array variable to hold the data sent back to the web page
  var data = [];
  // Actual database call to lookup the fob status info and populate the array variable
  db.get(`SELECT CASE WHEN Fob1.Status IS NULL THEN 'lock' ELSE Fob1.Status END AS Fob1_Status, CASE WHEN Fob1.Time IS NULL THEN 'N/A' ELSE 'Hub#' || Fob1.Hub_ID || ' - ' || Fob1.Time END AS Fob1_Act, CASE WHEN Fob2.Status IS NULL THEN 'lock' ELSE Fob2.Status END AS Fob2_Status, CASE WHEN Fob2.Time IS NULL THEN 'N/A' ELSE 'Hub#' || Fob2.Hub_ID || ' - ' || Fob2.Time END AS Fob2_Act, CASE WHEN Fob3.Status IS NULL THEN 'lock' ELSE Fob3.Status END AS Fob3_Status, CASE WHEN Fob3.Time IS NULL THEN 'N/A' ELSE 'Hub#' || Fob3.Hub_ID || ' - ' || Fob3.Time END AS Fob3_Act
  FROM (SELECT MAX(datetime(Timestamp, 'unixepoch', 'localtime')) AS Time, Hub_ID, Status FROM Entry WHERE Fob_ID = 1) AS Fob1
  , (SELECT MAX(datetime(Timestamp, 'unixepoch', 'localtime')) AS Time, Hub_ID, Status FROM Entry WHERE Fob_ID = 2) AS Fob2
  , (SELECT MAX(datetime(Timestamp, 'unixepoch', 'localtime')) AS Time, Hub_ID, Status FROM Entry WHERE Fob_ID = 3) AS Fob3`, function(err, row) {
      if (err) console.log(err);
      data.push(row.Fob1_Status);
      data.push(row.Fob1_Act);
      data.push(row.Fob2_Status);
      data.push(row.Fob2_Act);
      data.push(row.Fob3_Status);
      data.push(row.Fob3_Act);
      console.log("data: " + data);
      // Return the data as an array to the web page
      res.setHeader('Content-Type','application/json');
      res.status(200).send(data);
  });
};
