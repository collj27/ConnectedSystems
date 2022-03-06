# Overview

Above are team projects for my "connected systems" course that all leverage the ESP32. For a description of each project, please look at "Report.md" in each respective folder. The following were my responsibilities for each project. Since the projects built off each other, much of my code was reusable. 

## Coding responsibilities:

Pseudo-Mechanical Alarm Clock:

- Timer interrupt code (timer.c)

Swiss Army Tape Measure:

- LIDAR and IR code (lidar_sensor.c, IR_sensor.c)

Online Appliance (lines 77-92, 108-208 in main.c):

- Get requests from RPi to ESP32
- Timer interrupt initialization
- HTTPD server initialization
- Wifi initialization

Self-Driving Car (lines 364 - 594 in main_update.c):

- Get requests for sensor values 
- Timer interrupt initialization
- HTTPD server initialization
- Wifi initialization


Smart Key:

- Developed code for the "Hub" (Code/Hub)

Car Navigation lines (216-322, 330-548 in main.c):

- GET and POST Requests
- Timer interrupt initialization
- HTTPD server/client initialization
- Wifi initialization



