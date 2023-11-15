class ActorCritic {
  constructor(featureCount, historyLength, actionSize, reset, act, config) {
    this.render = false;
    this.featureCount = featureCount;
    this.historyLength = historyLength;
    this.actionSize = actionSize;
    this.replayBuffer = [];
    this.valueSize = 1;
    this.act = act;

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
  }

  customActorLoss(logProb, advantages) {
    return tf.tidy(() => {
      // Multiply the log probability by the advantages
      const loss = tf.mul(logProb, advantages);

      // Take the negative of the mean (since we want to maximize expected returns)
      return tf.neg(tf.mean(loss));
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

  getAction(state, actions) {
    let normalizeFeatures = normalizer.normalizeFeatures(state);
    let policy = this.actor.predict(tf.tensor2d(normalizeFeatures, [1, state.length * state[0].length]), {
      batchSize: 1,
    });

    let policyFlat = policy.dataSync();
    let weights = [parseFloat(policyFlat[ACTION_FORWARD]), parseFloat(policyFlat[ACTION_RIGHT]), parseFloat(policyFlat[ACTION_LEFT])];
    pushToWeightsChart(this.step, weights);

    let action = chance.weighted(actions, weights);
    perceptionChart.update(normalizeFeatures, action);
    if (Math.random() < this.config.epsilon) {
      action = chance.weighted(actions, [1, 1, 1]);
    }

    return action;
  }

  bufferReplay(previousState, previousAction, reward, state) {
    this.replayBuffer.push({'ps': previousState, 'pa': previousAction, 'r': reward, 's': state});
    this.computeTrainingData(previousState, state, reward);

    this.step++;
  }

  convertRawTrainingHistory() {
    this.replayBuffer.forEach(async (data) => {
      await actorCritic.bufferTrainingData(data.ps, data.pa, data.r, data.s);
    })
  }

  async bufferTrainingData(state, action, reward, nextState) {
    let {advantages, normalizedState, target, advantage} = this.computeTrainingData(state, nextState, reward, false);

    advantages[action] = advantage;

    this.trainingBuffer['tfState'].push(normalizedState);
    this.trainingBuffer['advantages'].push(advantages);
    this.trainingBuffer['targets'].push(target);

    this.rewards.push(reward);
  }

  computeTrainingData(previousState, state, reward, chart = true) {
    let advantages = new Array(this.actionSize).fill(0);

    let normalizedPreviousState = normalizer.normalizeFeatures(previousState);
    let tfPreviousState = tf.tensor2d(normalizedPreviousState, [1, normalizedPreviousState.length]);
    let predictedPreviousStateValue = this.critic.predict(tfPreviousState).dataSync();

    let normalizedState = normalizer.normalizeFeatures(state);
    let tfState = tf.tensor2d(normalizedState, [1, normalizedState.length]);
    let predictedStateValue = this.critic.predict(tfState).dataSync();

    let target = reward + config.discountFactor * predictedStateValue;
    let advantage = target - predictedPreviousStateValue;

    if (chart) {
      pushToRewardChart(actorCritic.step, reward, parseFloat(predictedPreviousStateValue), parseFloat(predictedStateValue), advantage, target);
    }

    return {advantages, normalizedState: normalizedPreviousState, target, advantage};
  }

  async trainModel() {
    let epochs = 50;
    let batchSize = 64;

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
      callbacks: {
        onEpochEnd: onActorEpochEnd,
        earlyStopping: tf.callbacks.earlyStopping
      }
    }).then(info => {
        this.latestActorLoss = info.history.loss.slice(-1);
        this.actorLosses.push(this.latestActorLoss[0]);
      }
    );

    await this.critic.fit(tfState, targets, {
      epochs: epochs,
      batchSize: batchSize,
      callbacks: {
        onEpochEnd: onCriticEpochEnd,
        earlyStopping: tf.callbacks.earlyStopping
      }
    }).then(info => {
        this.latestCriticLoss = info.history.loss.slice(-1);
        this.criticLosses.push(this.latestCriticLoss[0]);
      }
    );
  }

  async save() {
    await this.actor.save('localstorage://actor');
    await this.critic.save('localstorage://critic');

    this.averageReward = (this.averageReward * this.episode + reward) / (this.episode + 1);
    this.episode++;

    let replayBufferSize = Math.min(config.maxReplayBufferSize, parseInt(3000000 / JSON.stringify(this.replayBuffer[0]).length));
    localStorage.setItem('replayBuffer', JSON.stringify(this.replayBuffer.slice(-replayBufferSize)));
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
      this.replayBuffer = JSON.parse(localStorage.getItem('replayBuffer') ?? '[]');
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

  getAverage(list) {
    let sum = 0;
    for (let i = 0; i < list.length; i++) {
      sum += list[i];
    }
    return sum / list.length;
  }
}