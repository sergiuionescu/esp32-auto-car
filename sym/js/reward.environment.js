class RewardEnvironment {
  constructor() {
    this.previousAction = ACTION_FORWARD;
    this.maxReward = 0;
    this.previousMinDistance = 100;
  }

  getReward(car) {
    let reward = 0;
    let sensorDistanceBM = parseInt(car.colliderBM.getSensorDistance());
    let sensorDistanceFM = parseInt(car.colliderFM.getSensorDistance());
    let sensorDistanceFL = parseInt(car.colliderFL.getSensorDistance());
    let sensorDistanceFR = parseInt(car.colliderFR.getSensorDistance());

    let minVisibleDistance = Math.min(sensorDistanceFM, sensorDistanceFL, sensorDistanceFR);
    if (car.previousAction === ACTION_FORWARD && minVisibleDistance > config.proximityThreshold) {
      reward += car.maxDistance;
    }
    if (car.previousAction === ACTION_BACK) {
      reward -= sensorDistanceMax / 10;
    }
    reward -= car.maxDistance - sensorDistanceBM;
    reward -= car.maxDistance - sensorDistanceFM;
    reward -= car.maxDistance - sensorDistanceFL;
    reward -= car.maxDistance - sensorDistanceFR;

    if (reward > this.maxReward) {
      this.maxReward = reward;
    }

    this.previousMinDistance = minVisibleDistance;

    return reward;
  }
}