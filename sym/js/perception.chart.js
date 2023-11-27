class PerceptionChart {
  constructor(containerId, width, height) {
    this.containerId = containerId;
    this.canvas = document.createElement('canvas');
    this.canvas.width = width;
    this.canvas.height = height;

    let container = document.getElementById(containerId);
    container.appendChild(this.canvas);
    this.context = this.canvas.getContext('2d');
    this.context.font = height + 'px serif';
  }

  update(state, action, reward) {
    this.context.clearRect(0, 0, this.canvas.width, this.canvas.height);

    const barWidth = (this.canvas.width - 30) / (state.length / 3 + 1);
    const barHeight = this.canvas.height / 3;

    let offsetX = 0;
    let offsetY = 0;


    const actionsMap = ['green', 'blue', 'red']
    const actionColor = actionsMap[action];

    for (const i in state) {
      let color = state[i] * 255;
      this.context.fillStyle = 'rgb(' + color + ', ' + color + ','  + color + ')';
      this.context.fillRect(offsetX, offsetY, barWidth, barHeight);
      this.context.fill();

      offsetY += barHeight;

      if (offsetY >= this.canvas.height) {
        offsetY = 0;
        offsetX += barWidth;
      }
    }
    this.context.fillStyle = actionColor;
    this.context.fillRect(offsetX, offsetY, barWidth, this.canvas.height);
    this.context.fill();
    this.context.fillStyle = 'blue';
    this.context.fillText(reward, this.canvas.width - 28, this.canvas.height - 2);
  }
}