const config = {
  api_endpoint: 'https://u50ui9vcia.execute-api.us-east-1.amazonaws.com/beta/flamex_data'
};
var values_x = [];
var coluomns_colors = []; 
var colors = ["blue", "red", "black"]
var current_color = 0;
var values_y = [];
var ch1;
var ch1Stats; // For displaying statistics on Chart1
var values_y_gas = [];
var coluomns_colors_gas = [];
var values_y_stats = []; // Array for storing aggregated statistics
var ch2;
var ch2Stats; // For displaying statistics on Chart2
var input = [];
var input2 = [];

function displayDataOnChart(data_input) {
  console.log(data_input);
  const parsed_data = JSON.parse(data_input);

  console.log(parsed_data);

  // Clear the arrays before populating them with new data
  values_x = [];
  values_y = [];
  values_y_gas = [];
  coluomns_colors_gas = [];
  input = [];
  input2 = [];

  parsed_data.sort((a, b) => {
    return new Date(a.sample_time) - new Date(b.sample_time); // Sort the data based on sample time
  });

  for (let i = 0; i < parsed_data.length; i++) {
    coluomns_colors.push(colors[1]);
    coluomns_colors_gas.push(colors[0]);
    const sampleTime = new Date(parsed_data[i]["sample_time"]);
    label = sampleTime.toLocaleString(); // Convert sample time to Locale format

    values_x.push(label); // Add the label to the values_x array

    values_y.push(parsed_data[i].device_data.flame);
    values_y_gas.push(parsed_data[i].device_data.gas);

    input.push([parsed_data[i].sample_time, parsed_data[i].device_data.flame]);
    input2.push([parsed_data[i].sample_time, parsed_data[i].device_data.gas]);
  }

  // Update the chart with the new data
  ch1.data.labels = values_x;
  ch1.data.datasets[0].data = values_y;
  ch1.update();

  ch2.data.labels = values_x;
  ch2.data.datasets[0].data = values_y_gas;
  ch2.update();

  ch3.data.labels = values_x.slice(-360); // Update the fire sensor chart data with the last hour values
  ch3.data.datasets[0].data = values_y.slice(-360);
  ch3.update();

  ch4.data.labels = values_x.slice(-360); // Update the gas sensor chart data with the last hour values
  ch4.data.datasets[0].data = values_y_gas.slice(-360);
  ch4.update();

  calculateStatistics();
}



function calculateStatistics() {
  const lastHourData = input.slice(-60); // Retrieve data from the last hour
  const lastHourDataGas = input2.slice(-60); // Retrieve gas data from the last hour
  const lastHourDataFire = input.slice(-60); // Retrieve fire data from the last hour

  // Calculate aggregated statistics
  const flameStats = calculateAggregatedStats(lastHourData);
  const gasStats = calculateAggregatedStats(lastHourDataGas);
  const fireStats = calculateAggregatedStats(lastHourDataFire);

  displayStatsOnChart(flameStats, gasStats, fireStats);
}


function calculateAggregatedStats(data) {
  const values = data.map((entry) => entry[1]); // Extract the values from the data array

  const average = calculateAverage(values);
  const minimum = Math.min(...values);
  const maximum = Math.max(...values);

  return {
    average,
    minimum,
    maximum
  };
}

function calculateAverage(values) {
  const sum = values.reduce((acc, val) => acc + val, 0);
  return sum / values.length;
}

