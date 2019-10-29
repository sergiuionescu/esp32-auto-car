#include "WiFi.h"
#include "WebServer.h"

const int trigPin = 13;
const int echoPin = 12;

const char* ssid     = "ESP32-Access-Point";
const char* password = "987654321";

long duration;
int distance;

struct Channel {
    int en1;
    int en2;
    int pwm;
    int value;
};

Channel m1 = { 18 , 19 , 21 , 0 }; // en1 , en2 , pwm , value
Channel m2 = { 25 , 26 , 27 , 0 };

int standby = 33;

// Setting PWM properties
const int freq = 20000;
const int pwmChannelLeft = 0;
const int pwmChannelRight = 1;
const int resolution = 8;

IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

WebServer server(80);
String header;

void setup() {
  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input

  pinMode(m1.en1, OUTPUT);
  pinMode(m1.en2, OUTPUT);
  pinMode(m1.pwm, OUTPUT);
  pinMode(m2.en1, OUTPUT);
  pinMode(m2.en2, OUTPUT);
  pinMode(m2.pwm, OUTPUT);
  pinMode(standby, OUTPUT);

  digitalWrite(standby, HIGH);

  ledcSetup(pwmChannelLeft, freq, resolution);
  ledcSetup(pwmChannelRight, freq, resolution);
  ledcAttachPin(m1.pwm, pwmChannelLeft);
  ledcAttachPin(m2.pwm, pwmChannelRight);

  Serial.println("Setting AP (Access Point)…");

  WiFi.softAP(ssid, password);
  Serial.println("Wait 100 ms for AP_START...");
  delay(100);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);
  
  server.on("/", handleIndex);
  server.on("/distance", handleDistance);
  server.on("/data", handleData);
  
  server.begin();

  Serial.println("Setup done");
}

void loop() {
  
  server.handleClient();
  
  updateDistance();
  updatePWM();
}


void handleIndex() {
  server.send(200, "text/html", "<html>  <head>    <style>      body {        font-size: 2em;      }      div.container {        max-width: 800px;        max-height: 600px;        width: 100%;        height: 100%;        background-color: lightgray;        position: absolute;        top: 0;        bottom: 0;        left: 0;        right: 0;        margin: auto;      }      div.progress {        left: 0;        right: 0;        position: absolute;        margin: auto;        width: 50%;        height: 50px;        background-color: blueviolet;      }      div.control {        position: absolute;        margin: auto;        width: 80%;        height: 70%;        top: 0;        bottom: 0;        left: 0;        right: 0;      }      div.left {        background-color: gray;              position: absolute;        top: 0;        bottom: 0;        left: 0;        margin: auto;      }      div.right {        background-color: gray;              position: absolute;        top: 0;        bottom: 0;        right: 0;        margin: auto;      }      .throttle {        height: 80%;        width: 100px;        padding: 0 15px;        -webkit-appearance: slider-vertical;      }      .vcenter {        position: absolute;        left: 0;        right: 0;        margin: auto;      }      label {        position: absolute;        left: 0;        right: 0;        display: block;        margin-bottom: 10px;      }      div.slider {        width: 100px;      }      div.actions {        position: absolute;        top: 0;        bottom: 0;        left: 0;        right: 0;        margin: auto;        width:300px;      }      input {        font-size: 30px;      }          </style>  </head>  <body>    <div class='container'>      <div class='progress' id='progress'></div>      <div class='control'>        <div class='left slider'>            <label id='left_label' for='left'>0</label>            <input type='range' id='left' class='throttle vcenter' min=-100 max=100/>        </div>        <div class='actions'>          <input id='locked' type='button' value='Unlock'/>        </div>        <div class='right slider'>            <label id='right_label' for='right'>0</label>            <input type='range' id='right' class='throttle vcenter' min=-100 max=100/>        </div>      </div>    </div>    <script>      function getProgress() {        var xhttp = new XMLHttpRequest();        var left = document.getElementById('left').value;        var right = document.getElementById('right').value;        xhttp.onreadystatechange = function() {          if (this.readyState == 4 && this.status == 200) {            var response = JSON.parse(this.responseText.trim());            var distance = Math.min(response['data']['distance'], 500);            var newWidth = parseInt(distance/5);            document.getElementById('progress').style.width =            newWidth + '%';              setTimeout(getProgress, 500);          }        };        xhttp.open('GET', '/data?right=' + right + '&left=' + left, true);        xhttp.send();      }      getProgress();      var locked = true;      document.getElementById('locked').addEventListener(        'click', function () {          if(locked) {            document.getElementById('locked').value = 'Lock';            locked = false;          } else {            document.getElementById('locked').value = 'Unlock';            locked = true;          }          document.getElementById('right').value = 0;          document.getElementById('right_label').innerText = 0;          document.getElementById('left').value = 0;          document.getElementById('left_label').innerText = 0;        }      );      document.getElementById('left').addEventListener(        'input', function () {          document.getElementById('left_label').innerText = document.getElementById('left').value;          if(locked) {            document.getElementById('right').value = document.getElementById('left').value;            document.getElementById('right_label').innerText = document.getElementById('left').value;          }        }      );      document.getElementById('right').addEventListener(        'input', function () {          document.getElementById('right_label').innerText = document.getElementById('right').value;          if(locked) {            document.getElementById('left').value = document.getElementById('right').value;            document.getElementById('left_label').innerText = document.getElementById('right').value;          }        }      );    </script>  </body></html>\n");
}

void handleData() {
  String message;
  for (int i = 0; i < server.args(); i++) {
    message += "Arg n" + (String)i + " –> ";
    message += server.argName(i) + ": ";
    message += server.arg(i) + "\n";

    if(server.argName(i) == "right") {
      m1.value = server.arg(i).toInt();
    }
    if(server.argName(i) == "left") {
      m2.value = server.arg(i).toInt();
    }

    if(distance < 10) {
      m1.value = 0;
      m2.value = 0;
    }
  } 
  Serial.println(message);

  
  String response = "";
  response += "{\"data\":{\"motors\":";
  response += "{\"left\":{\"value\":";
  response += m2.value;
  response += "},\"right\":{\"value\":";
  response += m1.value;
  response += "}},";
  response += "\"distance\":";
  response += distance;
  response += "}}\n";
  server.send(200, "text/html", response);
}


void updateDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);

  distance = duration*0.034/2;
}

void handleDistance() {
  String response = "";
  response = response + distance;
  response = response + "\n";

  server.send(200, "text/html", response);
}


void updatePWM(){
    //Channel 1
    if(m1.value > 0){
        digitalWrite(m1.en1, LOW);
        digitalWrite(m1.en2, HIGH);
    }else{
        digitalWrite(m1.en1, HIGH);
        digitalWrite(m1.en2, LOW);  
    }
    ledcWrite(pwmChannelLeft, abs(m1.value));

    //Channel 2
    if(m2.value < 0){
        digitalWrite(m2.en1, LOW);
        digitalWrite(m2.en2, HIGH);
    }else{
        digitalWrite(m2.en1, HIGH);
        digitalWrite(m2.en2, LOW); 
    }
    ledcWrite(pwmChannelRight, abs(m2.value));
}
