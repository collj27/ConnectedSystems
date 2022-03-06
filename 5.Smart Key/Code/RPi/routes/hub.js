// This page handles sending requests for the hub-related pages and end-points of the app to the Controller to get the necessary data and render the page, process the data, or return data

var express = require('express');
var router = express.Router();

var hub_controller = require('../controllers/hubController.js');

// GET main hub page.
router.get('/', hub_controller.hub);

// GET API endpoint with parameters to process data sent from hub.
// "fob" = the numeric fob id
router.get('/:fob', hub_controller.hubdata);

// POST API endpoiont with query parameters to update hub data as input in the main hub page.
router.post('/', hub_controller.updatehub);

module.exports = router;
