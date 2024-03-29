class RewardEnvironment {
  constructor() {
    this.previousAction = ACTION_FORWARD;
    this.maxReward = 0;
    this.previousMinDistance = 100;
  }

  getReward(car) {
    let reward = 300;
    let sensorDistanceBL = parseInt(car.colliderBL.getSensorDistance());
    let sensorDistanceBR = parseInt(car.colliderBR.getSensorDistance());
    let sensorDistanceFM = parseInt(car.colliderFM.getSensorDistance());
    let sensorDistanceFL = parseInt(car.colliderFL.getSensorDistance());
    let sensorDistanceFR = parseInt(car.colliderFR.getSensorDistance());

    let minVisibleDistance = Math.min(sensorDistanceFM, sensorDistanceFL, sensorDistanceFR);
    if (car.previousAction === ACTION_FORWARD && minVisibleDistance > config.proximityThreshold) {
      reward += 3 * car.maxDistance;
    }

    reward -= car.maxDistance - sensorDistanceFM;
    reward -= car.maxDistance - sensorDistanceFL;
    reward -= car.maxDistance - sensorDistanceFR;

    if (reward > this.maxReward) {
      this.maxReward = reward;
    }

    this.previousMinDistance = minVisibleDistance;

    return Math.round(reward/8)/100;
  }
}