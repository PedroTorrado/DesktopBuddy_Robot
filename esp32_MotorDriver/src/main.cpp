#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// On Seeed Studio XIAO ESP32S3:
// - Built-in LED is connected to GPIO 21 (active-low)
#ifndef LED_BUILTIN
#define LED_BUILTIN 21
#endif

// L298N Motor Driver Pin Connections mapping to XIAO ESP32S3
#define PIN_LEFT_ENA  D0  // Left Motor Speed (PWM) -> GPIO 1
#define PIN_LEFT_IN1  D1  // Left Motor Direction 1 -> GPIO 2
#define PIN_LEFT_IN2  D2  // Left Motor Direction 2 -> GPIO 3

#define PIN_RIGHT_ENB D3  // Right Motor Speed (PWM) -> GPIO 4
#define PIN_RIGHT_IN3 D4  // Right Motor Direction 1 -> GPIO 5
#define PIN_RIGHT_IN4 D5  // Right Motor Direction 2 -> GPIO 6

// Wi-Fi configuration (Station mode)
// Loaded from local secrets.h (git ignored for privacy)
#include "secrets.h"

// Wi-Fi Access Point configuration (AP mode)
// Starts automatically if connection to local router fails after 10 seconds
const char* ap_ssid = "Mimir-Robot-AP";
const char* ap_password = "mimircontroller";

WebServer server(80);

