#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_PWMServoDriver.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <Preferences.h>
#include <ArduinoOTA.h>

// OLED Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// PCA9685 PWM driver
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

const char* ssid = "ESP32-Servo-AP";
const char* password = "12345678";

AsyncWebServer server(80);
Preferences prefs;

const int servoPins[4] = {18, 19, 21, 22};
Servo servos[4];
int angles[4] = {90, 90, 90, 90};
int pos1[4] = {90, 90, 90, 90};
int pos2[4] = {90, 90, 90, 90};

const int relayPins[4] = {25, 26, 27, 14};
bool relayStates[4] = {false, false, false, false};

bool testInProgress = false;
unsigned long testStartTime = 0;
byte testState = 0;
byte testServo = 0;
byte testSpeed = 15;

// Updated button handling - TOGGLE BEHAVIOR
const int buttonPin = 13;        // Servo 1 button
const int servo2ButtonPin = 12;  // Servo 2 button
bool buttonState = false;
bool lastButtonState = false;
bool servo2ButtonState = false;
bool lastServo2ButtonState = false;
bool servoToggled = false;        // Toggle state for servo 1
bool servo2Toggled = false;       // Toggle state for servo 2

// Simplified IR implementation
const uint16_t irRecvPin = 15;
const uint32_t irCodes[] = {
  0xFFA25D, 0xFF629D, 0xFF02FD, 0xFFB04F, 0xFF22DD,
  0xFFC23D, 0xFF00FF, 0xFF5AA5, 0xFF10EF, 0xFF6897,
  0xFF42BD, 0xFF906F, 0xFF18E7
};
enum {
  BTN_1, BTN_2, BTN_5, HASH_KEY, UP_ARROW,
  DOWN_ARROW, LEFT_ARROW, RIGHT_ARROW, OK_BTN, ZERO_BTN,
  STAR_BTN, NINE_BTN, BTN_8
};

volatile uint32_t irLastCode = 0;
volatile bool irCodeReady = false;
unsigned long lastIrTime = 0;

const int pcaServo8Pin = 8;
byte pcaServo8Angle = 35;
bool pcaServo8Toggled = false;
bool hashKeyPressed = false;
bool irServo5Toggled = false;
bool irServo1Toggled = false;
bool irServo2Toggled = false;

