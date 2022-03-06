// This page handles sending requests for the main page of the app to the Controller to get the necessary data and render the page

var express = require('express');
var router = express.Router();

var index_controller = require('../controllers/indexController.js');

// GET home page.
router.get('/', index_controller.index);

// GET entry data for home page data table.
// Queries the database for last 7 days of unlock/lock entries to populate the data table on the page
// Called after the page is loaded so this data request doesn't slow the page load
// Also pushed to page when new lock/unlock happens
router.get('/table_entries', index_controller.table_entries);

// GET entry data for home page chart.
// Queries the database for last 12 hours of unlock/lock entries to populate the chart on the page
// Called after the page is loaded so this data request doesn't slow the page load
// Also pushed to page when new lock/unlock happens
router.get('/chart_entries', index_controller.chart_entries);

// GET fob status data for home page chart.
// Queries the database for most recent unlock/lock entries to update fob status on the page
// Called after the page is loaded so this data request doesn't slow the page load
// Also pushed to page when new lock/unlock happens
router.get('/fob_status', index_controller.fob_status);

module.exports = router;
