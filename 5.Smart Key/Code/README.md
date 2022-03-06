<h1>Code Summary</h1>

fob1.c - Final code used on the Fobs; each Fob gets a unique ID assigned in it's code, but otherwise their code is identical<br/><br/>

Hub \ main \ hub.c - Final code used on the Hub; all other files in the 'Hub' folder are typical ESP-related files<br/><br/>

## Node Code for Web Server running on RPi. Gets called by Hub to validate and store (in local SQLite database) Lock/Unlock requests from Fobs; Connects to Fobs to send Lock/Unlock commands to turn on/off green LED on each Fob.

RPi \ app.js - Sets up Node environment, Express web server, and all required packages.<br/><br/>

RPi \ controllers \ fobController.js - Handles pulling data from database and rendering fob info/edit page of web app<br/>
RPi \ controllers \ hubController.js - Handles pulling data from database and rendering hub info/edit page of web app; also recdeives GET requests from Hub to process and insert Lock/Unlock "events" initiated at Fob<br/>
RPi \ controllers \ indexController.js - Handles pulling data from database and rendering main page of web app<br/><br/>

RPi \ db \ q5db.db - SQLite database file (contains all tables and data)<br/><br/>

RPi \ public\ css - Folder containing all necessary CSS files supporting the web app<br/>
RPi \ public \ css \ heroic-features.css - Our custom CSS supporting the web app<br/><br/>

RPi \ public \ js - Folder containing all necessary Javascript files supporting the web app<br/>
RPi \ public \ js \ indexpg.js - Our custom Javascript file supporting the main page fo the web app, including rendering the on-page chart and data table<br/><br/>

RPi \ public \ stylesheets - Folder containing stock CSS included with Express boilerplate web app<br/><br/>

RPi \ public \ vendor - Additional files supporting Bootstrap and jQuery for web app<br/><br/>

RPi \ public \ webfonts - Additional files supporting Font Awesome vector icons for web app<br/><br/>

RPi \ routes \ fob.js - File called when fob-related pages or REST requests of the web app are requested - calls controller to load page and data<br/>
RPi \ routes \ hub.js - File called when hub-related pages or REST requests of the web app are requested - calls controller to load page and data<br/>
RPi \ routes \ index.js - File called when the main page or related REST requests of the web app are requested - calls controller to load page and data<br/><br/>

RPi \ views \ error.hbs - Handlebars template file to load content specific to the default error page<br/>
RPi \ views \ fob.hbs - Handlebars template file to load content specific to the fob page of the web app<br/>
RPi \ views \ hub.hbs - Handlebars template file to load content specific to the hub page of the web app<br/>
RPi \ views \ index.hbs - Handlebars template file to load content specific to the main page of the web app<br/>
RPi \ views \ layout.hbs - Handlebars template file to load the outline of all pages in the site (header, footers, etc.)<br/>

 