// HTML & CSS for the Web Dashboard interface
const char* dashboardHtml = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Mimir Motor Controller</title>
  <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg-gradient: radial-gradient(circle at center, #0f172a 0%, #020617 100%);
      --glass-bg: rgba(30, 41, 59, 0.45);
      --glass-border: rgba(255, 255, 255, 0.08);
      --text-primary: #f8fafc;
      --text-secondary: #94a3b8;
      --accent-grad: linear-gradient(135deg, #00f2fe 0%, #4facfe 100%);
      --accent-glow: rgba(0, 242, 254, 0.35);
      --danger-grad: linear-gradient(135deg, #ff0844 0%, #ffb199 100%);
      --danger-glow: rgba(255, 8, 68, 0.4);
    }
    
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      user-select: none;
      -webkit-user-select: none;
    }
    
    body {
      font-family: 'Outfit', sans-serif;
      background: var(--bg-gradient);
      color: var(--text-primary);
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      padding: 20px;
      overflow-x: hidden;
    }
    
    .container {
      background: var(--glass-bg);
      backdrop-filter: blur(16px) saturate(180%);
      -webkit-backdrop-filter: blur(16px) saturate(180%);
      border: 1px solid var(--glass-border);
      border-radius: 24px;
      padding: 40px;
      width: 100%;
      max-width: 440px;
      box-shadow: 0 20px 40px rgba(0, 0, 0, 0.4);
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 30px;
      animation: fadeIn 0.8s ease-out;
    }
    
    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(20px); }
      to { opacity: 1; transform: translateY(0); }
    }
    
    h1 {
      font-size: 2.2rem;
      font-weight: 800;
      letter-spacing: -0.5px;
      background: var(--accent-grad);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      text-align: center;
    }
    
    .status-badge {
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 0.9rem;
      color: var(--text-secondary);
      background: rgba(255, 255, 255, 0.04);
      padding: 6px 14px;
      border-radius: 20px;
      border: 1px solid rgba(255, 255, 255, 0.05);
    }
    
    .status-dot {
      width: 8px;
      height: 8px;
      background-color: #10b981;
      border-radius: 50%;
      box-shadow: 0 0 8px #10b981;
      animation: pulse 1.5s infinite alternate;
    }
    
    @keyframes pulse {
      0% { transform: scale(0.9); opacity: 0.6; }
      100% { transform: scale(1.2); opacity: 1; }
    }
    
    .d-pad {
      display: grid;
      grid-template-columns: repeat(3, 80px);
      grid-template-rows: repeat(3, 80px);
      gap: 15px;
      justify-content: center;
      margin: 10px 0;
    }
    
    .btn {
      width: 80px;
      height: 80px;
      border-radius: 20px;
      border: 1px solid var(--glass-border);
      background: rgba(255, 255, 255, 0.03);
      color: var(--text-primary);
      font-size: 1.8rem;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
      box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.1);
    }
    
    .btn:hover {
      background: rgba(255, 255, 255, 0.08);
      transform: translateY(-2px);
    }
    
    .btn:active, .btn.active {
      transform: scale(0.95);
      background: var(--accent-grad);
      border-color: transparent;
      box-shadow: 0 0 20px var(--accent-glow);
    }
    
    .btn.stop-btn {
      background: rgba(239, 68, 68, 0.15);
      border-color: rgba(239, 68, 68, 0.2);
      color: #f87171;
    }
    
    .btn.stop-btn:hover {
      background: rgba(239, 68, 68, 0.25);
    }
    
    .btn.stop-btn:active, .btn.stop-btn.active {
      background: var(--danger-grad);
      color: #ffffff;
      border-color: transparent;
      box-shadow: 0 0 20px var(--danger-glow);
    }
    
    .empty-space {
      visibility: hidden;
    }
    
    .speed-control {
      width: 100%;
      display: flex;
      flex-direction: column;
      gap: 12px;
      background: rgba(255, 255, 255, 0.02);
      padding: 20px;
      border-radius: 18px;
      border: 1px solid rgba(255, 255, 255, 0.04);
    }
    
    .speed-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    
    .speed-label {
      font-size: 0.95rem;
      font-weight: 600;
      color: var(--text-secondary);
      text-transform: uppercase;
      letter-spacing: 1px;
    }
    
    .speed-value {
      font-size: 1.2rem;
      font-weight: 800;
      color: #00f2fe;
      font-variant-numeric: tabular-nums;
    }
    
    .slider {
      -webkit-appearance: none;
      width: 100%;
      height: 6px;
      border-radius: 3px;
      background: rgba(255, 255, 255, 0.1);
      outline: none;
      transition: background 0.3s;
    }
    
    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 22px;
      height: 22px;
      border-radius: 50%;
      background: var(--text-primary);
      cursor: pointer;
      box-shadow: 0 0 10px rgba(0, 0, 0, 0.5);
      transition: transform 0.15s;
    }
    
    .slider::-webkit-slider-thumb:hover {
      transform: scale(1.15);
      background: #00f2fe;
    }
    
    .instructions {
      font-size: 0.85rem;
      color: var(--text-secondary);
      text-align: center;
      line-height: 1.5;
      padding: 0 10px;
    }
    
    kbd {
      background: rgba(255, 255, 255, 0.1);
      border: 1px solid rgba(255, 255, 255, 0.15);
      border-radius: 4px;
      padding: 2px 6px;
      font-family: monospace;
      font-size: 0.8rem;
      color: var(--text-primary);
      box-shadow: 0 2px 0 rgba(0, 0, 0, 0.2);
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>MIMIR DRIVE</h1>
    
    <div class="status-badge">
      <div class="status-dot"></div>
      <span>ESP32-S3 Connected</span>
    </div>
    
    <div class="d-pad">
      <div class="empty-space"></div>
      <button id="btn-up" class="btn" data-action="forward">▲</button>
      <div class="empty-space"></div>
      
      <button id="btn-left" class="btn" data-action="left">◀</button>
      <button id="btn-stop" class="btn stop-btn" data-action="stop">■</button>
      <button id="btn-right" class="btn" data-action="right">▶</button>
      
      <div class="empty-space"></div>
      <button id="btn-down" class="btn" data-action="backward">▼</button>
      <div class="empty-space"></div>
    </div>
    
    <div class="speed-control">
      <div class="speed-header">
        <span class="speed-label">Engine Power</span>
        <span class="speed-value" id="speed-display">70%</span>
      </div>
      <input type="range" min="30" max="100" value="70" class="slider" id="speed-slider">
    </div>
    
    <div class="instructions">
      Use screen controls or keyboard <kbd>▲</kbd> <kbd>▼</kbd> <kbd>◀</kbd> <kbd>▶</kbd> / <kbd>W</kbd> <kbd>A</kbd> <kbd>S</kbd> <kbd>D</kbd>.<br>
      Press <kbd>Space</kbd> or <kbd>Esc</kbd> to halt motors.
    </div>
  </div>

  <script>
    const buttons = {
      forward: document.getElementById('btn-up'),
      backward: document.getElementById('btn-down'),
      left: document.getElementById('btn-left'),
      right: document.getElementById('btn-right'),
      stop: document.getElementById('btn-stop')
    };
    
    const speedSlider = document.getElementById('speed-slider');
    const speedDisplay = document.getElementById('speed-display');
    
    let currentDir = 'stop';
    let currentSpeedPct = parseInt(speedSlider.value);
    
    speedSlider.addEventListener('input', (e) => {
      currentSpeedPct = parseInt(e.target.value);
      speedDisplay.textContent = currentSpeedPct + '%';
      if (currentDir !== 'stop') {
        sendMotorCommand(currentDir);
      }
    });
    
    function sendMotorCommand(direction) {
      currentDir = direction;
      
      const maxSpeed = 255;
      const speedVal = Math.round((currentSpeedPct / 100) * maxSpeed);
      
      let left = 0;
      let right = 0;
      
      switch(direction) {
        case 'forward':
          left = -speedVal;
          right = -speedVal;
          break;
        case 'backward':
          left = speedVal;
          right = speedVal;
          break;
        case 'left':
          left = speedVal;
          right = -speedVal;
          break;
        case 'right':
          left = -speedVal;
          right = speedVal;
          break;
        case 'stop':
        default:
          left = 0;
          right = 0;
          break;
      }
      
      fetch(`/motor?left=${left}&right=${right}`)
        .catch(err => console.error('Error sending command:', err));
    }
    
    function attachButtonEvents(btnId, direction) {
      const btn = buttons[btnId];
      
      const pressHandler = (e) => {
        e.preventDefault();
        if (currentDir === direction) return;
        
        Object.values(buttons).forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        
        sendMotorCommand(direction);
      };
      
      const releaseHandler = (e) => {
        e.preventDefault();
        if (currentDir === 'stop' || currentDir !== direction) return;
        
        btn.classList.remove('active');
        sendMotorCommand('stop');
      };
      
      btn.addEventListener('mousedown', pressHandler);
      btn.addEventListener('mouseup', releaseHandler);
      btn.addEventListener('mouseleave', releaseHandler);
      
      btn.addEventListener('touchstart', pressHandler);
      btn.addEventListener('touchend', releaseHandler);
      btn.addEventListener('touchcancel', releaseHandler);
    }
    
    buttons.stop.addEventListener('click', () => {
      Object.values(buttons).forEach(b => b.classList.remove('active'));
      sendMotorCommand('stop');
    });
    
    attachButtonEvents('forward', 'forward');
    attachButtonEvents('backward', 'backward');
    attachButtonEvents('left', 'left');
    attachButtonEvents('right', 'right');
    
    const keyMap = {
      'ArrowUp': { dir: 'forward', btn: buttons.forward },
      'w': { dir: 'forward', btn: buttons.forward },
      'W': { dir: 'forward', btn: buttons.forward },
      
      'ArrowDown': { dir: 'backward', btn: buttons.backward },
      's': { dir: 'backward', btn: buttons.backward },
      'S': { dir: 'backward', btn: buttons.backward },
      
      'ArrowLeft': { dir: 'left', btn: buttons.left },
      'a': { dir: 'left', btn: buttons.left },
      'A': { dir: 'left', btn: buttons.left },
      
      'ArrowRight': { dir: 'right', btn: buttons.right },
      'd': { dir: 'right', btn: buttons.right },
      'D': { dir: 'right', btn: buttons.right },
      
      ' ': { dir: 'stop', btn: buttons.stop },
      'Escape': { dir: 'stop', btn: buttons.stop }
    };
    
    let activeKey = null;
    
    window.addEventListener('keydown', (e) => {
      const match = keyMap[e.key];
      if (match) {
        if (activeKey === e.key) return;
        activeKey = e.key;
        
        Object.values(buttons).forEach(b => b.classList.remove('active'));
        match.btn.classList.add('active');
        sendMotorCommand(match.dir);
      }
    });
    
    window.addEventListener('keyup', (e) => {
      if (activeKey === e.key) {
        activeKey = null;
        const match = keyMap[e.key];
        if (match) {
          match.btn.classList.remove('active');
          if (match.dir !== 'stop') {
            sendMotorCommand('stop');
          }
        }
      }
    });
  </script>
</body>
</html>
)rawliteral";

