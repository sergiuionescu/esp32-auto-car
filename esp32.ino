#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "AsyncJson.h"
#include "ArduinoJson.h"
#include "Wire.h"
#include "MPU9250.h"
#include "MadgwickAHRS.h"
#include "SPIFFS.h"
#include "EloquentTinyML.h"
#include "eloquent_tinyml/tensorflow.h"
#include "model_data.h"
#include <VL53L0X.h>

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#define N_INPUTS 25
#define N_OUTPUTS 4
#define TENSOR_ARENA_SIZE 2*1024
#define AI_ACTION_FORWARD 0
#define AI_ACTION_RIGHT 1
#define AI_ACTION_LEFT 2
#define AI_ACTION_BACK 3
#define AI_MODE_OFF 0
#define AI_MODE_ASSIST 1
#define AI_MODE_AUTO 2

Eloquent::TinyML::TensorFlow::TensorFlow<N_INPUTS, N_OUTPUTS, TENSOR_ARENA_SIZE> tf;

const int onBoardLed = 2;

const int pulseTimeout = 8000;

const char* ssid     = "ESP32";
const char* password = "987654321";

String wifi_ssid;
String wifi_password;

long duration;
int distanceFL;
int distanceFR;
int distanceFM;
int distanceBR;
int distanceBL;

VL53L0X sensorFM;
VL53L0X sensorFL;
VL53L0X sensorBL;
VL53L0X sensorFR;
VL53L0X sensorBR;

int motorRightReference;
int motorLeftReference;
int iddleDirection = 1;

struct Channel {
  int en1;
  int en2;
  int pwm;
  int value;
};

Channel motorLeft = { 26 , 25 , 32 , 0 };
Channel motorRight = { 18 , 19 , 23 , 0 }; // en1 , en2 , pwm , value

int standby = 27;

// Setting PWM properties
const int freq = 10000;
const int pwmChannelLeft = 0;
const int pwmChannelRight = 1;
const int resolution = 8;

IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

//MPU i2c 21, 22
MPU9250 MPU(Wire, 0x68);
Madgwick MadgwickFilter;

float accelX;
float accelY;
float accelZ;
float gyroX;
float gyroY;
float gyroZ;
float magX;
float magY;
float magZ;

float heading, yaw, pitch, roll;

unsigned long loopMs;
unsigned long loopTimeMs;
unsigned long distanceTimeMs;
unsigned long pwmTimeMs;
unsigned long mpuTimeMs;
unsigned long iddleDirectionTimeMs;
unsigned long aiActionTimeMs;

float input[N_INPUTS] = {};
int aiAction = 0;
int aiMode = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  logToSerial("Starting…");
  connectOrAp();

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  pinMode(onBoardLed, OUTPUT);

  init_sensor(sensorFM, 0x31, 13);
  init_sensor(sensorFL, 0x30, 5);
  init_sensor(sensorBL, 0x33, 14);
  init_sensor(sensorFR, 0x34, 12);
  init_sensor(sensorBR, 0x32, 33);

  pinMode(motorRight.en1, OUTPUT);
  pinMode(motorRight.en2, OUTPUT);
  pinMode(motorRight.pwm, OUTPUT);
  pinMode(motorLeft.en1, OUTPUT);
  pinMode(motorLeft.en2, OUTPUT);
  pinMode(motorLeft.pwm, OUTPUT);
  pinMode(standby, OUTPUT);
  iddleDirectionTimeMs = millis();

  digitalWrite(standby, HIGH);

  if (!MPU.begin()) {
    Serial.println("IMU initialization unsuccessful");
    Serial.println("Check IMU wiring or try cycling power");
    while (1) {}
  }

  ledcSetup(pwmChannelLeft, freq, resolution);
  ledcSetup(pwmChannelRight, freq, resolution);
  ledcAttachPin(motorRight.pwm, pwmChannelRight);
  ledcAttachPin(motorLeft.pwm, pwmChannelLeft);

  server.on("/", HTTP_GET, handleIndex);
  server.on("/joy.min.js", handleJoystick);
  server.on("/font-awesome.min.css", handleFont);
  server.on("/fonts/fontawesome-webfont.woff2", handleFontWoff);

  server.on("/login", handleLogin);
  server.on("/scan", handleScan);
  server.on("/connect", handleConnect);

  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.begin();

  //  Serial.println("calibrate MPU accelerometer");
  //  MPU.calibrateAccel();
  //  Serial.println("calibrate MPU gyro");
  //  MPU.calibrateGyro();
  //  Serial.println("calibrate MPU magnetometer");
  //  MPU.calibrateMag();
  //  Serial.println("calibrate completed");


  MPU.setAccelRange(MPU9250::ACCEL_RANGE_4G);
  MPU.setGyroRange(MPU9250::GYRO_RANGE_500DPS);
  MPU.setDlpfBandwidth(MPU9250::DLPF_BANDWIDTH_92HZ);
  MPU.setSrd(8); // 1000Hz / (1 + SRD)

  MadgwickFilter.begin(111); // Hz

  tf.begin(model_data);
  // check if model loaded fine
  if (!tf.isOk()) {
    Serial.print("ERROR: ");
    Serial.println(tf.getErrorMessage());

    while (true) delay(1000);
  }
  for (int i = 0; i < N_INPUTS; i++) {
    input[i] = 1;
  }

  scan_i2c();

  logToSerial("Setup done");

  digitalWrite(onBoardLed, HIGH);
  delay(500);
  digitalWrite(onBoardLed, LOW);
  delay(500);
  digitalWrite(onBoardLed, HIGH);
  delay(500);
  digitalWrite(onBoardLed, LOW);
}

void loop() {
  delay(5);
  unsigned long loopStartTimeMs = millis();
  unsigned long carrierTimeMs = millis();


  if (loopMs > 0) {
    loopTimeMs = loopStartTimeMs - loopMs;
  }
  loopMs = loopStartTimeMs;

  read_sensor(sensorFM, distanceFM);
  read_sensor(sensorFR, distanceFR);
  read_sensor(sensorBR, distanceBR);
  read_sensor(sensorFL, distanceFL);
  read_sensor(sensorBL, distanceBL);

  pushToInputBuffer(distanceBL, distanceFL, distanceFM, distanceFR, distanceBR);
  distanceTimeMs = millis() -  carrierTimeMs;
  carrierTimeMs = millis();
  getAiAction();
  aiActionTimeMs = millis() - carrierTimeMs;
  carrierTimeMs = millis();
  updatePWM();
  pwmTimeMs = millis() -  carrierTimeMs;
  carrierTimeMs = millis();
  readMPU();
  mpuTimeMs = millis() -  carrierTimeMs;
  sendToWs();
}

