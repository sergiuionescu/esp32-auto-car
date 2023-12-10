class BasicEnvironment {
  constructor(containerId, getObstacles, carPosition = null, isMaster = false, environments = []) {
    this.multiplier = 1;
    this.rewardEnvironment = new RewardEnvironment();
    this.isMaster = isMaster;
    this.name = containerId;
    this.done = false;
    this.environments = environments
    this.getObstacles = getObstacles;
    this.obstacles = [];
    this.timeouts = [];

    let Engine = Matter.Engine,
      Render = Matter.Render,
      World = Matter.World,
      Events = Matter.Events,
      Bodies = Matter.Bodies;

    // create an engine
    this.engine = Engine.create();
    this.engine.world.gravity.y = 0;

    // create a renderer
    let environmentWidth = 500;
    let environmentHeight = 350;
    if(carPosition === null) {
      carPosition = {x: 100, y:environmentHeight/2};
    }
    let render = Render.create({
      element: document.getElementById(containerId),
      engine: this.engine,
      options: {
        showAngleIndicator: false,
        showDebug: false,
        showVelocity: false,
        showCollisions: false,
        wireframes: false,
        width: environmentWidth,
        height: environmentHeight
      }
    });

    let boxes = this.createObstacles();
    let wallB = Bodies.rectangle(environmentWidth / 2, environmentHeight, environmentWidth + 10, 20, {isStatic: true});
    let wallT = Bodies.rectangle(environmentWidth / 2, 0, environmentWidth + 10, 20, {isStatic: true});
    let wallL = Bodies.rectangle(0, 205, 20, environmentWidth + 10, {isStatic: true});
    let wallR = Bodies.rectangle(environmentWidth, 205, 20, environmentWidth + 10, {isStatic: true});
    boxes.push(wallB);
    boxes.push(wallT);
    boxes.push(wallL);
    boxes.push(wallR);

    this.car = new Car(this, boxes, actorCritic, carPosition,  this.isMaster);
    boxes.push(this.car.carBody);

    // add all the bodies to the world
    World.add(this.engine.world, boxes);

    Events.on(this.engine, 'collisionStart', (event) => {
      this.car.collisions++;
      if (this.car.collisions > 5) {
        this.done = true;
      }
    });

    // run the engine
    Engine.run(this.engine);

    // run the renderer
    Render.run(render);
  }

  createObstacles() {
    this.obstacles = [];
    let boxes = [];
    for (let box of this.getObstacles()) {
      this.obstacles.push(box);
      boxes.push(box);
    }

    return boxes;
  }

  async run() {
    this.timeouts.pop();
    let state = this.car.getState();
    reward = this.rewardEnvironment.getReward(this.car);
    this.car.act(state, reward);

    if (this.isMaster && (actorCritic.step > 2000 || this.done || reward < -10000)) {
      actorCritic.convertRawTrainingHistory();

      await actorCritic.trainModel();

      actorCritic.save().then(() => {
        historyData['averageActorLoss'].push(
          actorCritic.getAverageActorLoss()
        );
        historyData['averageCriticLoss'].push(
          actorCritic.getAverageCriticLoss()
        );
        historyData['averageReward'].push(
          actorCritic.getAverageReward()
        );
        if (historyData['averageActorLoss'].length > 50) {
          historyData['averageActorLoss'].shift();
          historyData['averageCriticLoss'].shift();
          historyData['averageReward'].shift();
        }
        localStorage.setItem('historyData', JSON.stringify(historyData));

        config.epsilon = Math.max(0, (200 - actorCritic.episode)/200);
        saveConfigToStorage();

        this.car.actorCritic.reset();

        for (let environment of this.environments) {
          environment.reset();
          environment.done = false;

          setTimeout(environment.run.bind(environment), 1000);
        }
        document.getElementById('episode').innerText = this.car.actorCritic.episode;
        updateConfigUi();
      });

    } else {
      if (!this.done) {
        this.timeouts.push(setTimeout(this.run.bind(this), 50 / this.multiplier));
      }
    }
  }

  reset() {
    this.car.reset();

    for(let timeout of this.timeouts) {
      clearTimeout(timeout);
    }

    for (let obstacle of this.obstacles) {
      Matter.Composite.remove(this.engine.world, obstacle);
    }
    let boxes = this.createObstacles();
    Matter.World.add(this.engine.world, boxes);
  }
}