function displayStatsOnChart(flameStats, gasStats, fireStats) {
  const statsCanvas1 = document.getElementById("Chart1Stats");
  const statsCanvas2 = document.getElementById("Chart2Stats");
  const statsCanvas3 = document.getElementById("Chart3Stats"); // Get the fire sensor statistics canvas
  const statsCanvas4 = document.getElementById("Chart4Stats"); // Get the gas sensor statistics canvas

  ch1Stats = new Chart(statsCanvas1, {
    type: "bar",
    data: {
      labels: ["Average", "Minimum", "Maximum"],
      datasets: [{
        backgroundColor: colors[1],
        data: [
          flameStats.average.toFixed(2),
          flameStats.minimum,
          flameStats.maximum
        ]
      }]
    },
    options: {
      legend: { display: false },
      title: { display: true },
      scales: {
        yAxes: [{
          ticks: { beginAtZero: true }
        }]
      }
    }
  });

  ch2Stats = new Chart(statsCanvas2, {
    type: "bar",
    data: {
      labels: ["Average", "Minimum", "Maximum"],
      datasets: [{
        backgroundColor: colors[0],
        data: [
          gasStats.average.toFixed(2),
          gasStats.minimum,
          gasStats.maximum
        ]
      }]
    },
    options: {
      legend: { display: false },
      title: { display: true },
      scales: {
        yAxes: [{
          ticks: { beginAtZero: true }
        }]
      }
    }
  });

  ch3Stats = new Chart(statsCanvas3, { // Create a new chart for fire sensor statistics
    type: "bar",
    data: {
      labels: ["Average", "Minimum", "Maximum"],
      datasets: [{
        backgroundColor: colors[1],
        data: [
          fireStats.average.toFixed(2),
          fireStats.minimum,
          fireStats.maximum
        ]
      }]
    },
    options: {
      legend: { display: false },
      title: { display: true },
      scales: {
        yAxes: [{
          ticks: { beginAtZero: true }
        }]
      }
    }
  });

  ch4Stats = new Chart(statsCanvas4, { // Create a new chart for gas sensor statistics
    type: "bar",
    data: {
      labels: ["Average", "Minimum", "Maximum"],
      datasets: [{
        backgroundColor: colors[0],
        data: [
          gasStats.average.toFixed(2),
          gasStats.minimum,
          gasStats.maximum
        ]
      }]
    },
    options: {
      legend: { display: false },
      title: { display: true },
      scales: {
        yAxes: [{
          ticks: { beginAtZero: true }
        }]
      }
    }
  });
}


function createPlot(name, val_y) {
  var chartData = {
    labels: values_x,
    datasets: [{
      backgroundColor: coluomns_colors,
      data: val_y
    }]
  };

  if (name === "Chart1") {
    ch1 = new Chart(name, {
      type: "bar",
      data: chartData,
      options: {
        legend: { display: false },
        title: { display: true },
        scales: {
          yAxes: [{
            ticks: { beginAtZero: true }
          }]
        }
      }
    });
    return ch1;
  } else if (name === "Chart2") {
    var chartDataGas = {
      labels: values_x,
      datasets: [{
        backgroundColor: colors[0], // Use a different color for the gas chart
        data: val_y
      }]
    };
    ch2 = new Chart(name, {
      type: "bar",
      data: chartDataGas,
      options: {
        legend: { display: false },
        title: { display: true },
        scales: {
          yAxes: [{
            ticks: { beginAtZero: true }
          }]
        }
      }
    });
    return ch2;
  } else if (name === "Chart3") { // Create a new chart for the fire sensor
    var chartDataFire = {
      labels: values_x,
      datasets: [{
        backgroundColor: coluomns_colors,
        data: val_y
      }]
    };
    ch3 = new Chart(name, {
      type: "bar",
      data: chartDataFire,
      options: {
        legend: { display: false },
        title: { display: true },
        scales: {
          yAxes: [{
            ticks: { beginAtZero: true }
          }]
        }
      }
    });
    return ch3;
  } else if (name === "Chart4") { // Create a new chart for the gas sensor
    var chartDataGas = {
      labels: values_x,
      datasets: [{
        backgroundColor: colors[0],
        data: val_y
      }]
    };
    ch4 = new Chart(name, {
      type: "bar",
      data: chartDataGas,
      options: {
        legend: { display: false },
        title: { display: true },
        scales: {
          yAxes: [{
            ticks: { beginAtZero: true }
          }]
        }
      }
    });
    return ch4;
  }
}




function callAPI(){
  var headers = new Headers();
  var requestOptions = {
      method: 'GET',
      headers: headers,
  };
  
  fetch(config.api_endpoint, requestOptions)
  .then(response => response.text()).then(result => displayDataOnChart(result))
}

function init() {
  ch1 = createPlot("Chart1", values_y);
  ch2 = createPlot("Chart2", values_y_gas);
  ch3 = createPlot("Chart3", values_y); // Create the fire sensor chart
  ch4 = createPlot("Chart4", values_y_gas); // Create the gas sensor chart
  callAPI();
  setInterval(callAPI, 60000); // Refresh the data every minute
}


init();
