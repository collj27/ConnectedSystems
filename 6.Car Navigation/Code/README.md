<h1>Code Summary</h1>

main.c - Final version of combined code


## Node Code for Web Server running on RPi. 

RPI \ bin \ www - Customized file setting up Express webserver and allowing socket.io and Express to communicate on the same TCP port (3000).<br/><br/>

RPi \ app.js - Sets up Node environment, Express web server, and all required packages. Also answers all client, REST, and socket.io requests.<br/><br/>

RPi \ public\ css - Folder containing all necessary CSS files supporting the web app<br/>
RPi \ public \ css \ the-big-picture.css - Our custom CSS supporting the web app<br/><br/>

RPi \ public \ socket.io - Javascript socket.io client used by the web app to connect to the socket.io server running on this Node server<br/><br/>

RPi \ public \ vendor - Additional files supporting Bootstrap and jQuery for web app<br/><br/>

RPi \ public \ webfonts - Additional files supporting Font Awesome vector icons for web app<br/><br/>

RPi \ views \ error.hbs - Handlebars template file to load content specific to the default error page<br/>
RPi \ views \ index.hbs - Handlebars template file to load content specific to the main page of the web app<br/>
RPi \ views \ layout.hbs - Handlebars template file to load the outline of all pages in the site (header, footers, sitewide javascript, etc.)<br/>