const config = {
  api_endpoint: 'https://u50ui9vcia.execute-api.us-east-1.amazonaws.com/beta/flamex_data'
};
var values_x = [];
var coluomns_colors = []; 
var colors = ["blue", "red", "black"]
var current_color = 0;
var values_y = [];
var ch1;
var ch2;
var values_y_gas = [];
var coluomns_colors_gas = [];
var input = [];
var input2 = [];


function displayDataOnChart(data_input){
  console.log(data_input);
  const parsed_data = JSON.parse(data_input);

  console.log(parsed_data);
  for (let i = 0; i < parsed_data.length; i++) {
    coluomns_colors.push(colors[1]);
    const sampleTime = new Date(parsed_data[i]["sample_time"]);
    label = sampleTime.toLocaleString(); // Convert sample time to Locale format

    values_x.push(label); // Add the label to the values_x array

    ch1.data.datasets[0].data.push(parsed_data[i].device_data.flame);
    ch1.update();
    
    ch2.data.datasets[0].data.push(parsed_data[i].device_data.gas);
    ch2.update();
    
    input.push([parsed_data[i].sample_time, parsed_data[i].device_data.flame]);
    input2.push([parsed_data[i].sample_time, parsed_data[i].device_data.gas]);
  }
  

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
        backgroundColor: colors[2], // Use a different color for the gas chart
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

function init(){   
    
  ch1=createPlot("Chart1", values_y);
  ch2=createPlot("Chart2", values_y_gas);
  callAPI();
}
