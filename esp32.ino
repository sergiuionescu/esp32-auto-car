#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "AsyncJson.h"
#include "ArduinoJson.h"
#include "Wire.h"
#include "MPU9250.h"

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

const int onBoardLed = 2;

const int echoPinFM = 12;
const int trigPinFM = 13;

const int echoPinFL = 14;
const int trigPinFL = 32;

const int echoPinFR = 5;
const int trigPinFR = 4;

const char* ssid     = "ESP32";
const char* password = "987654321";

String wifi_ssid;
String wifi_password;

long duration;
int distanceFL;
int distanceFR;
int distanceFM;

int motorRightReference;
int motorLeftReference;

struct Channel {
    int en1;
    int en2;
    int pwm;
    int value;
};

Channel motorRight = { 26 , 25 , 33 , 0 };
Channel motorLeft = { 18 , 19 , 23 , 0 }; // en1 , en2 , pwm , value

int standby = 27;

// Setting PWM properties
const int freq = 10000;
const int pwmChannelLeft = 0;
const int pwmChannelRight = 1;
const int resolution = 8;

IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

//MPU i2c 21, 22
MPU9250 MPU(Wire,0x68);

float accelX;
float accelY;
float accelZ;
float gyroX;
float gyroY;
float gyroZ;
float magX;
float magY;
float magZ;

void setup() {
  Serial.begin(115200);

  logToSerial("Starting…");
  connectOrAp();

  pinMode(onBoardLed, OUTPUT);

  pinMode(trigPinFL, OUTPUT);
  pinMode(echoPinFL, INPUT);

  pinMode(trigPinFR, OUTPUT);
  pinMode(echoPinFR, INPUT);

  pinMode(trigPinFM, OUTPUT);
  pinMode(echoPinFM, INPUT);

  pinMode(motorRight.en1, OUTPUT);
  pinMode(motorRight.en2, OUTPUT);
  pinMode(motorRight.pwm, OUTPUT);
  pinMode(motorLeft.en1, OUTPUT);
  pinMode(motorLeft.en2, OUTPUT);
  pinMode(motorLeft.pwm, OUTPUT);
  pinMode(standby, OUTPUT);

  digitalWrite(standby, HIGH);

  if (!MPU.begin()) {
    Serial.println("IMU initialization unsuccessful");
    Serial.println("Check IMU wiring or try cycling power");
    while(1) {}
  }

  ledcSetup(pwmChannelLeft, freq, resolution);
  ledcSetup(pwmChannelRight, freq, resolution);
  ledcAttachPin(motorRight.pwm, pwmChannelRight);
  ledcAttachPin(motorLeft.pwm, pwmChannelLeft);
  
  server.on("/", HTTP_GET, handleIndex);
  server.on("/joy.min.js", handleJoystick);

  server.on("/login", handleLogin);
  server.on("/scan", handleScan);
  server.on("/connect", handleConnect);

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  
  server.begin();

  logToSerial("Setup done");
  
  digitalWrite(onBoardLed,HIGH);
  delay(500);
  digitalWrite(onBoardLed,LOW);
  delay(500);
  digitalWrite(onBoardLed,HIGH);
  delay(500);
  digitalWrite(onBoardLed,LOW);
}

void loop() {
  delay(10);
  updateDistance();  
  updatePWM();
  readMPU();
  sendToWs();
}


