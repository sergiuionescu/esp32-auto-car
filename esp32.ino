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
  server.on("/joy.min.js", handleJoystick);
  
  server.begin();

  Serial.println("Setup done");
}

void loop() {
  delay(50);
  updateDistance();  
  updatePWM();
}


void handleIndex(AsyncWebServerRequest *request) {
  request->send(200, "text/html", "<html>  <head>    <style>      body {        font-size: 2em;      }      div.container {        max-width: 800px;        max-height: 600px;        width: 100%;        height: 100%;        top: 0;        bottom: 0;        left: 0;        right: 0;        margin: auto;      }      div.progress {        position: absolute;        margin: auto;        width: 45%;        height: 2%;        background-color: blueviolet;      }      div.control {        position: absolute;        margin: auto;        width: 80%;        height: 70%;        top: 0;        bottom: 0;        left: 0;        right: 0;      }      div.left {        padding-top: 10%;        width:50%;        float: left;      }      div.right {        padding-top: 10%;        width:50%;        float: left;      }      .throttle {        height: 80%;        width: 100px;        padding: 0 15px;        -webkit-appearance: slider-vertical;      }      .vcenter {        position: absolute;        left: 0;        right: 0;        margin: auto;      }      label {        position: absolute;        left: 0;        right: 0;        display: block;        margin-bottom: 10px;      }      div.slider {        width: 100px;      }      input {        font-size: 30px;      }          </style>  </head>  <body>    <div class='container'>      <div class='progress' id='distanceFL' style='left:0;top:0'></div>      <div class='progress' id='distanceFR' style='right:0;top:0'></div>      <div class='toolbar'>        <input type='button' id='record' name='record' value='O' />         <input type='button' id='stop' name='stop' value='[]' disabled='disabled' />         <input type='button' id='play' name='play' value='>' disabled='disabled'/>         <div id='counter'></div>      </div>      <div>           <div class='left'>            <div id='left_joystick' style='width:200px;height:200px;margin:50px'></div>        </div>        <div class='right'>            <div id='right_joystick' style='width:200px;height:200px;margin:50px'></div>        </div>              </div>    </div>    <script src='joy.min.js'></script>    <script>      var leftJoystick = new JoyStick('left_joystick');      var rightJoystick = new JoyStick('right_joystick');      var maxThrottle = 200;      var recordingOn = false;      var recording = [];      var playCursor = 0;      var playbackOn = false;      function updateDistance(id, data) {        var distance = Math.min(data[id], 200);        var newWidth = parseInt(distance/5);        document.getElementById(id).style.width = newWidth + '%';        document.getElementById(id).innerHTML = distance + 'cm';      }      function getProgress() {        var throttle = leftJoystick.GetY();        var turn = rightJoystick.GetX();        var left = throttle * maxThrottle / 100;        left = left + (maxThrottle - left) * turn / 100;        var right = throttle * maxThrottle / 100;        right = right - (maxThrottle - right) * turn / 100;        if(recordingOn) {          recording.push({'left':left, 'right':right});          document.getElementById('counter').innerHTML = recording.length;        }        if(!playbackOn) {          performAction(left, right, getProgress);        }      }      function performAction(left, right, callback) {        var xhttp = new XMLHttpRequest();        xhttp.onreadystatechange = function() {          if (this.readyState == 4 && this.status == 200) {            var response = JSON.parse(this.responseText.trim());            console.log(this.responseText);            updateDistance('distanceFL', response['data']);            updateDistance('distanceFR', response['data']);            setTimeout(callback, 500);                      }        };        xhttp.onerror = function(e){          setTimeout(callback, 1000);        };        xhttp.open('GET', '/data?right=' + right + '&left=' + left, true);        xhttp.send();      }            getProgress();      function playback() {        if(playCursor >= recording.length) {          playbackOn = false;          document.getElementById('record').disabled = '';          document.getElementById('stop').disabled = '';          getProgress();          return;        }        document.getElementById('counter').innerHTML = playCursor;        left = recording[playCursor]['left'];        right = recording[playCursor]['right'];        playCursor++;        performAction(left, right, playback);      }      document.getElementById('record').addEventListener(        'click', function () {          document.getElementById('record').disabled = 'disabled';          document.getElementById('stop').disabled = '';          recordingOn = true;          recording = [];        }      );      document.getElementById('stop').addEventListener(        'click', function () {          document.getElementById('stop').disabled = 'disabled';          document.getElementById('record').disabled = '';          document.getElementById('play').disabled = '';          recordingOn = false;        }      );      document.getElementById('play').addEventListener(        'click', function () {          document.getElementById('record').disabled = 'disabled';          document.getElementById('stop').disabled = '';          playbackOn = true;          playCursor = 0;          playback();        }      );    </script>  </body></html>\n");
}