void handleIndex(AsyncWebServerRequest *request) {
  request->send(SPIFFS, "/index.html");
}

void handleJoystick(AsyncWebServerRequest *request) {
  request->send(SPIFFS, "/joy.min.js");
}

void handleActor(AsyncWebServerRequest *request) {
  request->send(SPIFFS, "/actor.json");
}

void handleActorWeights(AsyncWebServerRequest *request) {
  request->send(SPIFFS, "/actor.weights.bin");
}

void handleFont(AsyncWebServerRequest *request) {
  request->send(SPIFFS, "/font-awesome.min.css");
}

void handleFontWoff(AsyncWebServerRequest *request) {
  request->send(SPIFFS, "/fontawesome-webfont.woff2");
}

void handleChance(AsyncWebServerRequest *request) {
  request->send(SPIFFS, "/chance.min.js");
}

void handleTf(AsyncWebServerRequest *request) {
  request->send(SPIFFS, "/tf.2.0.1.min.js");
}

void sendToWs() {
  String message;
  const size_t capacity = 3 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(24);
  DynamicJsonDocument doc(capacity);

  JsonObject data = doc.createNestedObject("data");

  JsonObject data_motors = data.createNestedObject("motors");
  JsonObject data_motors_left = data_motors.createNestedObject("left");
  data_motors_left["value"] = motorLeft.value;
  JsonObject data_motors_right = data_motors.createNestedObject("right");
  data_motors_right["value"] = motorRight.value;
  data["distanceFL"] = distanceFL;
  data["distanceFM"] = distanceFM;
  data["distanceFR"] = distanceFR;
  data["accelX"] = accelX;
  data["accelY"] = accelY;
  data["accelZ"] = accelZ;
  data["gyroX"] = gyroX;
  data["gyroY"] = gyroY;
  data["gyroZ"] = gyroZ;
  data["magX"] = magX;
  data["magY"] = magY;
  data["magZ"] = magZ;
  data["loopMs"] = loopMs;
  data["loopTimeMs"] = loopTimeMs;
  data["heading"] = heading;
  data["yaw"] = yaw;
  data["roll"] = roll;
  data["pitch"] = pitch;
  data["distTimeMs"] = distanceTimeMs;
  data["pwmTimeMs"] = pwmTimeMs;
  data["mpuTimeMs"] = mpuTimeMs;
  data["signalStrength"] = WiFi.RSSI();
  data["aiActionTimeMs"] = aiActionTimeMs;

  serializeJson(doc, message);

  ws.textAll(message);
}


void init_sensor(VL53L0X &sensor, uint8_t address, int xshut_pin) {
  pinMode(xshut_pin, OUTPUT);
  digitalWrite(xshut_pin, LOW);
  digitalWrite(xshut_pin, HIGH);
  delay(50);

  sensor.setTimeout(500);
  if (!sensor.init())
  {
    Serial.println("Failed to detect and initialize sensor!");
    return;
  }
  sensor.setAddress(address);

  sensor.startContinuous(20);
}

void read_sensor(VL53L0X &sensor, int &distance) {
  distance = (int) sensor.readRangeContinuousMillimeters() / 10;
  Serial.print(sensor.readRangeContinuousMillimeters());
  if (sensor.timeoutOccurred()) {
    Serial.print(" TIMEOUT");
  }

  Serial.println();
}

void scan_i2c() {
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");

      nDevices++;
    }
    else if (error == 4)
    {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
}

void updatePWM() {
  motorRight.value = motorRightReference;
  motorLeft.value = motorLeftReference;
  bool distanceThreshold = false;


  if (distanceFL <= 50 || distanceFM <= 50 || distanceFR <= 50) {
    distanceThreshold = true;
  }

  if (distanceThreshold) {
    digitalWrite(onBoardLed, HIGH);
  } else {
    digitalWrite(onBoardLed, LOW);
  }

  if ((aiMode == AI_MODE_ASSIST && distanceThreshold) || aiMode == AI_MODE_AUTO) {
    if (aiAction == AI_ACTION_FORWARD) {
      motorRight.value = 150;
      motorLeft.value = 150;
    } else if (aiAction == AI_ACTION_RIGHT) {
      motorRight.value = -150;
      motorLeft.value = 150;
    } else if (aiAction == AI_ACTION_LEFT) {
      motorRight.value = 150;
      motorLeft.value = -150;
    } else if (aiAction == AI_ACTION_BACK) {
      motorRight.value = -150;
      motorLeft.value = -150;
    }
  }

  //Channel 1
  if (motorRight.value < 0) {
    digitalWrite(motorRight.en1, LOW);
    digitalWrite(motorRight.en2, HIGH);
  } else {
    digitalWrite(motorRight.en1, HIGH);
    digitalWrite(motorRight.en2, LOW);
  }
  ledcWrite(pwmChannelRight, abs(motorRight.value));

  //Channel 2
  if (motorLeft.value < 0) {
    digitalWrite(motorLeft.en1, LOW);
    digitalWrite(motorLeft.en2, HIGH);
  } else {
    digitalWrite(motorLeft.en1, HIGH);
    digitalWrite(motorLeft.en2, LOW);
  }
  ledcWrite(pwmChannelLeft, abs(motorLeft.value));
}