void handleIndex(AsyncWebServerRequest *request) {
  request->send(200, "text/html", "<html><body><div class='container'>  <div class='sensors'>    <div class='progress' id='distanceFL' style='left:0;top:0'></div>    <div class='progress' id='distanceFM' style='left:42.5%;top:0'></div>    <div class='progress' id='distanceFR' style='right:0;top:0'></div>  </div>  <div class='toolbar'>    <button class='btn' id='record' name='record'><i class='fa fa-circle'></i></button>    <button class='btn' id='stop' name='stop' disabled='disabled'><i class='fa fa-stop'></i></button>    <button class='btn' id='play' name='play' disabled='disabled'><i class='fa fa-play'></i></button>    <button class='btn off' id='assist' name='assist'><i class='fa fa-magic'></i></button>    <button class='btn off' id='auto' name='auto'><i class='fa fa-bomb'></i></button>    <button class='btn right' id='settings' name='settings'><i class='fa fa-cogs'></i></button>    <div id='counter'></div>  </div>  <div>    <div class='left'>      <div          id='left_joystick'          style='width:200px;height:200px;margin-right:50px;margin-left:50px'      ></div>    </div>    <div class='right'>      <div          id='right_joystick'          style='width:200px;height:200px;margin-right:50px;margin-left:50px'      ></div>    </div>  </div></div><div id='telemetry'></div><script src='https://cdn.jsdelivr.net/npm/@tensorflow/tfjs@2.0.1/dist/tf.min.js'></script><script src='https://sergiuionescu.github.io/esp32-auto-car/sym/js/chance.min.js'></script><script src='joy.min.js'></script><script>  class Nomarlizer {    normalizeSensorDistance(sensorDistance) {      sensorDistance = Math.round(sensorDistance/10);      return sensorDistance / 20;    }    normalizeFeatures(features) {      return [        this.normalizeSensorDistance(features[0]),        this.normalizeSensorDistance(features[1]),        this.normalizeSensorDistance(features[2]),      ];    }  }  normalizer = new Nomarlizer();</script><script>  let assist = false;  let auto = false;  let actor = false;  function getAssistedAction(state, action) {    if(!auto && (!assist || state[0] > 30)) {      return action;    }    let policy = actor.predict(tf.tensor2d(normalizer.normalizeFeatures(state), [1, state.length]), {      batchSize: 1,    });    let policyFlat = policy.dataSync();    action = chance.weighted([0, 1, 2], policyFlat);    console.log(state, action);    switch (action) {      case 0:        return [200, 200];      case 1:        return [255, -255];      case 2:        return [-255, 255];    }    return action;  }</script><script>  let leftJoystick = new JoyStick('left_joystick');  let rightJoystick = new JoyStick('right_joystick');  let maxThrottle = 220;  let recordingOn = false;  let recording = [];  let playCursor = 0;  let playbackOn = false;  let socket = null;  let state = [200, 200, 200];  let hostname = location.hostname;  let uri = 'ws://' + hostname + '/ws';  if(hostname === 'localhost') {    uri = 'ws://' + hostname + ':8000';  }  function initWebsocket() {    socket = new WebSocket(uri);    socket.onopen = function () {      if (socket.readyState === socket.OPEN) {        socket.onmessage = function (message) {          message = JSON.parse(message.data);          state = [            message['data']['distanceFM'],            message['data']['distanceFL'],            message['data']['distanceFR'],          ];          updateDistance('distanceFL', message['data']);          updateDistance('distanceFR', message['data']);          updateDistance('distanceFM', message['data']);          updateTelemetry(message['data']);        };      }    };    socket.onclose = function () {      console.log('Reconnecting to websocket...');      initWebsocket();    };  }  initWebsocket();  function updateDistance(id, data) {    let distance = Math.min(data[id], 200);    let newWidth = parseInt(distance / 5);    document.getElementById(id).style.width = newWidth + '%';    document.getElementById(id).innerHTML = distance + 'cm';  }  function updateTelemetry(data) {    document.getElementById('telemetry').innerHTML = JSON.stringify(data);  }  function getProgress() {    let throttle = leftJoystick.GetY();    let turn = rightJoystick.GetX();    let left = (throttle * maxThrottle) / 100;    left = left + ((maxThrottle - left) * turn) / 100;    let right = (throttle * maxThrottle) / 100;    right = right - ((maxThrottle - right) * turn) / 100;    if (recordingOn) {      recording.push({ left: left, right: right });      document.getElementById('counter').innerHTML = recording.length;    }    if (!playbackOn) {      action = getAssistedAction(state, [left, right]);      performAction(action[0], action[1], getProgress);    }  }  function performAction(left, right, callback) {    if(socket.readyState === socket.OPEN){      socket.send(JSON.stringify({'right': right, 'left': left}));    } else {      console.log('Waiting for socket...');    }    setTimeout(callback, 50);  }  getProgress();  function playback() {    if (playCursor >= recording.length) {      playbackOn = false;      document.getElementById('record').disabled = '';      document.getElementById('stop').disabled = '';      getProgress();      return;    }    document.getElementById('counter').innerHTML = playCursor;    left = recording[playCursor]['left'];    right = recording[playCursor]['right'];    playCursor++;    performAction(left, right, playback);  }  document.getElementById('record').addEventListener('click', function() {    document.getElementById('record').disabled = 'disabled';    document.getElementById('stop').disabled = '';    recordingOn = true;    recording = [];  });  document.getElementById('stop').addEventListener('click', function() {    document.getElementById('stop').disabled = 'disabled';    document.getElementById('record').disabled = '';    document.getElementById('play').disabled = '';    recordingOn = false;  });  document.getElementById('play').addEventListener('click', function() {    document.getElementById('record').disabled = 'disabled';    document.getElementById('stop').disabled = '';    playbackOn = true;    playCursor = 0;    playback();  });  document.getElementById('assist').addEventListener('click', function() {    assist = !assist;    if(assist) {      document.getElementById('assist').classList.remove('off');    } else {      document.getElementById('assist').classList.add('off');    }  });  document.getElementById('auto').addEventListener('click', function() {    auto = !auto;    if(auto) {      document.getElementById('auto').classList.remove('off');    } else {      document.getElementById('auto').classList.add('off');    }  });</script><script>  if(typeof tf === 'undefined') {    document.getElementById('assist').disabled = 'disabled';  } else {    tf.loadLayersModel('https://sergiuionescu.github.io/esp32-auto-car/sym/model/actor.json').then(model => {      actor = model;    });  }</script></body><head>    <meta name='viewport' content='initial-scale=1, maximum-scale=1'>    <style>      html, body {        overflow-x: hidden;      }      body {          position: relative;          font-size: 2em;      }      div.container {        max-width: 800px;        max-height: 600px;        width: 100%;        top: 0;        bottom: 0;        left: 0;        right: 0;        margin: auto;      }      div.sensors {        margin-top: 40px;      }      div.progress {        position: absolute;        margin: auto;        width: 15%;        height: 2%;        background-color: blueviolet;      }      div.control {        position: absolute;        margin: auto;        width: 80%;        height: 70%;        top: 0;        bottom: 0;        left: 0;        right: 0;      }      div.left {        width: 50%;        float: left;      }      div.right {        width: 50%;        float: left;      }      label {        position: absolute;        left: 0;        right: 0;        display: block;        margin-bottom: 10px;      }      input {        font-size: 30px;      }      .btn {        background-color: #0363c4;        border: none;        color: white;        padding: 12px 16px;        font-size: 16px;        cursor: pointer;      }      .btn:disabled {        background-color: #9a9b9b;      }      .off {          background-color: #772626;      }      .right {          float:right;      }    </style>    <link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/4.7.0/css/font-awesome.min.css'>  </head></html>\n");
}

