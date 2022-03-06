# Node Code for Web Server running on RPi. Connects to ESP to pull temperature data and stores it in local SQLite database.

app.js - Sets up Node environment, Express web server, and all required packages.

Controllers\indexController.js - Handles pulling data from database and rendering main page of web app

db\rpiwebacdb.db - SQLite database file (contains all tables and data)

public\css - Folder containing all necessary CSS files supporting the web app
public\css\heroic-features.css - Our custom CSS supporting the web app

public\js - Folder containing all necessary Javascript files supporting the web app

public\stylesheets - Folder containing stock CSS included with Express boilerplate web app

public\vendor - Additional files supporting Bootstrap and jQuery for web app

routes\index.js - File called when the main page of the web app is requested - calls controller to load page

views\error.hbs - Handlebars template file to load content specific to the default error page
views\index.hbs - Handlebars template file to load content specific to the main page of the web app
views\layout.hbs - Handlebars template file to loads the outline of all pages in the site (header, footers, etc.)

\bin, \node_modules - Default folder included with Node and Esxpress supporting their functionality - we did not edit or interract with these folders