void handleLogin(AsyncWebServerRequest *request) {
  String responseText = "";
  request->send(200, "text/html", "<html>  <head>    <style>        div.container {        max-width: 200px;        max-height: 200px;        width: 100%;        height: 100%;        top: 0;        bottom: 0;        left: 0;        right: 0;        margin: auto;        }        img {        width: 100%;        height: 100%;        top: 0;        bottom: 0;        left: 0;        right: 0;        margin: auto;        }          form {    border: 3px solid #f1f1f1;    }    input[type=text], input[type=password] {    width: 100%;    padding: 12px 20px;    margin: 8px 0;    display: inline-block;    border: 1px solid #ccc;    box-sizing: border-box;    }    button {    background-color: #4CAF50;    color: white;    padding: 14px 20px;    margin: 8px 0;    border: none;    cursor: pointer;    width: 100%;    }    button:hover {    opacity: 0.8;    }    </style>  </head>  <body>        <div class='container'>        <img id='loader' style='display: none;' src='data:image/gif;base64,R0lGODlhCAIiAff9AP9pB5mZmdjY2IyMjLKysv+0g2ZmZv/ZwczMzJ+fn+vr6/+OReXl5f97Jv9yFv/GosWEWtN+RvBwHLaLb+J3MaiShPX19eLi4m9vb/+hZPj4+P/Qsf/177+/v//i0P+FNf/s4MXFxfLy8qWlpd/f36ysrM/Pz6ioqIKCgv+XVLKNdHl5eby8vP+9kri4uNLS0v+qc5WVlfjv6fhsEPLf0+LBrNrFt/e3jem+ovC7mNPIwfi3jOvPvfjcyvDg1ezi2+nj4PTe0Pfdy/vbxvjJq/LWw/iRTuuXYN6edMiok+mZZPOTVL2totOig+XSxr+ZgeN/PdWGUa2Yi7qSePB4KsiLZN7W0OPTybKfk9iOXeWISvGBOMyTbuvGrfnJqfTMse7OuenQweOui8i6stG2pfWlcdqyl+yqfvn49/z28/728Pv39P328vr39vKUV/KnduzYy/HWxeja0vXw7ffv6vrSuPTx7/XUvuPc2Pbm3PHp5Pzk1O7q5/Tn4Pnl2Pvu5bimm8+bePnu58Ogiv3t4ueQV/KKRtuWaNeqjcO0quuhceCmf/ScYsyvnOvi3NjPyd/MwOvGrvLDpPjAm+XJttjGubicit65ovauf+6yi9W9rs7BucWpl+a2lrKWhNK2o8Wgh7iTer+soPLMtMymjsWypsKFXfXw7t15N+pyI/jl2ebSxbKpo7SLcc9/S/jTut+5ofbw68W8tvPx8KeShb+jkOfk4tjFuenRweHUzOXAp8yvneXc1vK6le7h2NK/s+vi3dvX1OHUy9LIwt/CsLimmeu9nsumj8aoleDLvdizmu3h2eCdceTc19ufeMy5rfLo4s24qsa7tLuupe6WXOraz8Kqmufb0+jIs9yxldCkh+Sba/PLsfPf0ujk4d3Nwu/OuN3W0uHd29KslNW0n+vZzNnOx9qfebytpOqqgOObbN/DsMSpmLqupurQvuXJt9zX0/XdzuTJuNahftbQzc6li+DVzuiYZNOjg9rX1uri3e/Xx////////wAAAAAAACH/C05FVFNDQVBFMi4wAwEAAAAh+QQFAAD9ACwAAAAACAIiAQAI/wD5CRxIsKDBgwgTKlzIsKHDhxAjSpxIsaLFixgzatzIsaPHjyBDihxJsqTJkyhTqlzJsqXLlzBjypxJs6bNmzhz6tzJs6fPn0CDCh1KtKjRo0iTKl3KtKnTp1CjSp1KtarVq1izat3KtavXr2DDih1LtqzZs2jTql3Ltq3bt3Djyp1Lt67du3jz6t3Lt6/fv4ADCx5MuLDhw4gTK17MuLHjx5AjS55MubLly5gza97MubPnz6BDix5NurTp06hTq17NurXr17Bjy55Nu7bt27hz697Nu7fv38CDCx9OvLjx48iTK1/OvLnz59CjS59Ovbr169iza9/Ovbv37+DDi/8fT768+fPo06tfz769+/fw48ufT7++/fv48+vfz7+///8ABijggAQWaOCBCCao4IIMNujggxBGKOGEFFZo4YUYZqjhhhx26OGHIIYo4ogklmjiiSimqOKKLLbo4oswxijjjDTWaOONOOao44489ujjj0AGKeSQRBZp5JG3MaCkCEjiJoIALiQQwJQBEMBkQhq8UAKVU3agQJOgKSAll1QmQAICaKaJJgMjkEnlCF+CyZkAbtZJwJ141klmAgLIqRkDelLZAQkIkdBBoAEkcKWflhEQaAlxKqTAlnp2wKhlIgTagQYOaXBonQlcWhmgdkrkaJ0MiDrZC3WWwGlEGlD/SiYCqkpGKpcJvCpRpm7SWitkGrjZZ0UICPtrZC5wOcJFGoxJ5aLHNnZrAC9g9GmX0SL7LEYklKlrto1pkOyyBCmAQKpYnksQrwlECu5jHRBQULHyJkQqtAG4+q5kIdQ7ULIBKMRqAOgKVOW+krngr0CnKlQswQQdjDBkCJQwr8T8nOqlQKQSFOzCEzPGakEaIBAnnW8O9AKhAwHqa8jSBsDyQQ9TmVCxM8PMWAKWIoTylOQetOW3Oit2KLQFfQonQoC6ULRjmfbMNANEE+RowU8vhjNFrIKcdWLNtiuRmIp+7RigfEIkgJRYm62YAqcOyxDKJQhQtduDMdCBs1OW/5CzQSTIOmUCLvyN91+Btwp0CCQoqSQJIbSZb6tyH86Xp5XyUzLfuCLA6bVkQmr5XiRI7uYI3zKg5poem27s6Hf9jKpE0/YKe12yn46QBgK8gPRArrtZ7e1xicD5rAiV0MHeCNUM6u/EqwX67AcFgMALoR5Uu5tSR8/W8WQmJMAICbxsEKKJ3u19Wdu7aRH6EK+vlgLw49lB2yK4y4/eeaIPvfxkORX8phSCgVRMeQPJHfq8BsCyKCB46OtZAkIQgoDxo1sDnBIB1NdAsjDgTGuqYKAI1YESjEBeGoDgmwSgpv91cC0qpJLT+JE/gSiQTJV7oVxuSCakCVBPM9ThXP9iyKXhCYRX6BMiXQYYRH7wkEz6UyJb6Ac/iw1EhOgznBTV0r46WW2A5tviWjAIvy/CL4xiTIvzEGVG9KExjWdZY6AcxwDBBeqNcCyLHDN4xjyu5YmzSlMG8ehHsXSRTEHjBxGLWEi1DHBh09NT2xpJFoAhyogXhF/2KHkWMuopVwVZpPU4mZZIvimKhwxA90hplrW5iQBR5BgER6BFVpKFd2qKZUFAiAAB6NKWwAymMIdJzGJOzAAGIYABDICBE7grAZsUwDKXeQILCGQA8sLmQAgwAIEoYADTzKExkYJMgggAA9a0AAsMMCwLoAAFJ0OmBQTAzWtm0wAXYFg3LYD/gXopAAXiHGdRyjkQFLCAICfIHgtOcIJ6SZMgA+iTNgdwAhTokx8m2CQ/LmACgSqFoAJhJ0FM0E1+rOACCsCADUF6J35MVAAxOGg9T9BRjzoFpPzAQOUE0M0L6JSnBXzoNrOZTQFYYAVw62ZEr7nMktrUKDiNQQGvGAN+JGAFAxgACropVIFI1aVF5QcLsNnNExw0gU59KlFwagJ4enMFJuBnOzFwga6m1JovvSZWN0rXK6ZVrUIxQFazKpAEYCCrGChgCFZAEGhKc7D4tKdLh5XSkpoAq1rFQE0BOxQBeNaz5pQbSgkyz3l+liCjvYA1BXKBfA6krkblrGxnS9vaqtr2trjNrW53y9ve+va3wA2ucIdL3OIa97jITa5yl8vc5jr3udCNrnSnS93qWve62M2udrfL3e5697vgDa94x0ve8pr3vOhNr3rXy972uve98I2vfOdL3/ra9774za9+98vf/vr3vwAOsIAHTOACG/jACE6wghfM4AY7+MEQjrCEJ0zhClv4whjOsIY3zOEOe/jDIA6xiEdM4hKb+MQoTrGKV8ziFrs4vgEBACH5BAUAAP0ALPcAoAAeAAMAAAg0AGWpGEiwoKN+CBP2K8hQhax+DxwAmEixwQOFCg80oFhxQ0IODwqILOABo8l+G0Y+KIkwIAAh+QQFAAD9ACz2AJ4AIAADAAAIMQBLqRhIsODASv0S9uNlsKEKTxsASJxIEYADEAoTNqjIsUC/DBwpeszYr0fIiRn6BQQAIfkEBQAA/QAs9QCcACIAAwAACC8AHakYSLAgQUv9EvazZLDhwEf9AEicSHHiAoX9UlTcKPFARo4VC2B8AJKig34BAQAh+QQFAAD9ACz1AJoAIgADAAAIKQArqRhIsKDBgwgLVnoAoKHDhw8XSFwAsaLDAgcsVnTQr2O/BhohtggIACH5BAUAAP0ALPUAmAAiAAMAAAgpACupGEiwYMF+CPtxMshwYKUCACJKnDjxgMUDHyhqjFgA4saPIDcWCAgAIfkEBQAA/QAs9QCWACIAAwAACCoAHakYSLBgwX4I+3EyyHBgpQ0AIkqcODFhvwUUM0YsUECjRosYPU4sEBAAIfkEBQAA/QAs9QCUACIAAwAACCwA+6kYSLAgwV39EvZzYrDhQGgeAEicSHHiB4X9YFTcKHHDAY4cMS4AWbFAQAAh+QQFAAD9ACz2AJIAIQADAAAILACdqBhIsOBAGf0SJuRksOGufg0ASJxIUWILhf1kVNwosR/HjSkwPvhY0UNAACH5BAUAAP0ALPYAkAAgAAMAAAgzAPv9UkGwoMFy/WSBeiKqX78nBiM+cdgAgMWLGFM4BOHBIRGMIC0+eBAyJAiHKI2UzBgQACH5BAUAAP0ALO8AjgAuAAMAAAhOAEWpGEiwoEEVsPopXIeF1SeF/XgcnEiwlocGADJq3MgRQAaFDjLAAKCwyIyOKDMu8KDwwIYCMA+MTAlgQ78MDRosUIgxZYMHMGGCUBgQACH5BAUAAP0ALO4AjAAwAAMAAAhBAB2pGEiw4MB+CBMqXMiwn8GHKhx5AECxokWKRxYsyHBgIQgPC2mk0LjgokkAHUueXAkjYYEGHzIkfLByZYN+AQEAIfkEBQAA/QAs7gCKADAAAwAACDwA+31SQbCgQRWO+ilcqFDFL0ihGDJ0dLDiJ4UOAGjcyBEAB4kLHzRwUADkwo4oHXA4gBKlyZcmW3Y8EBAAIfkEBQAA/QAs7wCIAC4AAwAACEUA+/FQQbCgQUv9EipMyGFUFxALF1oySFHFu34gHADYyLFjgYgKPyzI4ABkwgIdUwJwACKDypQHTPYDAKNFSZMHXnbMEBAAIfkEBQAA/QAs8ACGACwAAwAACDYA+/Ur8kSFwYMGLQlcyLAhQ0sII/JY+ACAxYsYHzjc2PAAxo8ACvSrCPJiA44jW6BsUPJii4AAIfkEBQAA/QAs8gCEACgAAwAACE8A+wl8BUqFwYMqWBl8AuvVgYcHXsF6YlAhQoOgRgkUyCEDgI8gP2box6GAg5AhHRTg0M8jyo8fPPTb0ODlxwYsNx4owLPngY0ka9oE8CAgACH5BAUAAP0ALPUAggAiAAMAAAg9APsJFOiBlAoVPAYqXCiQx0FQozgwPJDBAYCLFz9sYKhwwweMFx2k2NjPI0iMHwE0gEFS4AYYDTKezPggIAAh+QQFAAD9ACz6AIAAGAADAAAIKQA9hVLVr6DBgwgNqgrl6QAABw8SSjT4wAGAA/0WAAAQcSLCBxs/PAgIACH5BAUAAP0ALPkAfgAaAAMAAAgjAFV86kewoMGDCPuRUlEAwIaEEA+2ALCAgwMHHiJG9HARREAAIfkEBQAA/QAs+AB8ABwAAwAACDIAPX3qR7CgQRUIVVgyyBCUigwAQDA0mAIAgAYbJhI8ACAFCAAZNBY8cICDyH4LABwICAAh+QQFAAD9ACz3AHoAHgADAAAIMgAfqehHsKBBJyoSqnhisKEjFZUOANjQ0GABABgxVix4kaKDDBsJOskIoEHIfh8AcAgIACH5BAUAAP0ALPcAeAAeAAMAAAg9AEtx6keQICcVKgoqLFgJoSOFKjgV+LBwB4AFBRcAAJDBA8EDGxVyuNgCwMJ+BTz2e7BxY4OCLTYoBFkgIAAh+QQFAAD9ACz3AHYAHgADAAAIMwCfcepHkGAlFaQKKizoSIUKGQpV1IKxYGEKAAAWLmyB8YBCAAtSVFS4AKNGhQU6flwQEAAh+QQFAAD9ACz1AHQAIgADAAAIOgBLqRjYr6DBgwgTHuQ1UIUnDikANEDoocABhQY5WDwIAgAABw8KZliAsAAAkhj7HfAI4iCADxz6BQQAIfkEBQAA/QAs9AByACQAAwAACC0AS6kYqMJSv4MIEypciNATwYEHAEgE0IKhxYsZJgLI0C/FRBAXQyrcMNEBiIAAIfkEBQAA/QAs8wBwACYAAwAACC0AHakYSNBTv4MIEypc2I+gw0ocAEic+IChRYYFJlLsl0Jjg4sgE3JwoBEAiIAAIfkEBQAA/QAs8wBuACYAAwAACDMAHakYSHCgqH4IEypcmFBUwYKOugCYSBHABw4MMy7k8KEixR0HPAJwgFGjSYQgRAIoEBAAIfkEBQAA/QAs8wBsACYAAwAACDQA+3FSQbBgwWL9EipcuLCYwYef+skAQLFixQwcGGpUyCGDRYsO+h34WHHBxpMKF5CkeCAgACH5BAUAAP0ALPQAagAkAAMAAAg4APs5UUGwoMGC/RIqPMhQRSgn/WjMAECxosUMGxRq7Lchg8WPABzIcAOy4gcPG1P28/ChJMU3AQEAIfkEBQAA/QAs9QBoACIAAwAACD4A+/UDMWpcKBUIVZCSIbChw34cuoBKiFCZqoYeHADYyHGjgw0FQooMeaBBx44NPPTjMeOkSwALYsp86ZJHQAAh+QQFAAD9ACz4AGcAHQACAAAIIQD7CTxAEITAgwgTKhT4IIUDABABLDC48GCLDxEhZvAQEAAh+QQFAAD9ACwAAAAAAQABAAAIBAD7BQQAIfkEBQAA/QAsAAAAAAEAAQAACAQA+wUEACH5BAUAAP0ALAAAAAABAAEAAAgEAPsFBAAh+QQFAAD9ACwAAAAAAQABAAAIBAD7BQQAIfkEBQAA/QAsAAAAAAEAAQAACAQA+wUEACH5BAUAAP0ALAAAAAABAAEAAAgEAPsFBAAh+QQFAAD9ACwAAAAAAQABAAAIBAD7BQQAIfkEBQAA/QAsAAAAAAEAAQAACAQA+wUEACH5BAUAAP0ALAAAAAABAAEAAAgEAPsFBAAh+QQFAAD9ACwAAAAAAQABAAAIBAD7BQQAIfkEBQAA/QAsAAAAAAEAAQAACAQA+wUEACH5BAUAAP0ALAAAAAABAAEAAAgEAPsFBAAh+QQFAAD9ACwAAAAAAQABAAAIBAD7BQQAIfkEBQAA/QAsAAAAAAEAAQAACAQA+wUEACH5BAUAAP0ALAAAAAABAAEAAAgEAPsFBAAh+QQFAAD9ACwAAAAAAQABAAAIBAD7BQQAIfkEBQAA/QAsAAAAAAEAAQAACAQA+wUEACH5BAUAAP0ALAAAAAABAAEAAAgEAPsFBAAh+QQFAAD9ACwAAAAAAQABAAAIBAD7BQQAIfkEBQAA/QAsAAAAAAEAAQAACAQA+wUEACH5BAUAAP0ALAAAAAABAAEAAAgEAPsFBAAh+QQFAAD9ACzuAGcAMAA8AAAI/wD7CRxIcOCQg4QKKlzIsGFBQl4YUZFAUcKShA3VTDJUkWKZPQ4X7pnYsSKVOjdSqkw5ZEvJiltAhhyIK9XLl0ty6rz5EtfMfj5s8pRQpg7DOmWGSqBCZ+a9oYZkOtzDkWe6kHSGllHzs5+apDephBzCc0lXgkt4DnGI7aYhrmcFqqlaspdDsiWpwI0rkNDNGw7VvPTCt+CNwSEZddxSuKAakhUxNsRLcVLjgmA9/lRMUfLlfnVMes6omDHBPTfWZkxN0O9SwmfLmCV4eDZDvJ4lvOWLybZAzg4nUVQt0GJhRr77pZXg8LAE4v2M871hqGAv6co9ysRLUHDyn8IV3v+Q6WXxwElGDUoAzJds+oXOKzY8/D4ulTINr5hnyHFv3KSjEcTZFvURRBYjl/mF32RD+FdQWtBNJ0GBZwn3XVyPUSHVWSNREWB7r8XlxUQRNrbHcrDNVJ5uXjh41hBlQEaRIRQSVAddFFHBSI0N3egWRVtgUsdBB9WBiUu6uZViRpmVhJ8aN8jYERU3cNVkR1H1iORLW/g3xEosdbclYgqtyNOSXVH2V0Fm3mSaQmp4McmH/Yz5kmV9SVkSewoZskSMDMUXFkZXvlRicZhMItZCat60oJ4luSiQF1tQ2ZBSS6nR6E2fRYfpEHtgKoE6OZURISEbAoVcTqKutZyoFGHKMtANWxiyYD9timraHnZi+mgZmDDXzz5CwbrEhkOgxFKwQxlVxhZbzNYrTF6sRCdB01aEYD+oCsQNpmiGlOtNo1GD6bZdZdsRngJlJWpcsKKLK6ypNhSqqNUNxCymPCq0KU9owcpnSKHBGrCoAzcHq7ACvapUwvMtTOQQOA4FcaALZ1zRxQuNW9ZOAqcJ66ICqbvuWcYKCOuhDHH2MEEe5xVXwUN5WJDJ6/FVaEcQx6PUrSLquUW4ArXEZb8zxblSvQope4MXTBcUEAAh+QQFAAD9ACzuAGcAMAA8AAAI/wD7CRxIcKCQg38KKlzIsGHBP18UQaFAkYKShA3ZSCpUkeIZPw4X+pnYsSKUOzlSqkwpREvJilpAhhy4CtXLl0py6rz5ctXMfr5s8qRw5g7DO2eGUoASayazoYVkOvTDkaeYkLGGnmHzsx+bpDehhBTCU0lXgkp4CnEo72YhrmcFsqla0phDsiWhwI0r8M/NHA7ZvPzCt2COwSEVddRSuCAbkhUxNsRLUVLjgmA9/lRMUfLlfndMes6omDFBPznWZkxN0O9SwmfPmCV4eDZDvJ4pvOWbybZAzg4lUVQt0GJhRb77paXg8DAF4v2M881RqKAx6co9ysRLUHDyn8IV5v+Q+WXxQElGDVIAzJds+oXOKzY8/D4ulDMNhZlnyHFv3KSjEcSZFvURRJYil/mF32RC+FdQWtBNR0GBZwn3XVyPQSHVWSNBEWB7r8X1xUQRNubHcrDNVJ5uXzh4lhBnQEZRIRQSdAddFEGhSI0N3egWRVpkcsdBB92RiUu6uZViRpmVhB8bOcjYERQ5cNVkR1H1iORLWvgnxEosdbclYgqtyNOSXVH2V0Fm3mSaQmx8IcmH/Yz5kmV9SVkSewoVokSMDMUXFkZXvlRicZlIItZCat60oJ4luSjQF1pQ2ZBSS7HR6E2fRYepEH5gSoEzOZ0R4R8bAoVcTqKutZyoFGXKMlAOWhSyYD9timqaH3Zi+ugZmTDXDxxCwarEhkKgxFKwQxl1hhZazNYrTF+sRCdB01aEYD+oCgQOpmiGlOtNo22D6bZdZdsRngJlJWpcsKKLK6ypNhSqqNUNxCymPCq0KU9owcpnSKHBGrCoAzcHq7ACvapUwvMtTKQQOA4FcaALZ1zRxQuNW9ZOAqcJ66ICqbvuWcYKCOuhDHH2MEEe5xVXwUN5WJDJ6/FVaEcQd6PUrSLqqUW4ArXEZb8zxblSvQopm8MXTBcUEAAh+QQFAAD9ACzuAGcAMAA8AAAI/wD7CRxIcGCQg4IKKlzIsGFBQWAWRYlAMQKShA3TRDpUkaKYPA4X5pnYsWKUODhSqkwZJEvJillAhhx4xdXLl0hy6rz58srMfj9s8owgJg7DOGKGRogyZ+a8oYdkOszDkWe2kHOGiknzs1+apDejhAzCE0lXgkh4BnGY7OYhrmcFpqlaUpdDsiWjwI0rUNBNHA7TvATDtyCOwSEXdcxSuGAakhUxNsRLMVLjgmA9/lRMUfLlfnFMes6omDHBPDjWZkxN0O9SwmfFmCV4eDZDvJ4jvOXbybZAzg4jUVQt0GLhRb77pY3g8HAE4v2M88VxqKAu6co9ysRLUHDyn8IV4v+QCWbxwEhGDUYAzJds+oXOKzY8/D5uFDEN7ZlnyHFv3KSjEcRZFvURRNYil/mF32RB+FdQWtBNF0GBZwn3XVyPRSHVWSNFEWB7r8UFxkQRNpbHcrDNVJ5uYDh4VhBiQEbRIRQSFAddFEWxSI0N3egWRVl0EsdBB8XRiUu6uZViRpmVhF8aOMjYURQ4cNVkR1H1iORLWfgXxEosdbclYgqtyNOSXVH2V0Fm3mSaQmmAEcmH/Yz5kmV9SVkSewodgkSMDMUXFkZXvlRicZ1EItZCat60oJ4luSgQGFlQ2ZBSS6XR6E2fRYdpEHlgGgE+OYkRoSAbAoVcTqKutZyoFHXKMhAOWRyyYD9timpaHnZi+qgYnTDXTzVCwYrEhkGgxFKwQxklRhZZzNYrTGCsRCdB01aEYD+oCuQOpmiGlOtNo52D6bZdZdsRngJlJWpcsKKLK6ypNhSqqNUNxCymPCq0KU9owcpnSKHBGrCoAzcHq7ACvapUwvMtTGQQOA4FcaALZ1zRxQuNW9ZOAqcJ66ICqbvuWcYKCOuhDHH2MEEe5xVXwUN5WJDJ6/FVaEcQ+6DUrSLqmUW4ArXEZb8zxblSvQopiwMYTBcUEAAh+QQFAAD9ACzuAGcAMAA8AAAI/wD7CRxIcKCPg3QKKlzIsGFBOmEQVYFAEUKThA3XUApUkaKZPg4X9pnYsWIVODVSqkzpg0vJilxAhhyYy9TLl01y6rz5MtfMfsBs8oRgBg5DOGaGQqhyama9oYFkOuzDkSe5kKeGmlnzs9+apDerhPTBs0lXgk14+nD47WYgrmcFrqlakphDsiWrwI0rkM7NGg7XvAzDt2CNwSERdeRSuOAakhUxNsRLkVLjgmA9/lRMUfLlfnBMes6omDHBPjXWZkxN0O9SwmfNmCV4eDZDvJ4hvOV7ybZAzg4pUVQt0GJhRL77pYXg8DAE4v2M860RqCAx6co9ysRLUHDyn8IV1v+QGWbxQEpGDUIAzJds+oXOKzY8/D5uFTMNrZhnyHFv3KSjEcQZF/URRBYil/mF32Q++FdQWtBNB0GBZwn3XVyPVSHVWSNVEWB7r8UVxkQRNtbHcrDNVJ5uYTh4lg9mQEZRIBQSBAddFFWBSI0N3egWRVxcAsdBB8FxiUu6uZViRpmVhN8aNcjYURU1cNVkR1H1iORLXPjnw0osdbclYgqtyNOSXVH2V0Fm3mSaQmuEQcmH/Yz5kmV9SVkSewoF0kSMDMUXFkZXvlRicZdQItZCat60oJ4luShQGFxQ2ZBSS63R6E2fRYepD31gCsExOZkRIR0bAoVcTqKutZyoFF3KMlANXASyYD9timpaH3Zi+qgZlzDXzzVCwdrEhj6gxFKwQxllBhdczNYrTGGsRCdB01aEYD+oCrQKpmiGlOtNo2mD6bZdZdsRngJlJWpcsKKLK6ypNhSqqNUNxCymPCq0KU9owcpnSKHBGrCoAzcHq7ACvapUwvMtTKQPOA4FcaALZ1zRxQuNW9ZOAqcJ66ICqbvuWcYKCOuhDHH2MEEe5xVXwUN5WJDJ6/FVaEcQL6PUrSLqyUW4ArXEZb8zxblSvQopW0MYTBcUEAAh+QQFAAD9ACzuAGcAMAA8AAAI/wD7CRxIcOCPg3MKKlzIsGHBOVcaTZlAcUKShA3bQBpUkSIZPQ4X6pnYseIUOTZSqkz540nJik9AhhxopdXLl0ly6rz50srMfkBs8pxARg5DOWSGTphiZya7oYNkOtTDkWe0kHaGkmnzs1+bpDenhPzBM0lXgkl4/nBo7uYgrmcFtqla8pZDsiWnwI0rcM5NGw7bvLzCt6CNwSEbdXxSuGAbkhUxNsRLEVLjgmA9/lRMUfLlfnJMes6omDFBPTbWZkxN0O9SwmfJmCV4eDZDvJ4nvOWrybZAzg4hUVQt0GLhRr77pZ3g8PAE4v2M87UxqOAt6co9ysRLUHDyn8IV2v+QeWXxQEhGDU4AzJds+oXOKzY8/D7uFDIN4ZlnyHFv3KSjEcTZE/URRFYjl/mF32Q/+FdQWtBNN0GBZwn3XVyPTSHVWSNNEWB7r8V1xUQRNqbHcrDNVJ5uVzh41g9kQEbRIBQSJAddFE3RSI0N3egWRU9oIsdBB8mhiUu6uZViRpmVhF8bNsjY0RQ2cNVkR1H1iORLT/j3w0osdbclYgqtyNOSXVH2V0Fm3mSaQm1cAcmH/Yz5kmV9SVkSewoNkkSMDMUXFkZXvlRicZpAItZCat60oJ4luSjQFU9Q2ZBSS7XR6E2fRYfpD3pgOoE1OZER4RwbAoVcTqKutZyoFGnKMpANTwyyYD9timqaHnZi+igZmjDXTzNCwZrEhj+gxFKwQxlFxhNPzNYrTFesRCdB01aEYD+oCiQMpmiGlOtNoyGD6bZdZdsRngJlJWpcsKKLK6ypNhSqqNUNxCymPCq0KU9owcpnSKHBGrCoAzcHq7ACvapUwvMtTOQPOA4FcaALZ1zRxQuNW9ZOAqcJ66ICqbvuWcYKCOuhDHH2MEEe5xVXwUN5WJDJ6/FVaEcQ66PUrSLq+US4ArXEZb8zxblSvQopa8MVTBcUEAAh+QQFAAD9ACzuAGcAMAA8AAAI/wD7CRxIcCCQg3YKKlzIsGFBO1YSSalAsQKThA3RPAJUkeIYPg4X8pnYsaIUPDpSqkwJBEvJilhAhhwYjNbLl0xy6rz5MtjMfrZs8qwwBg9DPGOGVpAya+a0oYBkOuTDkae0kLOGjkHzsx+apDelhATCk0lXgkx4AnFI7yYgrmcFoqlacphDsiWlwI0r0M5NHQ7RvLTCt6COwSETdcRSuCAakhUxNsRL8VHjgmA9/lRMUfLlfnhMes6omDFBPjrWZkxN0O9SwmfHmCV4eDZDvJ4rvOW7ybZAzg4fUVQt0GLhRL77pa3g8HAF4v2M89UBqOAw6co9ysRLUHDyn8IV6v+QaWXxwEdGDVYAzJds+oXOKzY8/D6ulDEN85lnyHFv3KSjEcQZFvURRFYil/mF32RA+FdQWtBNV0GBZwn3XVyPSSHVWSNJEWB7r8VlxUQRNsbHcrDNVJ5uVjh4FhBjQEYRIBQShAddFEmRSI0N3egWRVhsgsdBB+GxiUu6uZViRpmVhB8aOsjYkRQ6cNVkR1H1iORLWPgHxEosdbclYgqtyNOSXVH2V0Fm3mSaQmhY8ciH/Yz5kmV9SVkSewoBwkSMDMUXFkZXvlRicZs8ItZCat60oJ4luSiQFVhQ2ZBSS6HR6E2fRYcpEHxgWkE7OY0RoR0bAoVcTqKutZyoFG3KMpAOWACyYD9timoaH3Zi+ugYmzDXjzhCwcrEhkCgxFKwQxk1BhZYzNYrTFasRCdB01aEYD+oChQOpmiGlOtNo6GD6bZdZdsRngJlJWpcsKKLK6ypNhSqqNUNxCymPCq0KU9owcpnSKHBGrCoAzcHq7ACvapUwvMtTCQQOA4FcaALZ1zRxQuNW9ZOAqcJ66ICqbvuWcYKCOuhDHH2MEEe5xVXwUN5WJDJ6/FVaEcQe6PUrSLqiUW4ArXEZb8zxblSvQopq4MVTBcUEAAh+QQFAAD9ACzuAGcAMAA8AAAI/wD7CRxIcCCDgyIKKlzIsGFBEQJcJAhAMQCBhA01vChRkWIHBQ4XKpjYsWICEghSqkzJYETJiiNAhhwo4KVNAjhz2nwpYGY/BjsrdiDBkESHoAESYHRIIGgJmQ4VcNzZIaSIoB00+Oyn4ajNBCGB3txKsKlNBg5f2CyhlaxADVNLInAotmOCtm4FXn05NyPPvAUR/HXoouMIwAU1kKy4lGHdAC8QF/Tq0Wdhio0lkzCZmaGGwocJKkCANiNpgnsT9CTbgUBgi3QxEwzANm8I1wQvp6VYWiDsvC5wDzTrUHCA3v1+u0VQ4rVwsx8FiiWoQTlZtQoRyKwJc+ALogYD9P91CxT8QuMVGwo27zZBVYbcKYZeyBGv26OdB14ewZ4gUBeSXfWeYwzYV1BTyOW1nmRqCYeYYglARdZISkkWnmpuCTBRgogpYNZqM3FXggAGksVAB4tRVEJ/BZEQF0UJuMCiQy6uJV8IJBx0EAkhuETbWiBmRFlJVWmAQIp2IaDVkB091RAJPr40gn0MrMQSdVEOVlB8NgW51WN8bYnUfIkJ8EJ+/WT5UmR6ISkXQyUQgCJD6H2FEZMvcehbCC+AtRCYL73nZkklCiTACAmMpxBSSWkA6EsWJscoAwowahFOHSQogoQ/BYeTpWiZZSlFIQyEwAglDMilpaEpoCajgnayEEIAAm02KkUESMgASizNGhRRHYwwAm6vwiTASmgSVGxFAPazqaGMehnSqjZlJupOzW61bEds6jWqW6Nm2w+1NnHaUKWWNjeQr4zO+OetZY2qKI3wDifvVnUiFa+l86p3q44MvBhUv3TeanBFBC9E7k063evToy/5KdC23JI1qoOXTUpWxgMTtLBdbtkaVIXKMppwQ3h21C/ElQGmoZTSSvcqfxZqcKxK5irEKwIC5FxQQAAh+QQFAAD9ACwAAAAAAQABAAAIBAD7BQQAIfkEBQAA/QAsAAAAAAEAAQAACAQA+wUEACH5BAUAAP0ALAAAAAABAAEAAAgEAPsFBAAh+QQFAAD9ACwAAAAAAQABAAAIBAD7BQQAIfkEBQAA/QAsAAAAAAEAAQAACAQA+wUEACH5BAUAAP0ALAAAAAABAAEAAAgEAPsFBAAh+QQFAAD9ACwAAAAAAQABAAAIBAD7BQQAIfkEBQAA/QAsAAAAAAEAAQAACAQA+wUEACH5BAUAAP0ALAAAAAABAAEAAAgEAPsFBAA7' />        <form method='POST' action='/connect' id='form'>            <label for='ssid'>SSID</label>            <select name='ssid' id='ssid'></select>            <label for='ssid'>Password</label>            <input type='password' name='password' id='password'/>            <input type='submit' />            <input type='button' value='Refresh' onclick='scan();' />        </form>    </div>    <script>        function scan(){            var xhttp = new XMLHttpRequest();            xhttp.onreadystatechange = function() {                if (this.readyState == 4 && this.status == 200) {                    var response = JSON.parse(this.responseText.trim());                    console.log(this.responseText);                    var select = document.getElementById('ssid');                    select.innerHTML = '';                    for (i = 0; i < response.length; i++) {                        var option = document.createElement('option');                        if(response[i]['ssid'] && response[i]['rssi']) {                            option.text = response[i]['ssid'] + '(' + response[i]['rssi'] + ' %)';                            option.value = response[i]['ssid'];                            select.add(option);                        }                    }                    document.getElementById('loader').style.display = 'none';                    document.getElementById('form').style.display = 'inline';                }            };            xhttp.onerror = function(e) {                setTimeout(scan, 1000);            };            document.getElementById('loader').style.display = 'inline';            document.getElementById('form').style.display = 'none';            xhttp.open('GET', '/scan', true);            xhttp.send();      }       scan();    </script>  </body></html>\n");
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
  for (int i = 0; i < params; i++) {
    AsyncWebParameter* param = request->getParam(i);
    if (param->name() == "ssid") {
      wifi_ssid = param->value();
    }
    if (param->name() == "password") {
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
  //  for(int i=0;i<10;i++){
  //     if(WiFi.status() == WL_CONNECTED) {
  //      delay(100);
  //      break;
  //    }
  //    delay(500);
  //    logToSerial("Connecting to previous WiFi..");
  //  }

  if (WiFi.status() != WL_CONNECTED) {
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
    aiMode = (int)doc["mode"];
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

  MadgwickFilter.update(gyroX * 57.29578f, gyroY * 57.29578f, gyroZ * 57.29578f, accelX, accelY, accelZ, magX, magY, magZ);

  heading = atan2(-magY, -magX);
  if (heading < 0) {
    heading += 2 * PI;
  }
  if (heading > 2 * PI) {
    heading -= 2 * PI;
  }

  yaw = MadgwickFilter.getYaw();
  roll = MadgwickFilter.getRoll();
  pitch = MadgwickFilter.getPitch();

}

float normalizeInput(int distance) {
  float result = round((float) distance / 10.0);

  return result / 20.0;
}

void pushToInputBuffer(int distanceBL, int distanceFL, int distanceFM, int distanceFR, int distanceBR) {
  for (int i = 0; i < N_INPUTS - 5; i++) {
    input[i] = input[i + 5];
  }

  input[N_INPUTS - 5] = normalizeInput(distanceBL);
  input[N_INPUTS - 4] = normalizeInput(distanceFL);
  input[N_INPUTS - 3] = normalizeInput(distanceFM);
  input[N_INPUTS - 2] = normalizeInput(distanceFR);
  input[N_INPUTS - 1] = normalizeInput(distanceBR);
}

int getRandomProbs(float values[N_OUTPUTS]) {
  float rnd = esp_random() / UINT32_MAX;
  float carry = 0;
  int i;

  for (i = 0; i <= N_OUTPUTS; i++) {
    if (rnd < carry + values[i]) {
      return i;
    }
    carry += values[i];
  }

  return i;
}

void getAiAction() {
  float output[N_INPUTS] = {};
  tf.predict(input, output);

  aiAction = getRandomProbs(output);
}

void logToSerial(const String message) {
  Serial.printf("%lld::", esp_timer_get_time());
  Serial.println(message);
}
