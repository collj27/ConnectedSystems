// This controller actually handles passing data into the fob pages of the app
var request = require("request");

var sqlite3 = require('sqlite3');
var db = new sqlite3.Database('././db/q5db.db');

// Called to load the fob info/edit page for a specific fob based on the "fob" parameter in the URL
exports.fobedit = function(req, res) {
  // These variables will be passed into the page when it is sent to the client
  var fobPersonName = 'not set';
  var fobIPAddress = 'not set';
  var reqFob = req.params.fob;  // Here we retrieve the "fob" parameter of the URL to lookup the correct data
  // Here we get the fob details from the database
  let sql = 'SELECT Person_Name, IP_Address FROM Fob WHERE Fob_ID = ?';  // The "?"" will be replaced by the fob id now stored in the "reqFob" variable
  db.get(sql, reqFob, function(err,row) {
    if (row != null && row.Person_Name != null && row.Person_Name != '') fobPersonName = row.Person_Name;
    if (row != null && row.IP_Address != null && row.IP_Address != '') fobIPAddress = row.IP_Address;
    // This line actually renders the page to the client, passing in the variable data gathered above
    res.render('fob', { title: 'Fob ' + reqFob + ' - Smart Key - Quest 5, Group 11', person_name: fobPersonName, ip_address: fobIPAddress, fob_id: reqFob });
  });
};

// Accept post request from fob page form to update fob details; then redirect back to fob page
exports.updatefob = function(req, res) {
  // Get information out of the fob info form, from fob.hbs
  var fobPersonName = req.body.formPersonName;
  var fobIPAddress = req.body.formIPAddress;
  var fobFobId = req.body.formFobId;
  // Actual database call to update the fob info and reload the page
  // Load the form data into the data array
  let data = [fobPersonName, fobIPAddress, fobFobId];
  console.log("submitted data: " + data);
  // SQL statement to insert/update the form values into the database
  let sql = 'UPDATE Fob SET Person_Name = ?, IP_Address = ? WHERE Fob_ID = ?';
  // Actual database insert/update
  db.run(sql, data, function(err) {
    if (err) {
      return console.error(err.message);
    }
    console.log(`Row(s) updated: ${this.changes}`);
    // Reloads the fob info/edit page
    res.redirect('/fob/' + fobFobId);
  });
};