void handleJoystick(AsyncWebServerRequest *request) {
  request->send(200, "text/html", "var JoyStick=function(t,e){var i=void 0===(e=e||{}).title?'joystick':e.title,n=void 0===e.width?0:e.width,o=void 0===e.height?0:e.height,r=void 0===e.internalFillColor?'#00AA00':e.internalFillColor,h=void 0===e.internalLineWidth?2:e.internalLineWidth,d=void 0===e.internalStrokeColor?'#003300':e.internalStrokeColor,a=void 0===e.externalLineWidth?2:e.externalLineWidth,l=void 0===e.externalStrokeColor?'#008000':e.externalStrokeColor,c=void 0===e.autoReturnToCenter||e.autoReturnToCenter,u=document.getElementById(t),s=document.createElement('canvas');s.id=i,0===n&&(n=u.clientWidth),0===o&&(o=u.clientHeight),s.width=n,s.height=o,u.appendChild(s);var f=s.getContext('2d'),g=0,v=2*Math.PI,w=(s.width-(s.width/2+10))/2,C=w+5,m=w+30,p=s.width/2,L=s.height/2,E=s.width/10,S=-1*E,k=s.height/10,W=-1*k,G=p,x=L;function R(){f.beginPath(),f.arc(p,L,m,0,v,!1),f.lineWidth=a,f.strokeStyle=l,f.stroke()}function T(){f.beginPath(),G<w&&(G=C),G+w>s.width&&(G=s.width-C),x<w&&(x=C),x+w>s.height&&(x=s.height-C),f.arc(G,x,w,0,v,!1);var t=f.createRadialGradient(p,L,5,p,L,200);t.addColorStop(0,r),t.addColorStop(1,d),f.fillStyle=t,f.fill(),f.lineWidth=h,f.strokeStyle=d,f.stroke()}'ontouchstart'in document.documentElement?(s.addEventListener('touchstart',function(t){g=1},!1),s.addEventListener('touchmove',function(t){t.preventDefault(),1===g&&t.targetTouches[0].target===s&&(G=t.targetTouches[0].pageX,x=t.targetTouches[0].pageY,G-=s.offsetLeft,x-=s.offsetTop,f.clearRect(0,0,s.width,s.height),R(),T())},!1),s.addEventListener('touchend',function(t){g=0,c&&(G=p,x=L);f.clearRect(0,0,s.width,s.height),R(),T()},!1)):(s.addEventListener('mousedown',function(t){g=1},!1),s.addEventListener('mousemove',function(t){1===g&&(G=t.pageX,x=t.pageY,G-=s.offsetLeft,x-=s.offsetTop,f.clearRect(0,0,s.width,s.height),R(),T())},!1),s.addEventListener('mouseup',function(t){g=0,c&&(G=p,x=L);f.clearRect(0,0,s.width,s.height),R(),T()},!1)),R(),T(),this.GetWidth=function(){return s.width},this.GetHeight=function(){return s.height},this.GetPosX=function(){return G},this.GetPosY=function(){return x},this.GetX=function(){return((G-p)/C*100).toFixed()},this.GetY=function(){return((x-L)/C*100*-1).toFixed()},this.GetDir=function(){var t='',e=G-p,i=x-L;return i>=W&&i<=k&&(t='C'),i<W&&(t='N'),i>k&&(t='S'),e<S&&('C'===t?t='W':t+='W'),e>E&&('C'===t?t='E':t+='E'),t}};\n");
}