// Direct Motor Speed and Direction logic
void controlMotors(int leftSpeed, int rightSpeed) {
  Serial.printf("Motor Control -> Left: %4d | Right: %4d\n", leftSpeed, rightSpeed);

  // Left Motor control
  if (leftSpeed > 0) {
    digitalWrite(PIN_LEFT_IN1, HIGH);
    digitalWrite(PIN_LEFT_IN2, LOW);
    analogWrite(PIN_LEFT_ENA, leftSpeed);
  } else if (leftSpeed < 0) {
    digitalWrite(PIN_LEFT_IN1, LOW);
    digitalWrite(PIN_LEFT_IN2, HIGH);
    analogWrite(PIN_LEFT_ENA, -leftSpeed);
  } else {
    digitalWrite(PIN_LEFT_IN1, LOW);
    digitalWrite(PIN_LEFT_IN2, LOW);
    analogWrite(PIN_LEFT_ENA, 0);
  }

  // Right Motor control
  if (rightSpeed > 0) {
    digitalWrite(PIN_RIGHT_IN3, HIGH);
    digitalWrite(PIN_RIGHT_IN4, LOW);
    analogWrite(PIN_RIGHT_ENB, rightSpeed);
  } else if (rightSpeed < 0) {
    digitalWrite(PIN_RIGHT_IN3, LOW);
    digitalWrite(PIN_RIGHT_IN4, HIGH);
    analogWrite(PIN_RIGHT_ENB, -rightSpeed);
  } else {
    digitalWrite(PIN_RIGHT_IN3, LOW);
    digitalWrite(PIN_RIGHT_IN4, LOW);
    analogWrite(PIN_RIGHT_ENB, 0);
  }
}

