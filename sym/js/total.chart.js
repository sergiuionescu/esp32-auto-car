function createTotalChart(historyData) {
  var canvasContainer = document.getElementById('total-reward-container');
  var canvas = document.createElement("CANVAS");
  canvasContainer.style.width = 350;
  canvasContainer.appendChild(canvas);
  totalChart = new Chart(canvas, {
    type: 'line',
    data: {
      labels: Object.keys(historyData["totalTrainReward"]),
      datasets: [
        {
          label: 'Train',
          yAxisID: 'y-axis-1',
          fill: false,
          borderColor: "#1634a0",
          data: historyData['totalTrainReward']
        },
        {
          label: 'Test',
          yAxisID: 'y-axis-1',
          fill: false,
          borderColor: "#e7cb3c",
          data: historyData['totalTestReward']
        }
      ]
    },
    options: {
      title: {
        display: true,
        text: 'Total Reward'
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

function resetTotalChart() {
  totalChart.data.labels = [];
  totalChart.data.datasets[0].data = [];
  totalChart.data.datasets[1].data = [];

  totalChart.update();
}

function updateTotalChart(historyData) {
  resetTotalChart();

  if(!('totalTrainReward' in historyData)) {
    return;
  }

  totalChart.data.labels = Object.keys(historyData["averageActorLoss"]);
  totalChart.data.datasets[0].data = historyData['totalTrainReward'];
  totalChart.data.datasets[1].data = historyData['totalTestReward'];

  totalChart.update();
}