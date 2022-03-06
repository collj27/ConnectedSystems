// This controller actually handles passing data into the hub page of the app
var request = require("request");

var sqlite3 = require('sqlite3');
var db = new sqlite3.Database('././db/q5db.db');

// Called to load the hub info/edit page
exports.hub = function(req, res) {
  // These variables will be passed into the page when it is sent to the client
  var hubName = 'not set';
  var hubLocation = 'not set';
  var hubStatus = '';
  // Here we get the Hub name and Location from the database
  db.get('SELECT Hub_Name, Location FROM Hub', function(err,row) {
    if (row != null && row.Hub_Name != null && row.Hub_Name != '') hubName = row.Hub_Name;
    if (row != null && row.Location != null && row.Location != '') hubLocation = row.Location;
    console.log("hubName: " + hubName + ", hubLocation: " + hubLocation);
    // This line actually renders the page to the client, passing in the variable data gathered above
    res.render('hub', { title: 'Smart Key Home - Quest 5, Group 11', hub_name: hubName, hub_location: hubLocation, hub_status: hubStatus });
  });
};

// Accept post request from hub page form to update hub name and location info; then redirect back to hub page
exports.updatehub = function(req, res) {
  //Get information out of the hub info form, from hub.hbs
  var hubName = req.body.formHubName;
  var hubLocation = req.body.formHubLocation;
  // Actual database call to update the hub info and reload the page
  // Load the form data into the data array
  let data = [hubName, hubLocation];
  console.log("submitted data: " + data);
  // SQL statement to insert/update the form values into the database
  let sql = 'UPDATE Hub SET Hub_Name = ?, Location = ? WHERE Hub_ID = 1';
  // Actual database insert/update
  db.run(sql, data, function(err) {
    if (err) {
      return console.error(err.message);
    }
    console.log(`Row(s) updated: ${this.changes}`);
    // Reloads the hub info/edit page
    res.redirect('/hub');
  });
};

// GET API endpoint with parameter to process data sent from hub.
// "fob" = the numeric fob id
exports.hubdata = function(req, res) {
  var reqType = 'unlock';
  var reqFob = req.params.fob;  // Here we retrieve the parameter in the URL - Should receive PIN '7568_' followed by Fob ID as '7568_1'
  var pincode = reqFob.substring(0, 5);  // Should be 7568_
  var fobid = reqFob.substring(5, 6);  // SHould be actual Fob ID
  console.log("pincode = " + pincode + ", fobid = " + fobid);
  if (pincode == '7568_') {  // Verify the PIN Code matches - if it does, process the request
    reqFob = fobid;
    console.log("reqFob now = " + reqFob);
    // Lookup current fob status in db
    let sql = 'SELECT Current_Status, IP_Address FROM Fob WHERE Fob_ID = ?';
    db.get(sql, reqFob, function(err,row) {
      if (err) {
        return console.error(err.message);
      }
      if (row != null) {
        console.log("Curr status: " + row.Current_Status);
        if (row.Current_Status == 'unlock') reqType = 'lock';
        // GET request to the actual fob to update fob status
        var getUrl = "http://" + row.IP_Address + "/" + reqType;
        request(getUrl, function(err1, res1, body) {
          if(err1) {
            console.log("Error to follow ");
            console.error(err1);
            res.sendStatus(500);
          } else { // If the fob request is successful, update the database with the entry
            console.log("BODY2: " + body);
            // Actual database call to insert the entry info
            // Load the form data into the data array
            let data = [reqFob, Math.floor(new Date() / 1000), '', reqType, 1];
            console.log("data: " + data);
            // SQL statement to insert an entry into the database
            let sql1 = 'INSERT INTO Entry (Fob_ID, Timestamp, Image, Status, Hub_ID) VALUES (?,?,?,?,?)';
            // Actual database insert
            db.run(sql1, data, function(err) {
              if (err) {
                return console.error(err.message);
              } else {
                console.log(`Row(s) updated: ${this.changes}`);
                // SQL statement to update the fob status in the Fob table
                let sql2 = 'UPDATE Fob SET Current_Status = ? WHERE Fob_ID = ?';
                let data2 = [reqType, reqFob];
                db.run(sql2, data2, function(err) {
                  if (err) {
                    return console.error(err.message);
                  }
                  // If all above successful, send 200 back to client
                  console.log(`Row(s) updated: ${this.changes}`);
                  res.sendStatus(200);
                });
              }
            });
          }
        });
      }
    });
  } else {  // If the PIN Code does not match, send a "Server Error" back to the Hub to indicate the request was invalid
    res.sendStatus(500);
  }
};
