  class Normalizer {
    constructor(scalingFactor = 20) {
      this.scalingFactor = scalingFactor;
    }
    normalizeSensorDistance(sensorDistance) {
      sensorDistance = Math.round(sensorDistance / 10);
      return sensorDistance / this.scalingFactor;
    }

    normalizeFeatures(features) {
      let normalized = [];
      features.forEach(function (feature, index) {
          normalized.push(this.normalizeSensorDistance(feature[0]));
          normalized.push(this.normalizeSensorDistance(feature[1]));
          normalized.push(this.normalizeSensorDistance(feature[2]));
          normalized.push(this.normalizeSensorDistance(feature[3]));
          normalized.push(this.normalizeSensorDistance(feature[4]));
      }, this);

      return normalized;
    }
  }