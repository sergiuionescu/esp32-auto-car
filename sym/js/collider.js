class Collider {
  constructor(boxes, car, sensor, name, angleOffset, sensorRange = 200) {
    this.boxes = boxes.slice();
    this.car = car;
    this.sensor = sensor;
    this.name = name;
    this.angleOffset = angleOffset;
    this.sensorRange = sensorRange;
  }

  getSensorDistance() {
    let sensorReading = this.sensorRange;
    let raycastCollisions = raycast(this.boxes, this.sensor.position,
      {
        x: this.sensor.position.x + this.sensorRange * Math.cos(this.car.angle + this.angleOffset),
        y: this.sensor.position.y + this.sensorRange * Math.sin(this.car.angle + this.angleOffset)
      }
    );
    if (raycastCollisions.length > 0) {
      let firstHit = raycastCollisions[0];
      sensorReading = Math.sqrt(
        Math.pow(this.sensor.position.x - firstHit.point.x, 2)
        + Math.pow(this.sensor.position.y - firstHit.point.y, 2)
      );
    }
    return sensorReading;
  }
}