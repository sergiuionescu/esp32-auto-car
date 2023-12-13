function createHistoryChart(historyData) {
  var canvasContainer = document.getElementById('training-history-container');
  var canvas = document.createElement("CANVAS");
  canvasContainer.style.width = 350;
  canvasContainer.appendChild(canvas);
  historyChart = new Chart(canvas, {
    type: 'line',
    data: {
      labels: Object.keys(historyData["averageActorLoss"]),
      datasets: [
        {
          label: 'Actor loss',
          yAxisID: 'y-axis-1',
          fill: false,
          borderColor: "#E74C3C",
          data: historyData['averageActorLoss']
        },
        {
          label: 'Critic loss',
          yAxisID: 'y-axis-1',
          fill: false,
          borderColor: "#16A085",
          data: historyData['averageCriticLoss']
        },
        {
          label: 'Reward',
          yAxisID: 'y-axis-1',
          fill: false,
          borderColor: "#1634a0",
          data: historyData['averageReward']
        },
        {
          label: 'Test Reward',
          yAxisID: 'y-axis-1',
          fill: false,
          borderColor: "#e7cb3c",
          data: historyData['averageTestReward']
        }
      ]
    },
    options: {
      title: {
        display: true,
        text: 'Evolution average history'
      },
      scales: {
        yAxes: [
          {
            type: "linear",
            display: true,
            position: "left",
            id: "y-axis-1",
          }
        ],
      }
    }
  });
}

function resetHistoryChart() {
  historyChart.data.labels = [];
  historyChart.data.datasets[0].data = [];
  historyChart.data.datasets[1].data = [];
  historyChart.data.datasets[2].data = [];
  historyChart.data.datasets[3].data = [];

  historyChart.update();
}

function updateHistoryChart(historyData) {
  resetHistoryChart();

  if(!('averageActorLoss' in historyData)) {
    return;
  }

  historyChart.data.labels = Object.keys(historyData["averageActorLoss"]);
  historyChart.data.datasets[0].data = historyData['averageActorLoss'];
  historyChart.data.datasets[1].data = historyData['averageCriticLoss'];
  historyChart.data.datasets[2].data = historyData['averageReward'];
  historyChart.data.datasets[3].data = historyData['averageTestReward'];

  historyChart.update();
}

function pushToHistoryChart(loss, type) {
  if (type === 'actor') {
    historyChart.data.datasets[0].data.push(loss);
    if (historyChart.data.datasets[0].data.length > historyChart.data.labels.length) {
      historyChart.data.labels.push(historyChart.data.datasets[0].data.length);
    }
  } else {
    historyChart.data.datasets[1].data.push(loss);
    if (historyChart.data.datasets[1].data.length > historyChart.data.labels.length) {
      historyChart.data.labels.push(historyChart.data.datasets[1].data.length);
    }
  }
  historyChart.update();
}