// Enhanced HTML interface
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>ESP32 Servo 4</title>
  <style>
    body {
      font-family: 'Segoe UI', sans-serif;
      background-color: #f7f9fc;
      margin: 0;
      padding: 20px;
      text-align: center;
    }

    .container {
      max-width: 500px;
      margin: auto;
      background: white;
      padding: 20px;
      border-radius: 16px;
      box-shadow: 0 8px 24px rgba(0,0,0,0.1);
    }

    h2 {
      color: #333;
      margin-bottom: 20px;
    }

    .ota-info {
      background: #e7f3ff;
      padding: 10px;
      margin: 10px 0;
      border-radius: 8px;
      font-size: 12px;
      color: #0066cc;
    }

    .tab-bar {
      display: flex;
      justify-content: center;
      gap: 10px;
      flex-wrap: wrap;
      margin-bottom: 20px;
    }

    .tab-btn {
      padding: 10px 18px;
      font-size: 14px;
      background: #007bff;
      color: white;
      border: none;
      border-radius: 12px;
      cursor: pointer;
      transition: background 0.3s ease;
    }

    .tab-btn:hover {
      background: #0056b3;
    }

    .tab-btn.active {
      background: #ff5722;
    }

    .tab-section {
      display: none;
    }

    .tab-section.active {
      display: block;
    }

    select, input[type=range] {
      width: 100%;
      padding: 10px;
      margin: 12px 0;
      font-size: 16px;
      border-radius: 8px;
      border: 1px solid #ccc;
    }

    .slider-row {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 10px;
      margin-top: 10px;
    }

    .slider-col button {
      width: 44px;
      height: 44px;
      font-size: 20px;
      border: none;
      border-radius: 50%;
      background-color: #28a745;
      color: white;
      cursor: pointer;
      box-shadow: 0 4px 8px rgba(0,0,0,0.1);
    }

    .slider-col button:hover {
      background-color: #218838;
    }

    .action-row {
      display: flex;
      flex-wrap: wrap;
      justify-content: center;
      gap: 10px;
      margin-top: 20px;
    }

    .action-row-col {
      display: flex;
      flex-direction: column;
      gap: 10px;
    }

    .action-row button {
      padding: 10px 16px;
      font-size: 14px;
      border: none;
      border-radius: 8px;
      background: #28a745;
      color: white;
      cursor: pointer;
      transition: background 0.2s;
    }

    .action-row button:hover {
      background-color: #218838;
    }

    .center-button {
      background-color: orange !important;
      color: white !important;
    }

    .clicked {
      transform: scale(0.98);
    }

    .angle-display {
      font-size: 1.2em;
      margin: 15px 0;
      color: #444;
    }

    .highlight {
      color: #ff5722;
      font-weight: bold;
      animation: pulse 0.4s;
    }

    @keyframes pulse {
      0% { color: #444; }
      50% { color: #ff5722; }
      100% { color: #444; }
    }

    .angle-list {
      text-align: left;
      margin-top: 20px;
    }

    .angle-entry {
      padding: 8px;
      border-bottom: 1px solid #eee;
    }

    .label {
      font-weight: 500;
      margin-bottom: 5px;
    }

    .bar-container {
      background: #e0e0e0;
      border-radius: 5px;
      height: 10px;
      overflow: hidden;
    }

    .bar-fill {
      background-color: #007bff;
      height: 100%;
      width: 0%;
      transition: width 0.4s ease-in-out;
    }

    .selected-servo .bar-fill {
      background-color: #ff5722;
    }
  </style>
</head>
<body>
  <h2>ESP32 Multi-Servo 4</h2>
  <div class="ota-info">OTA Updates: Use "ESP32-Servo" in Arduino IDE | Password: servo123</div>
  
  <div class="tab-bar">
    <button class="tab-btn active" onclick="switchTab('manual')">Setting Turnouts</button>
    <button class="tab-btn" onclick="switchTab('saved')">Turnout Control</button>
  </div>

  <div class="container tab-section active" id="tab-manual">
    <label><strong>Select Servo (GPIO):</strong></label>
    <select id="servoSelect" onchange="refreshAngleList()">
      <option value="0">Servo 1 (GPIO 18)</option>
      <option value="1">Servo 2 (GPIO 19)</option>
      <option value="2">Servo 3 (GPIO 21)</option>
      <option value="3">Servo 4 (GPIO 22)</option>
    </select>

    <label><strong>Movement Speed:</strong></label>
    <select id="speedSelect" onchange="updateSpeed()">
      <option value="5">Fast</option>
      <option value="15">Medium</option>
      <option value="30">Slow</option>
    </select>

    <div class="slider-row">
      <div class="slider-col"><button onclick="clickBtn(this); adjustAngle(-1)">-</button></div>
      <input type="range" min="0" max="180" value="90" id="angleSlider" oninput="updateAngle(this.value)">
      <div class="slider-col"><button onclick="clickBtn(this); adjustAngle(1)">+</button></div>
    </div>

    <p class="angle-display">Angle: <span id="angleValue" class="highlight">90</span>&deg;</p>

    <div class="action-row">
      <div class="action-row-col">
        <button onclick="clickBtn(this); savePosition(1)">Save Pos 1</button>
        <button onclick="clickBtn(this); clearAngles()">Clear Angles</button>
      </div>
      <div class="action-row-col">
        <button class="center-button" onclick="clickBtn(this); updateAngle(90)">Center (90)</button>
        <button onclick="clickBtn(this); saveSpeed()">Save Speed</button>
      </div>
      <div class="action-row-col">
        <button onclick="clickBtn(this); savePosition(2)">Save Pos 2</button>
        <button onclick="clickBtn(this); testSequence()">Test</button>
      </div>
    </div>

    <div class="angle-list" id="angleList"></div>
  </div>

  <div class="container tab-section" id="tab-saved">
    <label><strong>Select Servo:</strong></label>
    <select id="savedServoSelect" onchange="refreshAngleList()">
      <option value="0">Servo 1 (GPIO 18)</option>
      <option value="1">Servo 2 (GPIO 19)</option>
      <option value="2">Servo 3 (GPIO 21)</option>
      <option value="3">Servo 4 (GPIO 22)</option>
    </select>

    <div class="action-row" style="margin-top: 25px;">
      <button onclick="moveToSaved('pos1')">Closed</button>
      <button onclick="moveToSaved('pos2')">Thrown</button>
    </div>

    <p class="angle-display">Current: <span id="savedAngleValue">--</span>&deg; | Pos1: <span id="savedPos1">--</span>&deg; | Pos2: <span id="savedPos2">--</span>&deg;</p>
  </div>

  <script>
    let speed = 15;

    function showToast(msg) {
      const toast = document.createElement("div");
      toast.innerText = msg;
      toast.style.cssText = "position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%); background: #333; color: white; padding: 12px 20px; border-radius: 8px; z-index: 999; opacity: 0.9;";
      document.body.appendChild(toast);
      setTimeout(() => { toast.remove(); }, 2000);
    }

    function clickBtn(btn) {
      btn.classList.add("clicked");
      setTimeout(() => btn.classList.remove("clicked"), 300);
    }

    function switchTab(tab) {
      document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
      document.querySelectorAll('.tab-section').forEach(section => section.classList.remove('active'));
      document.getElementById('tab-'+tab).classList.add('active');
      event.target.classList.add('active');
      refreshAngleList();
    }

    function updateSpeed() {
      speed = parseInt(document.getElementById("speedSelect").value);
    }

    function updateAngle(value) {
      value = parseInt(value);
      document.getElementById("angleValue").innerText = value;
      document.getElementById("angleSlider").value = value;
      const servoIndex = document.getElementById("servoSelect").value;
      fetch('/servo?num='+servoIndex+'&angle='+value+'&speed='+speed).then(() => refreshAngleList());
      const angleSpan = document.getElementById("angleValue");
      angleSpan.classList.remove("highlight");
      void angleSpan.offsetWidth;
      angleSpan.classList.add("highlight");
    }

    function adjustAngle(delta) {
      const slider = document.getElementById("angleSlider");
      let newVal = parseInt(slider.value) + delta;
      updateAngle(Math.max(0, Math.min(180, newVal)));
    }

    function savePosition(posNum) {
      const angle = parseInt(document.getElementById("angleSlider").value);
      const servoIndex = document.getElementById("servoSelect").value;
      fetch('/savepos?num='+servoIndex+'&pos='+posNum+'&angle='+angle)
        .then(() => {
          showToast('Saved Pos '+posNum);
          setTimeout(() => {
            fetch('/servo?num='+servoIndex+'&angle=90&speed='+speed)
              .then(() => {
                document.getElementById("angleSlider").value = 90;
                document.getElementById("angleValue").innerText = "90";
                refreshAngleList();
              });
          }, 250);
        });
    }

    function clearAngles() {
      const servoIndex = document.getElementById("servoSelect").value;
      fetch('/clear?num='+servoIndex).then(() => {
        showToast("Angles cleared");
        updateAngle(90);
        refreshAngleList();
      });
    }

    function refreshAngleList() {
      const selectedManual = parseInt(document.getElementById("servoSelect").value);
      const selectedSaved = parseInt(document.getElementById("savedServoSelect").value);
      fetch("/angles")
        .then(res => res.json())
        .then(data => {
          const container = document.getElementById("angleList");
          container.innerHTML = "";
          for (let i = 0; i < 4; i++) {
            const entry = document.createElement("div");
            entry.className = "angle-entry" + (i === selectedManual ? " selected-servo" : "");

            const label = document.createElement("div");
            label.className = "label";
            label.innerHTML = 'Servo '+(i + 1)+' (GPIO '+[18,19,21,22][i]+'): '+data[i]+'&deg; | Pos1: '+data[i+4]+'&deg; | Pos2: '+data[i+8]+'&deg;';

            const bar = document.createElement("div");
            bar.className = "bar-container";
            const fill = document.createElement("div");
            fill.className = "bar-fill";
            fill.style.width = (data[i] / 1.8)+'%';
            bar.appendChild(fill);

            entry.appendChild(label);
            entry.appendChild(bar);
            container.appendChild(entry);
          }
          document.getElementById("savedAngleValue").innerText = data[selectedSaved];
          document.getElementById("savedPos1").innerText = data[selectedSaved + 4];
          document.getElementById("savedPos2").innerText = data[selectedSaved + 8];
        });
    }

    function moveToSaved(posType) {
      const servoIndex = document.getElementById("savedServoSelect").value;
      let angle = 90;
      if (posType === "pos1") angle = parseInt(document.getElementById("savedPos1").innerText);
      if (posType === "pos2") angle = parseInt(document.getElementById("savedPos2").innerText);
      fetch('/servo?num='+servoIndex+'&angle='+angle+'&speed='+speed).then(() => refreshAngleList());
    }

    function testSequence() {
      const servoIndex = document.getElementById("servoSelect").value;
      fetch('/test?num='+servoIndex+'&speed='+speed).then(() => showToast("Test started"));
    }

    function saveSpeed() {
      fetch('/savespeed?speed='+speed).then(() => showToast("Speed saved"));
    }

    window.onload = function() {
      refreshAngleList();
      fetch('/getspeed')
        .then(res => res.text())
        .then(val => {
          speed = parseInt(val);
          document.getElementById("speedSelect").value = speed;
        });
    }

    setInterval(refreshAngleList, 2000);
  </script>
</body>
</html>
)rawliteral";

// Simple IR interrupt
void IRAM_ATTR irInterrupt() {
  static unsigned long lastTime = 0;
  static byte bitCount = 0;
  static uint32_t code = 0;
  
  unsigned long now = micros();
  unsigned long duration = now - lastTime;
  lastTime = now;
  
  if (digitalRead(irRecvPin) == HIGH) { // Rising edge
    if (duration > 8000 && duration < 10000) {
      bitCount = 0;
      code = 0;
    } else if (bitCount < 32) {
      if (duration > 1400 && duration < 1800) {
        code |= (1UL << (31 - bitCount));
      }
      bitCount++;
      if (bitCount >= 32) {
        irLastCode = code;
        irCodeReady = true;
      }
    }
  }
}

void setRelay(byte index, bool state) {
  digitalWrite(relayPins[index], state);
  relayStates[index] = state;
  if (index == 0) {
    pwm.setPWM(0, 0, state ? 4095 : 0);
    pwm.setPWM(7, 0, state ? 0 : 4095);
  }
}

void updateOLED(byte servoIndex, int angle) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 10);
  display.print(F("Servo"));
  display.println(servoIndex + 1);
  display.setCursor(10, 35);
  display.println(angle >= 91 ? F("Thrown") : F("Closed"));
  display.display();
}

