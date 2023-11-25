function createWeightsChart() {
  var canvasContainer = document.getElementById('weights');
  var canvas = document.createElement("CANVAS");
  canvasContainer.style.width = 350;
  canvasContainer.appendChild(canvas);
  weightsChart = new Chart(canvas, {
    type: 'bar',
    data: {
      labels: [],
      datasets: [
        {
          label: 'Right',
          fill: false,
          backgroundColor: "#1238c1",
          data: []
        },
        {
          label: 'Forward',
          fill: false,
          backgroundColor: "#16A085",
          data: []
        },
        {
          label: 'Left',
          fill: false,
          backgroundColor: "#E74C3C",
          data: []
        },
        {
          label: 'Back',
          fill: false,
          backgroundColor: "#e7cb3c",
          data: []
        }
      ]
    },
    options: {
      title: {
        display: true,
        text: 'Weights'
      },
      scales: {
        xAxes: [{
          stacked: true,
        }],
        yAxes: [{
          stacked: true
        }]
      }
    }
  });
}

function pushToWeightsChart(step, weights) {
  weightsChart.data.labels.push(step);
  weightsChart.data.datasets[0].data.push(weights[ACTION_RIGHT]);
  weightsChart.data.datasets[1].data.push(weights[ACTION_FORWARD]);
  weightsChart.data.datasets[2].data.push(weights[ACTION_LEFT]);
  weightsChart.data.datasets[3].data.push(weights[ACTION_BACK]);
  if (weightsChart.data.labels.length > 50) {
    weightsChart.data.labels.shift();
    weightsChart.data.datasets[0].data.shift();
    weightsChart.data.datasets[1].data.shift();
    weightsChart.data.datasets[2].data.shift();
    weightsChart.data.datasets[3].data.shift();
  }
  weightsChart.update();
}