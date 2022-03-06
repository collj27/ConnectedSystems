// This page handles sending requests for the main page of the app to the Controller to get the necessary data and render the page

var express = require('express');
var router = express.Router();

var index_controller = require('../controllers/indexController.js');

// GET home page.
router.get('/', index_controller.index);

module.exports = router;
