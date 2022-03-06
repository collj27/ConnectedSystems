// This page handles sending requests for the fob-related pages and end-points of the app to the Controller to get the necessary data and render the page, process the data, or return data

var express = require('express');
var router = express.Router();

var fob_controller = require('../controllers/fobController.js');
var index_controller = require('../controllers/indexController.js');

// GET main page of web app, which shows all fobs.
router.get('/', index_controller.index);

// GET fob edit/detail page with parameters to load single specific fob for editing.
// "fob" = the numeric fob id; uses 0 when inserting new fob
router.get('/:fob', fob_controller.fobedit);

// POST API endpoiont to update fob data as input in the fob form page.
router.post('/update', fob_controller.updatefob);

module.exports = router;
