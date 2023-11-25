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
        },
        {
          label: 'predicted',
          yAxisID: 'y-axis-1',
          fill: false,
          borderColor: "orange",
          data: [],
        },
        {
          label: 'predicted next',
          yAxisID: 'y-axis-1',
          fill: false,
          borderColor: "green",
          data: [],
        },
        {
          label: 'advantage',
          yAxisID: 'y-axis-1',
          fill: false,
          borderColor: "yellow",
          data: [],
          hidden: true,
        },
        {
          label: 'target',
          yAxisID: 'y-axis-1',
          fill: false,
          borderColor: "gray",
          data: [],
          hidden: true,
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

function pushToRewardChart(step, score, predicted, predictedNext, advantage, target) {
  rewardChart.data.labels.push(step);
  rewardChart.data.datasets[0].data.push(score);
  rewardChart.data.datasets[1].data.push(predicted);
  rewardChart.data.datasets[2].data.push(predictedNext);
  rewardChart.data.datasets[3].data.push(advantage);
  rewardChart.data.datasets[4].data.push(target);
  if (rewardChart.data.labels.length > 50) {
    rewardChart.data.labels.shift();
    rewardChart.data.datasets[0].data.shift();
    rewardChart.data.datasets[1].data.shift();
    rewardChart.data.datasets[2].data.shift();
    rewardChart.data.datasets[3].data.shift();
    rewardChart.data.datasets[4].data.shift();
  }
  rewardChart.update();
}