void smoothMove(Servo &servo, int &currentAngle, int targetAngle, byte delayMs) {
  int step = (targetAngle > currentAngle) ? 1 : -1;
  while (currentAngle != targetAngle) {
    currentAngle += step;
    servo.write(currentAngle);
    delay(delayMs);
    byte index = &servo - servos;
    if (index < 4) {
      if (currentAngle >= 91) setRelay(index, true);
      else if (currentAngle <= 89) setRelay(index, false);
      updateOLED(index, currentAngle);
    }
  }
}

void handleIR() {
  if (!irCodeReady || (millis() - lastIrTime < 200)) return;
  
  uint32_t code = irLastCode;
  irCodeReady = false;
  lastIrTime = millis();
  
  byte btnIndex = 255;
  for (byte i = 0; i < 13; i++) {
    if (irCodes[i] == code) {
      btnIndex = i;
      break;
    }
  }
  
  if (btnIndex == 255) return;
  
  Serial.print(F("IR: 0x"));
  Serial.println(code, HEX);
  
  switch (btnIndex) {
    case BTN_1:
      irServo1Toggled = !irServo1Toggled;
      smoothMove(servos[0], angles[0], irServo1Toggled ? pos1[0] : pos2[0], testSpeed);
      updateOLED(0, angles[0]);
      break;
    case BTN_2:
      irServo2Toggled = !irServo2Toggled;
      smoothMove(servos[1], angles[1], irServo2Toggled ? pos1[1] : pos2[1], testSpeed);
      updateOLED(1, angles[1]);
      break;
    case BTN_5:
      irServo5Toggled = !irServo5Toggled;
      smoothMove(servos[0], angles[0], irServo5Toggled ? pos1[0] : pos2[0], testSpeed);
      smoothMove(servos[1], angles[1], irServo5Toggled ? pos1[1] : pos2[1], testSpeed);
      break;
    case HASH_KEY:
      hashKeyPressed = true;
      break;
    case NINE_BTN:
      if (hashKeyPressed) {
        hashKeyPressed = false;
        for (byte i = 0; i < 4; i++) {
          smoothMove(servos[i], angles[i], 90, testSpeed);
          prefs.putInt(("angle" + String(i)).c_str(), 90);
        }
      }
      break;
    case BTN_8:
      pcaServo8Toggled = !pcaServo8Toggled;
      pcaServo8Angle = pcaServo8Toggled ? 135 : 35;
      pwm.setPWM(pcaServo8Pin, 0, map(pcaServo8Angle, 0, 180, 150, 600));
      break;
    default:
      hashKeyPressed = false;
      break;
  }
}

