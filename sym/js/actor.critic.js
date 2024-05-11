class ActorCritic {
  constructor(featureCount, historyLength, actionSize, reset, test, config) {
    this.render = false;
    this.featureCount = featureCount;
    this.historyLength = historyLength;
    this.actionSize = actionSize;
    this.replayBuffer = [];
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
      'tfActionState': [],
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

    model.add(tf.layers.dropout(0.3))
    model.add(tf.layers.dense({
      units: parseInt(this.config.hiddenUnits),
      activation: 'relu',
      kernelInitializer: 'glorotUniform',
    }));
    model.add(tf.layers.dropout(0.3))

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

    model.add(tf.layers.inputLayer({inputShape: [this.historyLength * this.featureCount + 2],}));

    model.add(tf.layers.dropout(0.3))
    model.add(tf.layers.dense({
      units: parseInt(this.config.hiddenUnits),
      activation: 'relu',
      kernelInitializer: 'glorotUniform',
    }));
    model.add(tf.layers.dropout(0.3))

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

  getAction(state, actions, pushToChart = false, reward) {
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

    if (reward < this.config.boltzmannThreshold) {
      weights = this.getBoltzmannDistribution(weights);
    }
    let action = chance.weighted(actions, weights);
    let agentAction = action;

    policy.dispose();
    stateTensor.dispose();

    return {action, agentAction};
  }

  bufferReplay(previousState, previousAction, reward, state, pushToChart) {
    if (this.test) {
      this.testRewards.push(reward);
    }
    this.replayBuffer.push({'ps': previousState, 'pa': previousAction, 'r': reward, 's': state});
    this.computeTrainingData(previousState, previousAction, state, reward, pushToChart);

    this.step++;
  }

  convertRawTrainingHistory() {
    this.replayBuffer.forEach(async (data) => {
      await actorCritic.bufferTrainingData(data.ps, data.pa, data.r, data.s);
    })
  }

  async bufferTrainingData(state, action, reward, nextState) {
    let {normalizedState, normalizedActionState, targets, advantages} = this.computeTrainingData(state, action, nextState, reward, false);

    this.trainingBuffer['tfState'].push(normalizedState);
    this.trainingBuffer['advantages'].push(advantages);
    this.trainingBuffer['tfActionState'].push(normalizedActionState);
    this.trainingBuffer['targets'].push(targets[action]);

    this.rewards.push(reward);
  }

  computeTrainingData(previousState, previousAction, state, reward, pushToChart = true) {
    let normalizedPreviousState = normalizer.normalizeFeatures(previousState);
    let normalizedPreviousActionState = [...normalizedPreviousState, ...this.getActionOneHot(previousAction)];
    let tfPreviousActionState = tf.tensor2d(normalizedPreviousActionState, [1, normalizedPreviousActionState.length]);
    const tfPredictedPreviousStateValue =  this.critic.predict(tfPreviousActionState);
    let predictedPreviousStateValue = tfPredictedPreviousStateValue.dataSync();

    let normalizeStateFeatures = normalizer.normalizeFeatures(state);
    let normalizedActionStates = [];
    for (let potentialAction of [0, 1, 2, 3]) {
      normalizedActionStates.push([...normalizeStateFeatures, ...this.getActionOneHot(potentialAction)]);
    }
    let tfState = tf.tensor2d(normalizedActionStates, [normalizedActionStates.length, normalizedActionStates[0].length]);
    let tfPredictedActionStateValues = this.critic.predict(tfState);
    let predictedActionStateValues = tfPredictedActionStateValues.dataSync();

    let targets = [];
    for (let predictedActionStateValue of predictedActionStateValues) {
      targets.push(reward + config.discountFactor * predictedActionStateValue)
    }
    let advantages = [];
    for(let potentialAction in targets) {
      advantages.push(targets[potentialAction] - predictedPreviousStateValue);
    }

    if (pushToChart) {
      pushToAdvantagesChart(actorCritic.step, advantages);
      pushToRewardChart(actorCritic.step, reward);
    }

    tfPreviousActionState.dispose();
    tfPredictedPreviousStateValue.dispose();
    tfState.dispose();
    tfPredictedActionStateValues.dispose();

    return {normalizedState: normalizedPreviousState, normalizedActionState: normalizedPreviousActionState, targets, advantages};
  }

  async trainModel() {
    let epochs = 200;
    let batchSize = 16;

    let tfActionState = tf.tensor2d(
      this.trainingBuffer['tfActionState'],
      [this.trainingBuffer['tfActionState'].length, this.trainingBuffer['tfActionState'][0].length]
    );
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
        tf.callbacks.earlyStopping({monitor: 'loss', verbose: 2, patience: 10})
      ]
    }).then(info => {
        this.latestActorLoss = info.history.loss.slice(-1);
        this.actorLosses.push(this.latestActorLoss[0]);
      }
    );

    await this.critic.fit(tfActionState, targets, {
      epochs: epochs,
      batchSize: batchSize,
      callbacks: [
        new tf.CustomCallback({onEpochEnd: onCriticEpochEnd}),
        tf.callbacks.earlyStopping({monitor: 'loss', verbose: 2, patience: 10})
      ]
    }).then(info => {
        this.latestCriticLoss = info.history.loss.slice(-1);
        this.criticLosses.push(this.latestCriticLoss[0]);
      }
    );

    tfActionState.dispose();
    tfState.dispose();
    advantages.dispose()
    targets.dispose();
  }

  async save() {
    await this.actor.save('localstorage://actor');
    await this.critic.save('localstorage://critic');

    this.averageReward = (this.averageReward * this.episode + reward) / (this.episode + 1);
    this.episode++;

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

  getBoltzmannDistribution(weights) {

    const probabilitiesArray = [].slice.call(weights);

    const numerator = [];
    for (let i = 0; i < probabilitiesArray.length; i++) {
      numerator.push(Math.exp(-probabilitiesArray[i] / this.config.boltzmannTemperature));
    }

    let denominator = 0;
    for (let i = 0; i < numerator.length; i++) {
      denominator += numerator[i];
    }

    const boltzmannDist = [];
    for (let i = 0; i < numerator.length; i++) {
      boltzmannDist.push(numerator[i] / denominator);
    }

    return boltzmannDist;
  }

  getActionOneHot(action) {
    switch (action) {
      case 0:
        return [0.0, 0.0];
      case 1:
        return [0.0, 1.0];
      case 2:
        return [1.0, 0.0];
      case 3:
        return [1.0, 1.0];
    }
  }

  reset() {
    this.replayBuffer = [];
    this.rewards = [];
    this.testRewards = [];
    this.step = 0;
    this.test = !this.test && this.episode > 0 && this.episode % 10 === 0;
    this.trainingBuffer = {
      'tfActionState': [],
      'tfState': [],
      'advantages': [],
      'targets': []
    };
  }
}