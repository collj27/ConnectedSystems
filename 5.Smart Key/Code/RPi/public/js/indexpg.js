// Javascript running the main index page of the web app; loads data table and chart data and creates chart

// When the page is finished loading, call the "/chart_entries" URL to get data for the chart and load chart, 
// call the "/fob_status" URL to get the current fob status from the database and update the page,
// and call the "table_entries" URL to get the last 7 days of entries for display in the data table 
$(function(){
  // Get the data for the chart
  $.ajax({
    url: '/chart_entries',
    type: 'GET',
    success: function(result){
      console.log("success getting entries");
      // Call function to create the chart
      createChart(result);
      // Get the data for the fob statuses
      $.ajax({
        url: '/fob_status',
        type: 'GET',
        success: function(result){
          console.log("success getting entries");
          // Call function to update the fobs
          updateFobs(result);
          // Get the data for the data table
          $.ajax({
            url: '/table_entries',
            type: 'GET',
            success: function(result){
              console.log("success getting table_entries");
              // Call function to create the data table
              dataDump(result);
          }});
      }});
  }});
});

// Global function to setup and render the chart; data from GET above is passed in as 'data' variable
function createChart(data){
  var options = {
            chart: {
                height: 350,
                type: 'bar',
                stacked: true,
                toolbar: {
                    show: true
                },
                zoom: {
                    enabled: true
                }
            },
            responsive: [{
                breakpoint: 480,
                options: {
                    legend: {
                        position: 'bottom',
                        offsetX: -10,
                        offsetY: 0
                    }
                }
            }],
            plotOptions: {
                bar: {
                    horizontal: false,
                },
            },
            series: [{
                name: 'Fob #1',
                data: data[0]
            },{
                name: 'Fob #2',
                data: data[1]
            },{
                name: 'Fob #3',
                data: data[2]
            }],
            xaxis: {
                type: 'number',
                categories: data[3],
            },
            legend: {
                position: 'right',
            },
            fill: {
                opacity: 1
            },
        }

       var chart = new ApexCharts(
            document.querySelector("#chart"),
            options
        );

        chart.render();
}

// Global function to update teh fob statuses and color of fob box based on status; data from GET above is passed in as 'data' variable
function updateFobs(data) {
  // Set fob current status and most recent activity
  document.getElementById('fob1_status').innerHTML = (data[0] == 'lock' ? 'LOCKED' : 'UNLOCKED');
  document.getElementById('fob2_status').innerHTML = (data[2] == 'lock' ? 'LOCKED' : 'UNLOCKED');
  document.getElementById('fob3_status').innerHTML = (data[4] == 'lock' ? 'LOCKED' : 'UNLOCKED');
  document.getElementById('fob1_act').innerHTML = data[1];
  document.getElementById('fob2_act').innerHTML = data[3];
  document.getElementById('fob3_act').innerHTML = data[5];

  if (data[0] == 'unlock') {
    $('#fob1').removeClass("bg-light");
    $('#fob1').addClass("bg-success");
    $('#fob1').addClass("text-white");
  }

  if (data[2] == 'unlock') {
    $('#fob2').removeClass("bg-light");
    $('#fob2').addClass("bg-success");
    $('#fob2').addClass("text-white");
  }

  if (data[4] == 'unlock') {
    $('#fob3').removeClass("bg-light");
    $('#fob3').addClass("bg-success");
    $('#fob3').addClass("text-white");
  }
}

// Global function to setup and render the data table; data from GET above is passed in as 'data' variable
function dataDump(data) {
  var html = '<table class="table table-bordered">';
  html += '<tr>';
  for( var j in data[0] ) {
    html += '<th scope="col">' + j + '</th>';
  }
  html += '</tr>';
  for( var i = 0; i < data.length; i++) {
    html += '<tr>';
    for( var j in data[i] ) {
      html += '<td>' + data[i][j] + '</td>';
    }
  }
  html += '</table>';
  document.getElementById('container').innerHTML = html;
}