void sendToWs() {
  String message;
  const size_t capacity = 3*JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(13);
  DynamicJsonDocument doc(capacity);

  JsonObject data = doc.createNestedObject("data");
  
  JsonObject data_motors = data.createNestedObject("motors");
  JsonObject data_motors_left = data_motors.createNestedObject("left");
  data_motors_left["value"] = motorLeft.value;
  JsonObject data_motors_right = data_motors.createNestedObject("right");
  data_motors_right["value"] = motorRight.value;
  data["distanceFL"] = distanceFL;
  data["distanceFR"] = distanceFR;
  data["distanceFM"] = distanceFM;
  data["accelX"] = accelX;
  data["accelY"] = accelY;
  data["accelZ"] = accelZ;
  data["gyroX"] = gyroX;
  data["gyroY"] = gyroY;
  data["gyroZ"] = gyroZ;
  data["magX"] = magX;
  data["magY"] = magY;
  data["magZ"] = magZ;

  serializeJson(doc, message);

  ws.textAll(message);
}


void updateDistance() {
//  logToSerial("Sensor FL");
  digitalWrite(trigPinFL, LOW);
  delayMicroseconds(4);
  digitalWrite(trigPinFL, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPinFL, LOW);
  duration = pulseIn(echoPinFL, HIGH);
  distanceFL = duration*0.034/2;

//  logToSerial("Sensor FR");
  digitalWrite(trigPinFR, LOW);
  delayMicroseconds(4);
  digitalWrite(trigPinFR, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPinFR, LOW);
  duration = pulseIn(echoPinFR, HIGH);
  distanceFR = duration*0.034/2;

//  logToSerial("Sensor FM");
  digitalWrite(trigPinFM, LOW);
  delayMicroseconds(4);
  digitalWrite(trigPinFM, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPinFM, LOW);
  duration = pulseIn(echoPinFM, HIGH);
  distanceFM = duration*0.034/2;
//  logToSerial("Sensors Done");
}

