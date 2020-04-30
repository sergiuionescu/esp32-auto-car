#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "AsyncJson.h"
#include "ArduinoJson.h"

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

const int trigPinFL = 13;
const int echoPinFL = 12;

const int trigPinFR = 32;
const int echoPinFR = 14;

const int maxDistance = 200;
const int maxPower = 256;
const int minPower = -256;
const float distanceToPowerMultiplier = 0.5;

const char* ssid     = "ESP32";
const char* password = "987654321";

long duration;
int distanceFL;
int distanceFR;

int motorRightReference;
int motorLeftReference;

struct Channel {
    int en1;
    int en2;
    int pwm;
    int value;
};

Channel motorRight = { 25 , 26 , 27 , 0 };
Channel motorLeft = { 18 , 19 , 21 , 0 }; // en1 , en2 , pwm , value

int standby = 33;

// Setting PWM properties
const int freq = 20000;
const int pwmChannelLeft = 0;
const int pwmChannelRight = 1;
const int resolution = 8;

IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  pinMode(trigPinFL, OUTPUT);
  pinMode(echoPinFL, INPUT);

  pinMode(trigPinFR, OUTPUT);
  pinMode(echoPinFR, INPUT);

  pinMode(motorRight.en1, OUTPUT);
  pinMode(motorRight.en2, OUTPUT);
  pinMode(motorRight.pwm, OUTPUT);
  pinMode(motorLeft.en1, OUTPUT);
  pinMode(motorLeft.en2, OUTPUT);
  pinMode(motorLeft.pwm, OUTPUT);
  pinMode(standby, OUTPUT);

  digitalWrite(standby, HIGH);

  ledcSetup(pwmChannelLeft, freq, resolution);
  ledcSetup(pwmChannelRight, freq, resolution);
  ledcAttachPin(motorRight.pwm, pwmChannelLeft);
  ledcAttachPin(motorLeft.pwm, pwmChannelRight);

  Serial.println("Setting AP (Access Point)…");

  WiFi.softAP(ssid, password);
  Serial.println("Wait 100 ms for AP_START...");
  delay(100);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);
  
  server.on("/", HTTP_GET, handleIndex);
  server.on("/data", handleData);
  
  server.begin();

  Serial.println("Setup done");
}

void loop() {
  updateDistance();  
  updatePWM();
}


void handleIndex(AsyncWebServerRequest *request) {
  request->send(200, "text/html", "<html>  <head>    <style>      body {        font-size: 2em;      }      div.container {        max-width: 800px;        max-height: 600px;        width: 100%;        height: 100%;        background-color: lightgray;        position: absolute;        top: 0;        bottom: 0;        left: 0;        right: 0;        margin: auto;      }      div.progress {        position: absolute;        margin: auto;        width: 45%;        height: 2%;        background-color: blueviolet;      }      div.control {        position: absolute;        margin: auto;        width: 80%;        height: 70%;        top: 0;        bottom: 0;        left: 0;        right: 0;      }      div.left {        background-color: gray;              position: absolute;        top: 0;        bottom: 0;        left: 0;        margin: auto;      }      div.right {        background-color: gray;              position: absolute;        top: 0;        bottom: 0;        right: 0;        margin: auto;      }      .throttle {        height: 80%;        width: 100px;        padding: 0 15px;        -webkit-appearance: slider-vertical;      }      .vcenter {        position: absolute;        left: 0;        right: 0;        margin: auto;      }      label {        position: absolute;        left: 0;        right: 0;        display: block;        margin-bottom: 10px;      }      div.slider {        width: 100px;      }      div.actions {        position: absolute;        top: 0;        bottom: 0;        left: 0;        right: 0;        margin: auto;        width:300px;      }      input {        font-size: 30px;      }          </style>  </head>  <body>    <div class='container'>      <div class='progress' id='distanceFL' style='left:0;top:0'></div>      <div class='progress' id='distanceFR' style='right:0;top:0'></div>      <div class='control'>        <div class='left slider'>            <label id='left_label' for='left'>0</label>            <input type='range' id='left' class='throttle vcenter' min='-255' max='255'/>        </div>        <div class='actions'>          <input id='locked' type='button' value='Unlock'/>        </div>        <div class='right slider'>            <label id='right_label' for='right'>0</label>            <input type='range' id='right' class='throttle vcenter' min='-255' max='255'/>        </div>      </div>      <div class='progress' id='distanceBL' style='left:0;bottom:0'></div>      <div class='progress' id='distanceBR' style='right:0;bottom:0'></div>    </div>    <script>      function updateDistance(id, data) {        var distance = Math.min(data[id], 200);        var newWidth = parseInt(distance/5);        document.getElementById(id).style.width = newWidth + '%';        document.getElementById(id).innerHTML = distance + 'cm';      }      function getProgress() {        var xhttp = new XMLHttpRequest();        var left = document.getElementById('left').value;        var right = document.getElementById('right').value;        xhttp.onreadystatechange = function() {          if (this.readyState == 4 && this.status == 200) {            var response = JSON.parse(this.responseText.trim());            console.log(this.responseText);            updateDistance('distanceFL', response['data']);            updateDistance('distanceFR', response['data']);                        setTimeout(getProgress, 250);          }        };        xhttp.onerror = function(e){          setTimeout(getProgress, 50);        };        xhttp.open('GET', '/data?right=' + right + '&left=' + left, true);        xhttp.send();      }      getProgress();      var locked = true;      document.getElementById('locked').addEventListener(        'click', function () {          if(locked) {            document.getElementById('locked').value = 'Lock';            locked = false;          } else {            document.getElementById('locked').value = 'Unlock';            locked = true;          }          document.getElementById('right').value = 0;          document.getElementById('right_label').innerText = 0;          document.getElementById('left').value = 0;          document.getElementById('left_label').innerText = 0;        }      );      document.getElementById('left').addEventListener(        'input', function () {          document.getElementById('left_label').innerText = document.getElementById('left').value;          if(locked) {            document.getElementById('right').value = document.getElementById('left').value;            document.getElementById('right_label').innerText = document.getElementById('left').value;          }        }      );      document.getElementById('right').addEventListener(        'input', function () {          document.getElementById('right_label').innerText = document.getElementById('right').value;          if(locked) {            document.getElementById('left').value = document.getElementById('right').value;            document.getElementById('left_label').innerText = document.getElementById('right').value;          }        }      );    </script>  </body></html>\n");
}

