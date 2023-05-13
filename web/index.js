var values_x = [];
var coluomns_colors = []; 
var colors = ["blue", "red", "black"]
var current_color = 0;
var values_y = [];
var ch1;
var input = [];
   
function createPlot(name, val_y){

    ch = new Chart(name, {
        type: "bar",
        data: {
            labels: values_x,
            datasets: [{
            backgroundColor: coluomns_colors,
            data: val_y
            }]
        },
        options: {
            legend: {display: false},
            title: {
            display: true,
            },
            scales: {
                yAxes: [{
                  ticks: {
                    beginAtZero: true
                  }
                }],
              }

        }
        });
    return ch;


}

function init(){   
    
    ch1=createPlot("Chart1", values_y);

}