// Serve root webpage
void handleRoot() {
  server.send(200, "text/html", dashboardHtml);
}

// Handle GET motor speed request
void handleMotor() {
  int leftVal = 0;
  int rightVal = 0;

  if (server.hasArg("left")) {
    leftVal = server.arg("left").toInt();
  }
  if (server.hasArg("right")) {
    rightVal = server.arg("right").toInt();
  }

  // Constrain speeds to safe ranges (-255 to 255)
  leftVal = constrain(leftVal, -255, 255);
  rightVal = constrain(rightVal, -255, 255);

  controlMotors(leftVal, rightVal);
  
  // Respond to client
  server.send(200, "text/plain", "OK");
}

#define BOOT_BUTTON_PIN 0

// Variables for periodic IP printing and BOOT button debouncing
unsigned long lastIpPrintTime = 0;
const unsigned long ipPrintInterval = 10000; // Print every 10 seconds

bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
bool buttonPressed = false;

// Helper function to print IP Address to Serial
void printIPAddress() {
  Serial.println("\n--- Mimir Network Info ---");
  wifi_mode_t mode = WiFi.getMode();
  if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
    Serial.println("Mode: Access Point (AP)");
    Serial.printf("SSID: %s\n", ap_ssid);
    Serial.print("Web Server IP: http://");
    Serial.println(WiFi.softAPIP());
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Mode: Station (STA)");
    Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
    Serial.print("Web Server IP: http://");
    Serial.println(WiFi.localIP());
    Serial.printf("Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
  } else if (mode != WIFI_MODE_AP && mode != WIFI_MODE_APSTA) {
    Serial.println("Status: Disconnected");
  }
  Serial.println("---------------------------\n");
}

void setup() {
  Serial.begin(115200);
  // Wait up to 2s for native USB Serial
  unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart < 2000)) {
    delay(10);
  }
  Serial.println("\n=== Mimir Motor Driver Initialization ===");

  // Set built-in status LED pin
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // Off initially (active-low)

  // Configure BOOT button pin as input
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // Configure Motor Pins as output
  pinMode(PIN_LEFT_ENA, OUTPUT);
  pinMode(PIN_LEFT_IN1, OUTPUT);
  pinMode(PIN_LEFT_IN2, OUTPUT);
  pinMode(PIN_RIGHT_ENB, OUTPUT);
  pinMode(PIN_RIGHT_IN3, OUTPUT);
  pinMode(PIN_RIGHT_IN4, OUTPUT);

  // Stop motors initially
  controlMotors(0, 0);

  // Try to connect to station Wi-Fi
  Serial.printf("Attempting to connect to Wi-Fi SSID: %s\n", ssid);
  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();
  bool connected = true;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // Flash status LED during connection attempt
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

    if (millis() - startAttempt > 10000) {
      Serial.println("\nWi-Fi connection timed out. Switching to AP mode...");
      connected = false;
      break;
    }
  }

  if (connected) {
    // Solid status LED when connected to Station Wi-Fi
    digitalWrite(LED_BUILTIN, LOW); // ON (active-low)
    Serial.println("\nWi-Fi successfully connected!");
  } else {
    // Set up own Access Point
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    
    // Solid status LED in AP Mode as well
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("Wi-Fi Access Point (AP) Started.");
  }

  // Set Web Server route handlers
  server.on("/", handleRoot);
  server.on("/motor", handleMotor);

  server.begin();
  Serial.println("HTTP Web Server Started successfully.");
  
  // Initial print of the IP Address
  printIPAddress();
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();

  // 1. Periodically print the IP address (every 10 seconds)
  if (currentMillis - lastIpPrintTime >= ipPrintInterval) {
    lastIpPrintTime = currentMillis;
    printIPAddress();
  }

  // 2. Read the BOOT button. Pressing it triggers an instant status print.
  int reading = digitalRead(BOOT_BUTTON_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis;
  }

  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && !buttonPressed) {
      buttonPressed = true;
      Serial.println("\n[BOOT Button Triggered]");
      printIPAddress();
    } else if (reading == HIGH && buttonPressed) {
      buttonPressed = false;
    }
  }

  lastButtonState = reading;
}