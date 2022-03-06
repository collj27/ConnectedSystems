<h1>Code Summary</h1>

thermistorCode.c - Final code for the thermistor temperature polling implemented into main.c

main.c - Final version of combined code

## Node Code for Web Server running on RPi. Connects to ESP to pull temperature data and stores it in local SQLite database.

node\app.js - Sets up Node environment, Express web server, and all required packages.

node\Controllers\indexController.js - Handles pulling data from database and rendering main page of web app

node\db\rpiwebacdb.db - SQLite database file (contains all tables and data)

node\public\css - Folder containing all necessary CSS files supporting the web app
node\public\css\heroic-features.css - Our custom CSS supporting the web app

node\public\js - Folder containing all necessary Javascript files supporting the web app

node\public\stylesheets - Folder containing stock CSS included with Express boilerplate web app

node\public\vendor - Additional files supporting Bootstrap and jQuery for web app

node\routes\index.js - File called when the main page of the web app is requested - calls controller to load page

node\views\error.hbs - Handlebars template file to load content specific to the default error page
node\views\index.hbs - Handlebars template file to load content specific to the main page of the web app
node\views\layout.hbs - Handlebars template file to loads the outline of all pages in the site (header, footers, etc.)

node\bin, node\node_modules - Default folder included with Node and Esxpress supporting their functionality - we did not edit or interract with these folders
