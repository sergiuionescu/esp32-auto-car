const ACTION_FORWARD = 0;
const ACTION_RIGHT = 1;
const ACTION_LEFT = 2;
const ACTION_BACK = 3;

const actions = [ACTION_FORWARD, ACTION_RIGHT, ACTION_LEFT, ACTION_BACK]

class Car {
  constructor(environment, collisionBoxes, actorCritic, position = {x: 100, y: 175}, pushToChart = false) {

    this.environment = environment;
    this.originalPosition = position;
    this.carLength = 25;
    this.carWidth = this.carLength / 5 * 3;
    this.actorCritic = actorCritic;
    this.maxDistance = 100;
    this.previousState = this.state = [];
    this.previousAction = ACTION_FORWARD;
    this.actionBeforePrevious = ACTION_FORWARD;
    this.previousAgentAction = ACTION_FORWARD;
    this.pushToChart = pushToChart;
    this.perceptionChart = new PerceptionChart(environment.name, 350, 15);

    let Bodies = Matter.Bodies,
      Body = Matter.Body,
      activeSensorColor = {
        render: {
          fillStyle: 'red', strokeStyle: 'blue', lineWidth: 3
        }
      }, carX = position.x, carY = position.y,
      carBody = Bodies.rectangle(carX, carY, this.carLength, this.carWidth), sensorRadius = this.carLength / 15,
      carWheelFL = Bodies.rectangle(carX - this.carLength / 2 + this.carLength / 3.5, carY - this.carWidth / 2, this.carLength / 3.5, this.carWidth / 3.5),
      carWheelBL = Bodies.rectangle(carX + this.carLength / 2 - this.carLength / 3.5, carY - this.carWidth / 2, this.carLength / 3.5, this.carWidth / 3.5),
      carWheelFR = Bodies.rectangle(carX - this.carLength / 2 + this.carLength / 3.5, carY + this.carWidth / 2, this.carLength / 3.5, this.carWidth / 3.5),
      carWheelBR = Bodies.rectangle(carX + this.carLength / 2 - this.carLength / 3.5, carY + this.carWidth / 2, this.carLength / 3.5, this.carWidth / 3.5);

    this.sensorBL = Bodies.circle(carX - this.carLength / 2 - this.carLength / 15 + 2 * sensorRadius, carY - this.carWidth / 2 + this.carLength * 2 / 15, sensorRadius);
    this.sensorBR = Bodies.circle(carX - this.carLength / 2 - this.carLength / 15 + 2 * sensorRadius, carY + this.carWidth / 2 - this.carLength * 2 / 15, sensorRadius);
    this.sensorFM = Bodies.circle(carX + this.carLength / 2 - this.carLength / 15, carY, sensorRadius, activeSensorColor);
    this.sensorFL = Bodies.circle(carX + this.carLength / 2 - this.carLength / 15, carY - this.carWidth / 2 + this.carLength * 2 / 15, sensorRadius, activeSensorColor);
    this.sensorFR = Bodies.circle(carX + this.carLength / 2 - this.carLength / 15, carY + this.carWidth / 2 - this.carLength * 2 / 15, sensorRadius, activeSensorColor);

    this.carBody = Body.create({
      parts: [carBody, carWheelFL, carWheelBL, carWheelFR, carWheelBR, this.sensorBL, this.sensorBR, this.sensorFM, this.sensorFL, this.sensorFR]
    });
    this.carBody.frictionAir = 0.1;

    this.reset(collisionBoxes);
  }

  getState() {
    let sensorDistanceBL = parseInt(this.colliderBL.getSensorDistance());
    let sensorDistanceBR = parseInt(this.colliderBR.getSensorDistance());
      let sensorDistanceFM = parseInt(this.colliderFM.getSensorDistance());
      let sensorDistanceFL = parseInt(this.colliderFL.getSensorDistance());
      let sensorDistanceFR = parseInt(this.colliderFR.getSensorDistance());

    return [sensorDistanceBL, sensorDistanceFL, sensorDistanceFM, sensorDistanceFR, sensorDistanceBR];
  }
  act(state, reward) {
    this.state.push(state);
    this.state.shift();

    let {action, agentAction} = actorCritic.getAction(this.state, actions, this.pushToChart);

    this.perceptionChart.update(normalizer.normalizeFeatures(this.previousState), this.previousAgentAction, reward);

    if (this.previousState.length > 0) {
      this.actorCritic.bufferReplay([...this.previousState], this.previousAction, reward, [...this.state], this.pushToChart);
    }

    this.previousState = [...this.state];

    this.applyAction(action);
    this.actionBeforePrevious = this.previousAction;
    this.previousAction = action;
    this.previousAgentAction = agentAction;

    return action;
  }

  applyAction(action) {
    this.previousPosition = {...this.carBody.position};
    let multiplier = this.carLength / 200;

    let leftSpeed = 0;
    let rightSpeed = 0;
    switch (action) {
      case ACTION_FORWARD:
        leftSpeed = 200;
        rightSpeed = 200;
        break;
      case ACTION_RIGHT:
        leftSpeed = 200;
        rightSpeed = -200;
        break;
      case ACTION_LEFT:
        leftSpeed = -200;
        rightSpeed = 200;
        break;
      case ACTION_BACK:
        leftSpeed = -200;
        rightSpeed = -200;
    }

    let leftForce = leftSpeed / 40000 * multiplier;
    let rightForce = rightSpeed / 40000 * multiplier;

    Matter.Body.applyForce(this.carBody, {
      x: this.carBody.position.x + this.carLength * Math.sin(this.carBody.angle) / 2,
      y: this.carBody.position.y - this.carWidth * Math.cos(this.carBody.angle) / 2
    }, {x: leftForce * Math.cos(this.carBody.angle), y: leftForce * Math.sin(this.carBody.angle)});
    Matter.Body.applyForce(this.carBody, {
      x: this.carBody.position.x - this.carLength * Math.sin(this.carBody.angle) / 2,
      y: this.carBody.position.y + this.carWidth * Math.cos(this.carBody.angle) / 2
    }, {x: rightForce * Math.cos(this.carBody.angle), y: rightForce * Math.sin(this.carBody.angle)});
  }

  reset (collisionBoxes) {
    Matter.Body.setPosition(this.carBody, this.originalPosition);
    Matter.Body.setAngle(this.carBody, 0);
    Matter.Body.setSpeed(this.carBody, 0);

    this.previousPosition = {...this.carBody.position};

    this.state = [];
    for (let i = 0; i < config.activeHistoryLength; i++) {
      this.state.push([this.maxDistance, this.maxDistance, this.maxDistance, this.maxDistance, this.maxDistance]);
    }

    this.collisions = 0;

    this.colliderBL = new Collider(collisionBoxes, this.carBody, this.sensorBL, 'sensorBL', Math.PI * 3, 100);
    this.colliderBR = new Collider(collisionBoxes, this.carBody, this.sensorBR, 'sensorBR', -Math.PI * 3, 100);
    this.colliderFM = new Collider(collisionBoxes, this.carBody, this.sensorFM, 'sensorFM', 0, 100);
    this.colliderFL = new Collider(collisionBoxes, this.carBody, this.sensorFL, 'sensorFL', -Math.PI / 6, 100);
    this.colliderFR = new Collider(collisionBoxes, this.carBody, this.sensorFR, 'sensorFR', Math.PI / 6, 100);
  }
}