void handleJoystick(AsyncWebServerRequest *request) {
  request->send(200, "text/html", "var JoyStick=function(t,e){var i=void 0===(e=e||{}).title?'joystick':e.title,n=void 0===e.width?0:e.width,o=void 0===e.height?0:e.height,r=void 0===e.internalFillColor?'#00AA00':e.internalFillColor,h=void 0===e.internalLineWidth?2:e.internalLineWidth,d=void 0===e.internalStrokeColor?'#003300':e.internalStrokeColor,a=void 0===e.externalLineWidth?2:e.externalLineWidth,l=void 0===e.externalStrokeColor?'#008000':e.externalStrokeColor,c=void 0===e.autoReturnToCenter||e.autoReturnToCenter,u=document.getElementById(t),s=document.createElement('canvas');s.id=i,0===n&&(n=u.clientWidth),0===o&&(o=u.clientHeight),s.width=n,s.height=o,u.appendChild(s);var f=s.getContext('2d'),g=0,v=2*Math.PI,w=(s.width-(s.width/2+10))/2,C=w+5,m=w+30,p=s.width/2,L=s.height/2,E=s.width/10,S=-1*E,k=s.height/10,W=-1*k,G=p,x=L;function R(){f.beginPath(),f.arc(p,L,m,0,v,!1),f.lineWidth=a,f.strokeStyle=l,f.stroke()}function T(){f.beginPath(),G<w&&(G=C),G+w>s.width&&(G=s.width-C),x<w&&(x=C),x+w>s.height&&(x=s.height-C),f.arc(G,x,w,0,v,!1);var t=f.createRadialGradient(p,L,5,p,L,200);t.addColorStop(0,r),t.addColorStop(1,d),f.fillStyle=t,f.fill(),f.lineWidth=h,f.strokeStyle=d,f.stroke()}'ontouchstart'in document.documentElement?(s.addEventListener('touchstart',function(t){g=1},!1),s.addEventListener('touchmove',function(t){t.preventDefault(),1===g&&t.targetTouches[0].target===s&&(G=t.targetTouches[0].pageX,x=t.targetTouches[0].pageY,G-=s.offsetLeft,x-=s.offsetTop,f.clearRect(0,0,s.width,s.height),R(),T())},!1),s.addEventListener('touchend',function(t){g=0,c&&(G=p,x=L);f.clearRect(0,0,s.width,s.height),R(),T()},!1)):(s.addEventListener('mousedown',function(t){g=1},!1),s.addEventListener('mousemove',function(t){1===g&&(G=t.pageX,x=t.pageY,G-=s.offsetLeft,x-=s.offsetTop,f.clearRect(0,0,s.width,s.height),R(),T())},!1),s.addEventListener('mouseup',function(t){g=0,c&&(G=p,x=L);f.clearRect(0,0,s.width,s.height),R(),T()},!1)),R(),T(),this.GetWidth=function(){return s.width},this.GetHeight=function(){return s.height},this.GetPosX=function(){return G},this.GetPosY=function(){return x},this.GetX=function(){return((G-p)/C*100).toFixed()},this.GetY=function(){return((x-L)/C*100*-1).toFixed()},this.GetDir=function(){var t='',e=G-p,i=x-L;return i>=W&&i<=k&&(t='C'),i<W&&(t='N'),i>k&&(t='S'),e<S&&('C'===t?t='W':t+='W'),e>E&&('C'===t?t='E':t+='E'),t}};\n");
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
  delayMicroseconds(4);
  digitalWrite(trigPinFL, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPinFL, LOW);
  duration = pulseIn(echoPinFL, HIGH);
  distanceFL = duration*0.034/2;


  digitalWrite(trigPinFR, LOW);
  delayMicroseconds(4);
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