void setup() {
  Serial.begin(115200);
  prefs.begin("servoPos", false);
  testSpeed = prefs.getInt("speed", 15);

  // Setup buttons with pullup resistors
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(servo2ButtonPin, INPUT_PULLUP);
  pinMode(irRecvPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(irRecvPin), irInterrupt, CHANGE);

  for (byte i = 0; i < 4; i++) {
    servos[i].setPeriodHertz(50);
    servos[i].attach(servoPins[i], 500, 2400);
    angles[i] = prefs.getInt(("angle" + String(i)).c_str(), 90);
    pos1[i] = prefs.getInt(("pos1_" + String(i)).c_str(), 90);
    pos2[i] = prefs.getInt(("pos2_" + String(i)).c_str(), 90);
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }

  Wire.begin(4, 5);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 failed"));
    while (true);
  }
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Servo Init"));
  display.display();
  delay(1000);

  pwm.begin();
  pwm.setPWMFreq(1000);

  // LED test
  pwm.setPWM(0, 0, 4095);
  delay(1000);
  pwm.setPWM(0, 0, 0);
  pwm.setPWM(7, 0, 4095);
  delay(1000);
  pwm.setPWM(7, 0, 0);
  delay(2000);

  WiFi.softAP(ssid, password);
  Serial.print(F("AP IP: "));
  Serial.println(WiFi.softAPIP());

  // Setup OTA
  ArduinoOTA.setHostname("ESP32-Servo");
  ArduinoOTA.setPassword("servo123");
  
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("Start updating " + type);
    for (byte i = 0; i < 4; i++) {
      servos[i].detach();
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("OTA Update"));
    display.println(F("Starting..."));
    display.display();
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nOTA Complete"));
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("OTA Complete"));
    display.println(F("Restarting..."));
    display.display();
    delay(1000);
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    unsigned int percent = (progress / (total / 100));
    Serial.printf("Progress: %u%%\r", percent);
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("OTA Update"));
    display.printf("Progress: %u%%", percent);
    display.display();
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    String errorMsg = "";
    if (error == OTA_AUTH_ERROR) errorMsg = "Auth Failed";
    else if (error == OTA_BEGIN_ERROR) errorMsg = "Begin Failed";
    else if (error == OTA_CONNECT_ERROR) errorMsg = "Connect Failed";
    else if (error == OTA_RECEIVE_ERROR) errorMsg = "Receive Failed";
    else if (error == OTA_END_ERROR) errorMsg = "End Failed";
    
    Serial.println(errorMsg);
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("OTA Error:"));
    display.println(errorMsg);
    display.display();
    delay(3000);
  });
  
  ArduinoOTA.begin();
  Serial.println(F("OTA Ready"));
  Serial.print(F("Password: servo123"));
  Serial.println();

  // Move servos to start positions
  for (byte i = 0; i < 4; i++) {
    smoothMove(servos[i], angles[i], 90, 20);
    angles[i] = 90;
    delay(500);
  }
  
  delay(2000);
  
  for (byte i = 0; i < 4; i++) {
    smoothMove(servos[i], angles[i], pos2[i], 20);
    angles[i] = pos2[i];
    delay(500);
  }

  // Web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/servo", HTTP_GET, [](AsyncWebServerRequest *request) {
    byte num = request->getParam("num")->value().toInt();
    int angle = request->getParam("angle")->value().toInt();
    byte speed = request->getParam("speed")->value().toInt();
    if (num < 4) {
      smoothMove(servos[num], angles[num], angle, speed);
      angles[num] = angle;
      prefs.putInt(("angle" + String(num)).c_str(), angle);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Invalid");
    }
  });

  server.on("/angles", HTTP_GET, [](AsyncWebServerRequest *request) {
    String response = "[";
    for (byte i = 0; i < 4; i++) {
      response += String(angles[i]);
      if (i < 3) response += ",";
    }
    response += ",";
    for (byte i = 0; i < 4; i++) {
      response += String(pos1[i]);
      if (i < 3) response += ",";
    }
    response += ",";
    for (byte i = 0; i < 4; i++) {
      response += String(pos2[i]);
      if (i < 3) response += ",";
    }
    response += "]";
    request->send(200, "application/json", response);
  });

  server.on("/getspeed", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(testSpeed));
  });

  server.on("/savespeed", HTTP_GET, [](AsyncWebServerRequest *request) {
    testSpeed = request->getParam("speed")->value().toInt();
    prefs.putInt("speed", testSpeed);
    request->send(200, "text/plain", "Saved");
  });

  server.on("/relay", HTTP_GET, [](AsyncWebServerRequest *request) {
    byte num = request->getParam("num")->value().toInt();
    bool state = request->getParam("state")->value().toInt();
    if (num < 4) {
      setRelay(num, state);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Invalid");
    }
  });

  server.on("/savepos", HTTP_GET, [](AsyncWebServerRequest *request) {
    byte num = request->getParam("num")->value().toInt();
    byte pos = request->getParam("pos")->value().toInt();
    int angle = request->getParam("angle")->value().toInt();
    if (num < 4 && (pos == 1 || pos == 2)) {
      if (pos == 1) {
        pos1[num] = angle;
        prefs.putInt(("pos1_" + String(num)).c_str(), angle);
      } else {
        pos2[num] = angle;
        prefs.putInt(("pos2_" + String(num)).c_str(), angle);
      }
      request->send(200, "text/plain", "Saved");
    } else {
      request->send(400, "text/plain", "Invalid");
    }
  });

  server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!testInProgress) {
      testServo = request->getParam("num")->value().toInt();
      testSpeed = request->getParam("speed")->value().toInt();
      if (testServo < 4) {
        testInProgress = true;
        testStartTime = millis();
        testState = 0;
        request->send(200, "text/plain", "Started");
      } else {
        request->send(400, "text/plain", "Invalid");
      }
    } else {
      request->send(200, "text/plain", "Busy");
    }
  });

  server.on("/clear", HTTP_GET, [](AsyncWebServerRequest *request) {
    byte num = request->getParam("num")->value().toInt();
    if (num < 4) {
      angles[num] = pos1[num] = pos2[num] = 90;
      prefs.putInt(("angle" + String(num)).c_str(), 90);
      prefs.putInt(("pos1_" + String(num)).c_str(), 90);
      prefs.putInt(("pos2_" + String(num)).c_str(), 90);
      smoothMove(servos[num], angles[num], 90, 10);
      request->send(200, "text/plain", "Cleared");
    } else {
      request->send(400, "text/plain", "Invalid");
    }
  });

  server.begin();
  
  Serial.println(F("Setup complete!"));
  Serial.println(F("Button Controls:"));
  Serial.println(F("- GPIO 13: Servo 1 (Toggle)"));
  Serial.println(F("- GPIO 12: Servo 2 (Toggle)"));
  Serial.println(F("- Press once = Move to Pos1"));
  Serial.println(F("- Press again = Move to Pos2"));
}