void updatePWM(){
    motorRight.value = motorRightReference;
    motorLeft.value = motorLeftReference;
    bool distanceThreshold = false;
    bool avoidAtAllCost = false;

    if(distanceFL <= 30 || distanceFM <= 30 || distanceFR <= 30) {
      distanceThreshold = true;
    }
    
    if(motorRight.value > 0 && motorLeft.value > 0 && distanceThreshold) {
      avoidAtAllCost = true;
    }

    if(distanceThreshold) {
      digitalWrite(onBoardLed,HIGH);
    } else {
      digitalWrite(onBoardLed,LOW);
    }
    avoidAtAllCost = false;

   if(avoidAtAllCost) {
      if(distanceFL < distanceFR) {
        motorRight.value = -220;
        motorLeft.value = 220;
      } else {
        motorRight.value = 220;
        motorLeft.value = -220;
      }
    } 
    
    //Channel 1
    if(motorRight.value < 0){
        digitalWrite(motorRight.en1, LOW);
        digitalWrite(motorRight.en2, HIGH);
    }else{
        digitalWrite(motorRight.en1, HIGH);
        digitalWrite(motorRight.en2, LOW);  
    }
    ledcWrite(pwmChannelRight, abs(motorRight.value));

    //Channel 2
    if(motorLeft.value < 0){
        digitalWrite(motorLeft.en1, LOW);
        digitalWrite(motorLeft.en2, HIGH);
    }else{
        digitalWrite(motorLeft.en1, HIGH);
        digitalWrite(motorLeft.en2, LOW); 
    }
    ledcWrite(pwmChannelLeft, abs(motorLeft.value));
}

