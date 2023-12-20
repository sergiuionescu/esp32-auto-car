class ActorCritic {
  constructor(featureCount, historyLength, actionSize, reset, test, config) {
    this.render = false;
    this.featureCount = featureCount;
    this.historyLength = historyLength;
    this.actionSize = actionSize;
    this.replayBuffer = [];
    this.stateAdvantages = {};
    this.trainingBufferKeys = {};
    this.valueSize = 1;
    this.test = test;

    this.episode = 0;
    this.averageReward = 0;

    this.config = config;

    if (!reset) {
      this.load();
    }

    this.commonLayer = tf.layers.dense({
      units: parseInt(this.config.hiddenUnits),
      activation: 'relu',
      kernelInitializer: 'glorotUniform',
    });
    this.actor = this.actor ?? this.buildActor();
    this.critic = this.critic ?? this.buildCritic();
    this.name = this.name ?? new Date().toLocaleString('en-in');
    this.trainingBuffer = {
      'tfState': [],
      'advantages': [],
      'targets': []
    };

    this.step = 0;
    this.latestActorLoss = 0;
    this.latestCriticLoss = 0;

    this.actorLosses = [];
    this.criticLosses = [];
    this.rewards = [];
    this.testRewards = [];
  }

  customActorLoss(logProb, advantages) {
    return tf.tidy(() => {
      // Multiply the log probability by the advantages
      const loss = tf.mul(logProb, advantages);

      // Take the negative of the mean (since we want to maximize expected returns)
      const negLoss = tf.neg(tf.mean(loss));
      return negLoss;
    });
  }

  buildActor() {
    const model = tf.sequential();
    model.add(tf.layers.inputLayer({inputShape: [this.historyLength * this.featureCount],}));

    model.add(this.commonLayer);

    model.add(tf.layers.dense({
      units: this.actionSize,
      activation: 'softmax',
      kernelInitializer: 'glorotUniform',
    }));

    this.compile(model, config.actorLearningRate, this.customActorLoss);

    return model;
  }

  buildCritic() {
    const model = tf.sequential();

    model.add(tf.layers.inputLayer({inputShape: [this.historyLength * this.featureCount],}));

    model.add(this.commonLayer);

    model.add(tf.layers.dense({
      units: this.valueSize,
      activation: 'linear',
      kernelInitializer: 'glorotUniform',
    }));

    this.compile(model, config.criticLearningRate, tf.losses.huberLoss);

    return model;
  }

  compile(model, learningRate, loss) {
    model.compile({
      optimizer: tf.train.adam(learningRate),
      loss: loss,
    });
  }

  getAction(state, actions, pushToChart = false) {
    let normalizeFeatures = normalizer.normalizeFeatures(state);
    let stateTensor = tf.tensor2d(normalizeFeatures, [1, state.length * state[0].length]);
    let policy = this.actor.predict(stateTensor, {
      batchSize: 1,
    });

    let policyFlat = policy.dataSync();
    let weights = [parseFloat(policyFlat[ACTION_FORWARD]), parseFloat(policyFlat[ACTION_RIGHT]), parseFloat(policyFlat[ACTION_LEFT]), parseFloat(policyFlat[ACTION_BACK])];
    if (pushToChart) {
      pushToWeightsChart(this.step, weights);
    }

    let action = chance.weighted(actions, weights);
    let agentAction = action;

    if (Math.random() < this.config.epsilon && !this.test) {
      action = chance.weighted(actions, [1, 1, 1, 0.2]);
    }

    policy.dispose();
    stateTensor.dispose();

    return {action, agentAction};
  }

  bufferReplay(previousState, previousAction, reward, state, pushToChart) {
    if (this.test) {
      this.testRewards.push(reward);
    }
    this.replayBuffer.push({'ps': previousState, 'pa': previousAction, 'r': reward, 's': state});
    this.computeTrainingData(previousState, state, reward, pushToChart);

    this.step++;
  }

  convertRawTrainingHistory() {
    this.replayBuffer.forEach(async (data) => {
      await actorCritic.bufferTrainingData(data.ps, data.pa, data.r, data.s);
    })
  }

  async bufferTrainingData(state, action, reward, nextState) {
    let advantages = new Array(this.actionSize).fill(0);
    let {normalizedState, target, advantage} = this.computeTrainingData(state, nextState, reward, false);

    const stateKey = JSON.stringify(normalizedState);
    if (stateKey in this.stateAdvantages) {
      advantages = this.stateAdvantages[stateKey];
    }

    advantages[action] = advantage;

    this.stateAdvantages[stateKey] = advantages;

    let count = 0;
    for (advantage of advantages) {
      if (advantage > 0) {
        count++;
      }
    }
    if (count > 1) {
      console.log(advantages, stateKey);
    }

    if (!(stateKey in this.trainingBufferKeys)) {
      this.trainingBuffer['tfState'].push(normalizedState);
      this.trainingBuffer['advantages'].push(advantages);
      this.trainingBuffer['targets'].push(target);
    }
    this.trainingBufferKeys[stateKey] = true;


    this.rewards.push(reward);
  }

  computeTrainingData(previousState, state, reward, pushToChart = true) {
    let normalizedPreviousState = normalizer.normalizeFeatures(previousState);
    let tfPreviousState = tf.tensor2d(normalizedPreviousState, [1, normalizedPreviousState.length]);
    const tfPredictedPreviousStateValue =  this.critic.predict(tfPreviousState);
    let predictedPreviousStateValue = tfPredictedPreviousStateValue.dataSync();

    let normalizedState = normalizer.normalizeFeatures(state);
    let tfState = tf.tensor2d(normalizedState, [1, normalizedState.length]);
    let tfPredictedStateValue = this.critic.predict(tfState);
    let predictedStateValue = tfPredictedStateValue.dataSync();

    let target = reward + config.discountFactor * predictedStateValue;
    let advantage = target - predictedPreviousStateValue;

    if (pushToChart) {
      pushToRewardChart(actorCritic.step, reward, parseFloat(predictedPreviousStateValue), parseFloat(predictedStateValue), advantage, target);
    }

    tfPreviousState.dispose();
    tfPredictedPreviousStateValue.dispose();
    tfState.dispose();
    tfPredictedStateValue.dispose();

    return {normalizedState: normalizedPreviousState, target, advantage};
  }

  async trainModel() {
    let epochs = 200;
    let batchSize = 16;

    let tfState = tf.tensor2d(
      this.trainingBuffer['tfState'],
      [this.trainingBuffer['tfState'].length, this.trainingBuffer['tfState'][0].length]
    );
    let advantages = tf.tensor(this.trainingBuffer['advantages']);
    let targets = tf.tensor(this.trainingBuffer['targets']);

    resetHistoryChart();

    function onActorEpochEnd(batch, logs) {
      pushToHistoryChart(logs.loss, 'actor')
    }

    function onCriticEpochEnd(batch, logs) {
      pushToHistoryChart(logs.loss, 'critic')
    }

    await this.actor.fit(tfState, advantages, {
      epochs: epochs,
      batchSize: batchSize,
      callbacks: [
        new tf.CustomCallback({onEpochEnd: onActorEpochEnd}),
        tf.callbacks.earlyStopping({monitor: 'loss', verbose: 2})
      ]
    }).then(info => {
        this.latestActorLoss = info.history.loss.slice(-1);
        this.actorLosses.push(this.latestActorLoss[0]);
      }
    );

    await this.critic.fit(tfState, targets, {
      epochs: epochs,
      batchSize: batchSize,
      callbacks: [
        new tf.CustomCallback({onEpochEnd: onCriticEpochEnd}),
        tf.callbacks.earlyStopping({monitor: 'loss', verbose: 2})
      ]
    }).then(info => {
        this.latestCriticLoss = info.history.loss.slice(-1);
        this.criticLosses.push(this.latestCriticLoss[0]);
      }
    );

    tfState.dispose();
    advantages.dispose()
    targets.dispose();
  }

  async save() {
    await this.actor.save('localstorage://actor');
    await this.critic.save('localstorage://critic');

    this.averageReward = (this.averageReward * this.episode + reward) / (this.episode + 1);
    this.episode++;

    let stateAdvantagesSize = parseInt(3000000 / JSON.stringify(Object.entries(this.stateAdvantages)[0]).length);
    localStorage.setItem('stateAdvantages', JSON.stringify(Object.fromEntries(Object.entries(this.stateAdvantages).slice(-stateAdvantagesSize))));
    localStorage.setItem('episode', this.episode);
    localStorage.setItem('averageReward', this.averageReward);
    localStorage.setItem('name', this.name);
  }

  async load(actorPath = 'localstorage://actor', criticPath = 'localstorage://critic', fromLocalStorage = true) {
    let name = fromLocalStorage ? (localStorage.getItem('name') ?? this.name) : config.name;

    if (!fromLocalStorage || localStorage.getItem('name')) {
      tf.loadLayersModel(actorPath).then(model => {
        this.compile(model, config.actorLearningRate, this.customActorLoss);
        this.actor = model;
      });
      tf.loadLayersModel(criticPath).then(model => {
        model.layers[1] = this.actor.layers[1];
        this.compile(model, config.criticLearningRate, tf.losses.huberLoss);
        this.critic = model;
      });
      this.name = name;
      this.episode = fromLocalStorage ? (localStorage.getItem('episode') ?? 0) : config.episode;
      this.replayBuffer = [];
      this.stateAdvantages = JSON.parse(localStorage.getItem('stateAdvantages') ?? '{}');
      this.averageReward = localStorage.getItem('averageReward') ?? 0;
      document.getElementById('episode').innerText = this.episode;
      document.getElementById('name').innerHTML = this.name;
    }
  }

  getAverageActorLoss() {
    return this.getAverage(this.actorLosses);
  }

  getAverageCriticLoss() {
    return this.getAverage(this.criticLosses);
  }

  getAverageReward() {
    return this.getAverage(this.rewards);
  }

  getAverageTestReward() {
    return this.getAverage(this.testRewards);
  }

  getTotalTrainReward() {
    let sum = 0;
    for(let reward of this.rewards) {
      sum += reward;
    }

    return sum;
  }

  getTotalTestReward() {
    let sum = 0;
    for(let reward of this.testRewards) {
      sum += reward;
    }

    return sum;
  }

  getAverage(list) {
    let sum = 0;
    for (let i = 0; i < list.length; i++) {
      sum += list[i];
    }
    return sum / list.length;
  }

  reset() {
    this.replayBuffer = [];
    this.trainingBufferKeys = {};
    this.rewards = [];
    this.testRewards = [];
    this.step = 0;
    this.test = !this.test;
    this.trainingBuffer = {
      'tfState': [],
      'advantages': [],
      'targets': []
    };
  }
}