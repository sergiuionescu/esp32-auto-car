function createRewardChart() {
  var canvasContainer = document.getElementById('reward-chart');
  var canvas = document.createElement("CANVAS");
  canvasContainer.style.width = 350;
  canvasContainer.appendChild(canvas);
  rewardChart = new Chart(canvas, {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        {
          label: 'reward',
          yAxisID: 'y-axis-1',
          fill: false,
          backgroundColor: "#157041",
          borderColor: "blue",
          data: []
        }
      ]
    },
    options: {
      title: {
        display: true,
        text: 'Reward'
      },
      scales: {
        yAxes: [
          {
            type: "linear",
            display: true,
            position: "left",
            id: "y-axis-1"
          }
        ],
      }
    }
  });
}

function pushToRewardChart(step, score) {
  rewardChart.data.labels.push(step);
  rewardChart.data.datasets[0].data.push(score);
  if (rewardChart.data.labels.length > 50) {
    rewardChart.data.labels.shift();
    rewardChart.data.datasets[0].data.shift();
  }
  rewardChart.update();
}