void handleLogin(AsyncWebServerRequest *request) {
  String responseText = "";
  request->send(200, "text/html", "<html><body><div class='container'>  <div class='sensors'>    <div class='progress' id='distanceFL' style='left:0;top:0'></div>    <div class='progress' id='distanceFM' style='left:42.5%;top:0'></div>    <div class='progress' id='distanceFR' style='right:0;top:0'></div>  </div>  <div class='toolbar'>    <button class='btn' id='record' name='record'><i class='fa fa-circle'></i></button>    <button class='btn' id='stop' name='stop' disabled='disabled'><i class='fa fa-stop'></i></button>    <button class='btn' id='play' name='play' disabled='disabled'><i class='fa fa-play'></i></button>    <button class='btn off' id='assist' name='assist'><i class='fa fa-magic'></i></button>    <button class='btn off' id='auto' name='auto'><i class='fa fa-bomb'></i></button>    <button class='btn right' id='settings' name='settings'><i class='fa fa-cogs'></i></button>    <div id='counter'></div>  </div>  <div>    <div class='left'>      <div          id='left_joystick'          style='width:200px;height:200px;margin-right:50px;margin-left:50px'      ></div>    </div>    <div class='right'>      <div          id='right_joystick'          style='width:200px;height:200px;margin-right:50px;margin-left:50px'      ></div>    </div>  </div></div><div class='telemetry'>  <span id='magX'>0</span>  <span id='magY'>0</span>  <span id='magZ'>0</span></div><script src='https://cdn.jsdelivr.net/npm/@tensorflow/tfjs@2.0.1/dist/tf.min.js'></script><script src='https://sergiuionescu.github.io/esp32-auto-car/sym/js/chance.min.js'></script><script src='joy.min.js'></script><script>  class Nomarlizer {    normalizeSensorDistance(sensorDistance) {      sensorDistance = Math.round(sensorDistance/10);      return sensorDistance / 20;    }    normalizeFeatures(features) {      return [        this.normalizeSensorDistance(features[0]),        this.normalizeSensorDistance(features[1]),        this.normalizeSensorDistance(features[2]),      ];    }  }  normalizer = new Nomarlizer();</script><script>  let assist = false;  let auto = false;  let actor = false;  function getAssistedAction(state, action) {    if(!auto && (!assist || state[0] > 30)) {      return action;    }    let policy = actor.predict(tf.tensor2d(normalizer.normalizeFeatures(state), [1, state.length]), {      batchSize: 1,    });    let policyFlat = policy.dataSync();    action = chance.weighted([0, 1, 2], policyFlat);    console.log(state, action);    switch (action) {      case 0:        return [200, 200];      case 1:        return [255, -255];      case 2:        return [-255, 255];    }    return action;  }</script><script>  let leftJoystick = new JoyStick('left_joystick');  let rightJoystick = new JoyStick('right_joystick');  let maxThrottle = 220;  let recordingOn = false;  let recording = [];  let playCursor = 0;  let playbackOn = false;  let socket = null;  let state = [200, 200, 200];  let hostname = location.hostname;  let uri = 'ws://' + hostname + '/ws';  if(hostname === 'localhost') {    uri = 'ws://' + hostname + ':8000';  }  function initWebsocket() {    socket = new WebSocket(uri);    socket.onopen = function () {      if (socket.readyState === socket.OPEN) {        socket.onmessage = function (message) {          message = JSON.parse(message.data);          state = [            message['data']['distanceFM'],            message['data']['distanceFL'],            message['data']['distanceFR'],          ];          updateDistance('distanceFL', message['data']);          updateDistance('distanceFR', message['data']);          updateDistance('distanceFM', message['data']);          updateTelemetry(message['data']);        };      }    };    socket.onclose = function () {      console.log('Reconnecting to websocket...');      initWebsocket();    };  }  initWebsocket();  function updateDistance(id, data) {    let distance = Math.min(data[id], 200);    let newWidth = parseInt(distance / 5);    document.getElementById(id).style.width = newWidth + '%';    document.getElementById(id).innerHTML = distance + 'cm';  }  function updateTelemetry(data) {    document.getElementById('magX').innerText = data['magX'];    document.getElementById('magY').innerText = data['magY'];    document.getElementById('magZ').innerText = data['magZ'];  }  function getProgress() {    let throttle = leftJoystick.GetY();    let turn = rightJoystick.GetX();    let left = (throttle * maxThrottle) / 100;    left = left + ((maxThrottle - left) * turn) / 100;    let right = (throttle * maxThrottle) / 100;    right = right - ((maxThrottle - right) * turn) / 100;    if (recordingOn) {      recording.push({ left: left, right: right });      document.getElementById('counter').innerHTML = recording.length;    }    if (!playbackOn) {      action = getAssistedAction(state, [left, right]);      performAction(action[0], action[1], getProgress);    }  }  function performAction(left, right, callback) {    if(socket.readyState === socket.OPEN){      socket.send(JSON.stringify({'right': right, 'left': left}));    } else {      console.log('Waiting for socket...');    }    setTimeout(callback, 50);  }  getProgress();  function playback() {    if (playCursor >= recording.length) {      playbackOn = false;      document.getElementById('record').disabled = '';      document.getElementById('stop').disabled = '';      getProgress();      return;    }    document.getElementById('counter').innerHTML = playCursor;    left = recording[playCursor]['left'];    right = recording[playCursor]['right'];    playCursor++;    performAction(left, right, playback);  }  document.getElementById('record').addEventListener('click', function() {    document.getElementById('record').disabled = 'disabled';    document.getElementById('stop').disabled = '';    recordingOn = true;    recording = [];  });  document.getElementById('stop').addEventListener('click', function() {    document.getElementById('stop').disabled = 'disabled';    document.getElementById('record').disabled = '';    document.getElementById('play').disabled = '';    recordingOn = false;  });  document.getElementById('play').addEventListener('click', function() {    document.getElementById('record').disabled = 'disabled';    document.getElementById('stop').disabled = '';    playbackOn = true;    playCursor = 0;    playback();  });  document.getElementById('assist').addEventListener('click', function() {    assist = !assist;    if(assist) {      document.getElementById('assist').classList.remove('off');    } else {      document.getElementById('assist').classList.add('off');    }  });  document.getElementById('auto').addEventListener('click', function() {    auto = !auto;    if(auto) {      document.getElementById('auto').classList.remove('off');    } else {      document.getElementById('auto').classList.add('off');    }  });</script><script>  if(typeof tf === 'undefined') {    document.getElementById('assist').disabled = 'disabled';  } else {    tf.loadLayersModel('https://sergiuionescu.github.io/esp32-auto-car/sym/model/actor.json').then(model => {      actor = model;    });  }</script></body><head>    <meta name='viewport' content='initial-scale=1, maximum-scale=1'>    <style>      html, body {        overflow-x: hidden;      }      body {          position: relative;          font-size: 2em;      }      div.container {        max-width: 800px;        max-height: 600px;        width: 100%;        top: 0;        bottom: 0;        left: 0;        right: 0;        margin: auto;      }      div.sensors {        margin-top: 40px;      }      div.progress {        position: absolute;        margin: auto;        width: 15%;        height: 2%;        background-color: blueviolet;      }      div.control {        position: absolute;        margin: auto;        width: 80%;        height: 70%;        top: 0;        bottom: 0;        left: 0;        right: 0;      }      div.left {        width: 50%;        float: left;      }      div.right {        width: 50%;        float: left;      }      label {        position: absolute;        left: 0;        right: 0;        display: block;        margin-bottom: 10px;      }      input {        font-size: 30px;      }      .btn {        background-color: #0363c4;        border: none;        color: white;        padding: 12px 16px;        font-size: 16px;        cursor: pointer;      }      .btn:disabled {        background-color: #9a9b9b;      }      .off {          background-color: #772626;      }      .right {          float:right;      }    </style>    <link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/4.7.0/css/font-awesome.min.css'>  </head></html>\n");
}

