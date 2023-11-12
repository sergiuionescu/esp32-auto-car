class Car {
  constructor(collisionBoxes) {

    this.carLength = 50;
    this.carWidth = this.carLength / 5 * 3;

    let Bodies = Matter.Bodies,
      Body = Matter.Body,
      activeSensorColor = {
        render: {
          fillStyle: 'red', strokeStyle: 'blue', lineWidth: 3
        }
      }, carX = 250, carY = 50,
      carBody = Bodies.rectangle(carX, carY, this.carLength, this.carWidth), sensorRadius = this.carLength / 15,
      carWheelFL = Bodies.rectangle(carX - this.carLength / 2 + this.carLength / 3.5, carY - this.carWidth / 2, this.carLength / 3.5, this.carWidth / 3.5),
      carWheelBL = Bodies.rectangle(carX + this.carLength / 2 - this.carLength / 3.5, carY - this.carWidth / 2, this.carLength / 3.5, this.carWidth / 3.5),
      carWheelFR = Bodies.rectangle(carX - this.carLength / 2 + this.carLength / 3.5, carY + this.carWidth / 2, this.carLength / 3.5, this.carWidth / 3.5),
      carWheelBR = Bodies.rectangle(carX + this.carLength / 2 - this.carLength / 3.5, carY + this.carWidth / 2, this.carLength / 3.5, this.carWidth / 3.5),
      sensorBM = Bodies.circle(carX - this.carLength / 2 + this.carLength / 15, carY, sensorRadius),
      sensorFM = Bodies.circle(carX + this.carLength / 2 - this.carLength / 15, carY, sensorRadius, activeSensorColor),
      sensorFL = Bodies.circle(carX + this.carLength / 2 - this.carLength / 15, carY - this.carWidth / 2 + this.carLength * 2 / 15, sensorRadius, activeSensorColor),
      sensorFR = Bodies.circle(carX + this.carLength / 2 - this.carLength / 15, carY + this.carWidth / 2 - this.carLength * 2 / 15, sensorRadius, activeSensorColor);


    this.collisions = 0;
    this.carBody = Body.create({
      parts: [carBody, carWheelFL, carWheelBL, carWheelFR, carWheelBR, sensorBM, sensorFM, sensorFL, sensorFR]
    });
    this.carBody.frictionAir = 0.1

    this.colliderBM = new Collider(collisionBoxes, this.carBody, sensorBM, 'sensorFM', Math.PI);
    this.colliderFM = new Collider(collisionBoxes, this.carBody, sensorFM, 'sensorFM', 0);
    this.colliderFL = new Collider(collisionBoxes, this.carBody, sensorFL, 'sensorFL', -Math.PI / 6);
    this.colliderFR = new Collider(collisionBoxes, this.carBody, sensorFR, 'sensorFR', Math.PI / 6);
  }

}