// This file is required by the index.html file and will
// be executed in the renderer process for that window.
// All of the Node.js APIs are available in this process.
const serialport = require('serialport')

const port = new serialport('COM3', {
  baudRate: 115200
});

port.on('data', function (data) {
  var x = new Date();  // current time
  var partsOfStr = data.toString('ascii').split(',');
  dgdata.push([x, parseFloat(partsOfStr[0])]);  // Wheel speed
  dgdata_o1.push([x, parseFloat(partsOfStr[1])]);  // IR Range
  dgdata_o2.push([x, parseFloat(partsOfStr[2])]);  // LIDAR
  dgdata_s1.push([x, parseFloat(partsOfStr[3])]);  // Sonic - Single
  dgdata_s2.push([x, parseFloat(partsOfStr[4])]);  // Sonic - Double
  g.updateOptions( { 'file': dgdata } );
  go1.updateOptions( { 'file': dgdata_o1 } );
  go2.updateOptions( { 'file': dgdata_o2 } );
  gs1.updateOptions( { 'file': dgdata_s1 } );
  gs2.updateOptions( { 'file': dgdata_s2 } );
  //console.log('Data:', data.toString('ascii'))
})
