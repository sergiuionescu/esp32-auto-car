class BasicEnvironment {
  constructor(containerId, additionalObstacles = [], isMaster = false) {
    this.multiplier = 1;
    this.rewardEnvironment = new RewardEnvironment();
    this.isMaster = isMaster;

    let Engine = Matter.Engine,
      Render = Matter.Render,
      World = Matter.World,
      Events = Matter.Events,
      Bodies = Matter.Bodies;

    // create an engine
    let engine = Engine.create();
    engine.world.gravity.y = 0;

    // create a renderer
    let environmentWidth = 500;
    let environmentHeight = 350;
    let render = Render.create({
      element: document.getElementById(containerId),
      engine: engine,
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

    // create two boxes and a ground
    let boxes = [];

    for(let box of additionalObstacles) {
      boxes.push(box);
    }
    let wallB = Bodies.rectangle(environmentWidth / 2, environmentHeight, environmentWidth + 10, 20, {isStatic: true});
    let wallT = Bodies.rectangle(environmentWidth / 2, 0, environmentWidth + 10, 20, {isStatic: true});
    let wallL = Bodies.rectangle(0, 205, 20, environmentWidth + 10, {isStatic: true});
    let wallR = Bodies.rectangle(environmentWidth, 205, 20, environmentWidth + 10, {isStatic: true});
    boxes.push(wallB);
    boxes.push(wallT);
    boxes.push(wallL);
    boxes.push(wallR);

    this.car = new Car(boxes, actorCritic, this.isMaster);
    boxes.push(this.car.carBody);

    // add all the bodies to the world
    World.add(engine.world, boxes);

    Events.on(engine, 'collisionStart', function (event) {
      this.car.collisions++;
    });
    Events.on(engine, 'collisionActive', function (event) {
      reward += config.collisionPenalty;
    })

    // run the engine
    Engine.run(engine);

    // run the renderer
    Render.run(render);
  }

  async run() {
    let done = false;

    let state = this.car.getState();
    reward = this.rewardEnvironment.getReward(this.car);
    this.car.act(state, reward);

    if (this.isMaster && (actorCritic.step > 400 || done || reward < -10000)) {
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

        config.epsilon = Math.max(Math.min(1, (400 - actorCritic.step) * (1 - config.minEpsilon) / 400), config.minEpsilon);
        saveConfigToStorage();

        window.location.href = window.location.href.split('#')[0];
      });

    } else {
      setTimeout(this.run.bind(this), 50 / this.multiplier);
    }
  }
}