void handleData(AsyncWebServerRequest *request) {
  String message;
  String responseText;
  int count = request->params();
  for (int i = 0; i < count; i++) {
    AsyncWebParameter* param = request->getParam(i);
    message += "Param n" + (String)i + " –> ";
    message += param->name() + ": ";
    message += param->value() + "\n";

    if(param->name() == "right") {
      motorRightReference = param->value().toInt();
    }
    if(param->name() == "left") {
      motorLeftReference = param->value().toInt();
    }
  } 
  Serial.println(message);

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  
  const size_t capacity = 3*JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(5);
  DynamicJsonDocument doc(capacity);

  JsonObject data = doc.createNestedObject("data");
  
  JsonObject data_motors = data.createNestedObject("motors");
  JsonObject data_motors_left = data_motors.createNestedObject("left");
  data_motors_left["value"] = motorLeft.value;
  JsonObject data_motors_right = data_motors.createNestedObject("right");
  data_motors_right["value"] = motorRight.value;
  data["distanceFL"] = distanceFL;
  data["distanceFR"] = distanceFR;
  data["distanceBL"] = 0;
  data["distanceBR"] = 0;

  serializeJson(doc, responseText);
  response->print(responseText);
  
  request->send(response);
}


void updateDistance() {
  digitalWrite(trigPinFL, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPinFL, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPinFL, LOW);
  duration = pulseIn(echoPinFL, HIGH);
  distanceFL = duration*0.034/2;


  digitalWrite(trigPinFR, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPinFR, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPinFR, LOW);
  duration = pulseIn(echoPinFR, HIGH);
  distanceFR = duration*0.034/2;
}

void updatePWM(){
    motorRight.value = motorRightReference;
    motorLeft.value = motorLeftReference;
    if(motorRight.value > 0 && distanceFL < 100) {
      motorRight.value = max(motorRight.value - (maxDistance - distanceFL) * distanceToPowerMultiplier, minPower);
    }
    if(motorLeft.value > 0 && distanceFR < 100) {
      motorLeft.value = max(motorLeft.value - (maxDistance - distanceFR) * distanceToPowerMultiplier, minPower);
    }
    
    //Channel 1
    if(motorRight.value < 0){
        digitalWrite(motorRight.en1, LOW);
        digitalWrite(motorRight.en2, HIGH);
    }else{
        digitalWrite(motorRight.en1, HIGH);
        digitalWrite(motorRight.en2, LOW);  
    }
    ledcWrite(pwmChannelLeft, abs(motorRight.value));

    //Channel 2
    if(motorLeft.value < 0){
        digitalWrite(motorLeft.en1, LOW);
        digitalWrite(motorLeft.en2, HIGH);
    }else{
        digitalWrite(motorLeft.en1, HIGH);
        digitalWrite(motorLeft.en2, LOW); 
    }
    ledcWrite(pwmChannelRight, abs(motorLeft.value));
}