void handleScan(AsyncWebServerRequest *request) {
  String responseText;
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  int n = WiFi.scanNetworks();

  const size_t capacity = JSON_ARRAY_SIZE(n) + n * JSON_OBJECT_SIZE(3);
  DynamicJsonDocument doc(capacity);
  
  for (int i = 0; i < n; ++i) {
    JsonObject wifi = doc.createNestedObject();
    wifi["ssid"] = WiFi.SSID(i);
    wifi["rssi"] = 100 + WiFi.RSSI(i);
    wifi["isOpen"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "1" : "0";
  }
  
  serializeJson(doc, responseText);
  response->print(responseText);
  
  request->send(response);
}

void handleConnect(AsyncWebServerRequest *request) {
  int params = request->params();
  for(int i=0;i<params;i++){
    AsyncWebParameter* param = request->getParam(i);
    if(param->name() == "ssid") {
      wifi_ssid = param->value();
    }
    if(param->name() == "password") {
      wifi_password = param->value();
    }
  }

  logToSerial(wifi_ssid.c_str());

  WiFi.disconnect();
  delay(100);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    logToSerial("Connecting to WiFi..");
  }
 
  logToSerial("Connected to the WiFi network");
  logToSerial(WiFi.localIP().toString());
}

void connectOrAp() {
  WiFi.begin();
  for(int i=0;i<10;i++){
     if(WiFi.status() == WL_CONNECTED) {
      delay(100);
      break;
    }
    delay(500);
    logToSerial("Connecting to previous WiFi..");
  }

  if(WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.disconnect();
    delay(100);
    
    logToSerial("Setting AP (Access Point)…");
  
    WiFi.softAP(ssid, password);
    logToSerial("Wait 100 ms for AP_START...");
    delay(100);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    delay(100);
  } else {
    logToSerial(WiFi.localIP().toString());
    logToSerial(WiFi.gatewayIP().toString());
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    StaticJsonDocument<200> doc;
    deserializeJson(doc, (char*)data);     
   
    motorRightReference = (int)doc["right"]; 
    motorLeftReference = (int)doc["left"];   
  }
}


void readMPU() {
  // read the sensor
  MPU.readSensor();

  accelX = MPU.getAccelX_mss();
  accelY = MPU.getAccelY_mss();
  accelZ = MPU.getAccelZ_mss();
  gyroX = MPU.getGyroX_rads();
  gyroY = MPU.getGyroY_rads();
  gyroZ = MPU.getGyroZ_rads();
  magX = MPU.getMagX_uT();
  magY = MPU.getMagY_uT();
  magZ = MPU.getMagZ_uT();
}

void logToSerial(const String message) {
  Serial.printf("%lld::", esp_timer_get_time());
  Serial.println(message);
}
  
