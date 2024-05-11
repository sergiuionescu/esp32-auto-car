function createAdvantagesChart() {
  var canvasContainer = document.getElementById('advantages-chart');
  var canvas = document.createElement("CANVAS");
  canvasContainer.style.width = 350;
  canvasContainer.appendChild(canvas);
  advantagesChart = new Chart(canvas, {
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
        text: 'Advantages'
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

function pushToAdvantagesChart(step, advantages) {
  advantagesChart.data.labels.push(step);
  advantagesChart.data.datasets[0].data.push(advantages[ACTION_RIGHT]);
  advantagesChart.data.datasets[1].data.push(advantages[ACTION_FORWARD]);
  advantagesChart.data.datasets[2].data.push(advantages[ACTION_LEFT]);
  advantagesChart.data.datasets[3].data.push(advantages[ACTION_BACK]);
  if (advantagesChart.data.labels.length > 50) {
    advantagesChart.data.labels.shift();
    advantagesChart.data.datasets[0].data.shift();
    advantagesChart.data.datasets[1].data.shift();
    advantagesChart.data.datasets[2].data.shift();
    advantagesChart.data.datasets[3].data.shift();
  }
  advantagesChart.update();
}