void loop() {
  ArduinoOTA.handle(); // Handle OTA updates
  
  // Test mode
  if (testInProgress) {
    if (millis() - testStartTime >= 1000) { // 1 second between test steps
      testStartTime = millis();
      switch (testState) {
        case 0:
          smoothMove(servos[testServo], angles[testServo], pos1[testServo], testSpeed);
          testState = 1;
          break;
        case 1:
          smoothMove(servos[testServo], angles[testServo], pos2[testServo], testSpeed);
          testState = 2;
          break;
        default:
          smoothMove(servos[testServo], angles[testServo], 90, testSpeed);
          testInProgress = false;
          break;
      }
    }
  }

  // TOGGLE Button 1 handling (Servo 1) - GPIO 13
  buttonState = digitalRead(buttonPin) == LOW;
  if (buttonState && !lastButtonState) {
    // Button pressed - toggle between positions
    servoToggled = !servoToggled;
    if (servoToggled) {
      Serial.println(F("Button 1 Pressed - Moving Servo 1 to Pos1"));
      smoothMove(servos[0], angles[0], pos1[0], testSpeed);
    } else {
      Serial.println(F("Button 1 Pressed - Moving Servo 1 to Pos2"));
      smoothMove(servos[0], angles[0], pos2[0], testSpeed);
    }
    prefs.putInt("angle0", angles[0]);
    updateOLED(0, angles[0]);
  }
  lastButtonState = buttonState;

  // TOGGLE Button 2 handling (Servo 2) - GPIO 12
  servo2ButtonState = digitalRead(servo2ButtonPin) == LOW;
  if (servo2ButtonState && !lastServo2ButtonState) {
    // Button pressed - toggle between positions
    servo2Toggled = !servo2Toggled;
    if (servo2Toggled) {
      Serial.println(F("Button 2 Pressed - Moving Servo 2 to Pos1"));
      smoothMove(servos[1], angles[1], pos1[1], testSpeed);
    } else {
      Serial.println(F("Button 2 Pressed - Moving Servo 2 to Pos2"));
      smoothMove(servos[1], angles[1], pos2[1], testSpeed);
    }
    prefs.putInt("angle1", angles[1]);
    updateOLED(1, angles[1]);
  }
  lastServo2ButtonState = servo2ButtonState;

  handleIR();
}