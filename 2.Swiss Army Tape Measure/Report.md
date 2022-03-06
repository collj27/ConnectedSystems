  ### Swiss Army Tape Measure
  
  Authors: James Coll, Matt Boyd, Alex Trinh, 2018-10-12
  
  ## Summary
  
  In this quest, we read and graphed distance measurements from an IR sensor, LIDAR sensor, "wheel" sensor, 
  and two ultrasonic sensors. We used node.js to create an electron app and plot the data on dygraphs.

  ## Evaluation Criteria
  
  The following specifications were required in our solution:
  - Periodic reporting of ultrasonic range in m
  - Periodic reporting of LIDAR range in m
  - Periodic reporting of wheel speed in m/s
  - Periodic reporting of IR range in m
  - Results graphed at host
  - Results graphed continuously based on reporting period
  
  All the requirements were met with the exception of the wheel speed. We demonstrate the sensor works by
  flashing our fingers in front of it and plotting the data in m/s, however, it does not actually read the speed 
  of a wheel. 
  
  ## Solution Design
  
  Our code reads and plots data from the sensors concurrently. To accomplish this, we have a timer interrupt every
  0.5s that increments a variable called "vol_seconds". When this is incremented, the code enters our main loop and 
  reads data  from all the sensors. All the data is converted to engineering units per the specifications above.
  Our sensors are mounted on a cardboard stand to ensure they are stable and the readings are accurate. 
  
  ## Sketches and Photos
  
  Preliminary sketch:
  
  ![preliminarySketch](Images/Outline.png)
  
  Sensor mounting setup:
  
  ![sensorSetup](Images/Sensor_Housing.png)
  
  Wiring:
  
  ![wiring](Images/Wiring_Setup.png)
  
  ## Modules,Tools,Sources Used in Solution
  
  ADC Example:<br/> 
  https://github.com/espressif/esp-idf/tree/221eced/examples/peripherals/adc
  
  ADC2 Example:<br/>  
  https://github.com/espressif/esp-idf/tree/master/examples/peripherals/adc2
  
  UART Example:<br/>  
  https://github.com/espressif/esp-idf/tree/221eced/examples/peripherals/uart_async_rxtxtasks
  
  Trigger-Echo Example:<br/>  
  https://github.com/UncleRus/esp-idf-lib/tree/master/examples/ultrasonic
  
  Node SerialPort, reading data:<br/>
https://serialport.io/docs/guide-usage#reading-data

Electron jQuery is not defined:<br/>
https://stackoverflow.com/questions/32621988/electron-jquery-is-not-defined

PowerShell Electron Demo:<br/>
https://xainey.github.io/2017/powershell-electron-demo/

Read buffer data from Node SerialPort:<br/>
https://stackoverflow.com/questions/29477702/how-can-i-read-this-particular-buffer-data-that-im-getting-from-arduino-while-u

Installing modules and rebuilding for Electron:<br/>
https://electronjs.org/docs/tutorial/using-native-node-modules#the-npm-way

Electron-SerialPort:<br/> 
https://github.com/johnny-five-io/electron-serialport

Dygraphs javascript graphing:<br/>
http://dygraphs.com/

Dygraphs format legend datetime:<br/>
https://stackoverflow.com/questions/32148482/how-to-format-date-and-time-in-dygraphs-legend-according-to-user-locale

Component library for ESP32:<br/>
https://github.com/UncleRus/esp-idf-lib

Long Range Infrared Sensor: GP2Y0A02YK0F:<br/>
http://arduinomega.blogspot.com/2011/05/infrared-long-range-sensor-gift-of.html




  
