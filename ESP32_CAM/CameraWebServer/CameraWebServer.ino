#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15 
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

// ===================
// Select camera model
// ===================
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
//#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD
//#define CAMERA_MODEL_ESP32S3_CAM_LCD
//#define CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3 // Has PSRAM
//#define CAMERA_MODEL_DFRobot_Romeo_ESP32S3 // Has PSRAM
#include "camera_pins.h"

#include "secrets.h"
#include "FS.h"
#include "SD_MMC.h"
#include <Preferences.h>

#include "app_httpd.h"

struct WiFiCreds {
  String ssid;
  String password;
};

WiFiCreds wifiList[10];
int wifiCount = 0;
Preferences prefs;
String lastSSID = "";
String lastPassword = "";

void loadWiFiFromSD() {
  Serial.println("Initializing SD Card in 1-bit mode...");
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card mount failed. Using fallback credentials.");
    return;
  }

  File file = SD_MMC.open("/wifi.txt");
  if (!file) {
    Serial.println("Failed to open /wifi.txt. Using fallback credentials.");
    SD_MMC.end();
    return;
  }

  Serial.println("Parsing /wifi.txt...");
  wifiCount = 0;
  while (file.available() && wifiCount < 10) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.startsWith("#")) continue;
    
    int commaIdx = line.indexOf(',');
    if (commaIdx != -1) {
      String s = line.substring(0, commaIdx);
      String p = line.substring(commaIdx + 1);
      s.trim();
      p.trim();
      if (s.length() > 0) {
        wifiList[wifiCount].ssid = s;
        wifiList[wifiCount].password = p;
        wifiCount++;
      }
    }
  }
  file.close();
  SD_MMC.end();
  Serial.printf("Parsed %d WiFi credentials from SD card.\n", wifiCount);
}

void loadLastWiFi() {
  prefs.begin("wifi-store", false);
  lastSSID = prefs.getString("last-ssid", "");
  lastPassword = prefs.getString("last-pass", "");
  prefs.end();
}

void saveLastWiFi(String s, String p) {
  prefs.begin("wifi-store", false);
  prefs.putString("last-ssid", s);
  prefs.putString("last-pass", p);
  prefs.end();
}

void sendWiFiListToNucleo() {
  if (wifiCount == 0) {
    Serial.printf("WIFILIST:%s\n", ssid);
    return;
  }
  
  String msg = "WIFILIST:";
  for (int i = 0; i < wifiCount; i++) {
    msg += wifiList[i].ssid;
    if (i < wifiCount - 1) {
      msg += ",";
    }
  }
  msg += "\n";
  Serial.print(msg);
}

void connectWiFiOnBoot() {
  loadWiFiFromSD();
  loadLastWiFi();

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_15dBm);

  if (lastSSID.length() > 0) {
    Serial.printf("Connecting to last used WiFi: %s\n", lastSSID.c_str());
    Serial.printf("WIFISTATUS:CONNECTING,%s\n", lastSSID.c_str());
    WiFi.begin(lastSSID.c_str(), lastPassword.c_str());
    
    int retries = 20;
    while (WiFi.status() != WL_CONNECTED && retries > 0) {
      delay(500);
      Serial.print(".");
      retries--;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected to last used!");
      Serial.println("WIFISTATUS:CONNECTED");
      return;
    }
    Serial.println("\nFailed to connect to last used WiFi.");
  }

  for (int i = 0; i < wifiCount; i++) {
    if (wifiList[i].ssid == lastSSID) continue;
    
    Serial.printf("Trying SD WiFi: %s\n", wifiList[i].ssid.c_str());
    Serial.printf("WIFISTATUS:CONNECTING,%s\n", wifiList[i].ssid.c_str());
    WiFi.begin(wifiList[i].ssid.c_str(), wifiList[i].password.c_str());
    
    int retries = 20;
    while (WiFi.status() != WL_CONNECTED && retries > 0) {
      delay(500);
      Serial.print(".");
      retries--;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected to SD list entry!");
      Serial.println("WIFISTATUS:CONNECTED");
      saveLastWiFi(wifiList[i].ssid, wifiList[i].password);
      return;
    }
    Serial.println("\nFailed.");
  }

  Serial.printf("Trying compiled fallback WiFi: %s\n", ssid);
  Serial.printf("WIFISTATUS:CONNECTING,%s\n", ssid);
  WiFi.begin(ssid, password);
  int retries = 20;
  while (WiFi.status() != WL_CONNECTED && retries > 0) {
    delay(500);
    Serial.print(".");
    retries--;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected to compiled fallback!");
    Serial.println("WIFISTATUS:CONNECTED");
    saveLastWiFi(ssid, password);
  } else {
    Serial.println("\nAll WiFi connections failed.");
    Serial.println("WIFISTATUS:FAILED");
  }
}

void connectToSelectedWiFi(int idx) {
  if (idx < 0 || idx >= wifiCount) {
    Serial.println("Invalid WiFi index selected.");
    Serial.println("WIFISTATUS:FAILED");
    return;
  }
  
  String selected_ssid = wifiList[idx].ssid;
  String selected_pass = wifiList[idx].password;
  
  Serial.printf("Connecting to requested WiFi: %s\n", selected_ssid.c_str());
  Serial.printf("WIFISTATUS:CONNECTING,%s\n", selected_ssid.c_str());
  
  WiFi.disconnect();
  WiFi.begin(selected_ssid.c_str(), selected_pass.c_str());
  
  int retries = 20;
  while (WiFi.status() != WL_CONNECTED && retries > 0) {
    delay(500);
    Serial.print(".");
    retries--;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected to selected!");
    Serial.println("WIFISTATUS:CONNECTED");
    saveLastWiFi(selected_ssid, selected_pass);
  } else {
    Serial.println("\nFailed to connect to selected WiFi.");
    Serial.println("WIFISTATUS:FAILED");
  }
}

WiFiClientSecure net;
PubSubClient mqttClient(net);

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  Serial.printf("[MQTT RX] Topic: %s, Message: %s\n", topic, message);
  
  int rateIdx = String(message).indexOf("refresh_rate");
  if (rateIdx != -1) {
    int colonIdx = String(message).indexOf(':', rateIdx);
    if (colonIdx != -1) {
      int rateVal = String(message).substring(colonIdx + 1).toInt();
      if (rateVal >= 500 && rateVal <= 10000) {
        Serial.printf("SETINTERVAL:%d\n", rateVal);
      }
    }
  }
}

void connectAWS() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  static bool certsConfigured = false;
  if (!certsConfigured) {
    net.setCACert(AWS_CERT_CA);
    net.setCertificate(AWS_CERT_CRT);
    net.setPrivateKey(AWS_CERT_PRIVATE);
    mqttClient.setServer(AWS_IOT_ENDPOINT, AWS_IOT_PORT);
    mqttClient.setCallback(mqttCallback);
    certsConfigured = true;
  }

  Serial.println("Attempting to connect to AWS IoT...");
  if (mqttClient.connect(AWS_IOT_CLIENT_ID)) {
    Serial.println("Connected to AWS IoT successfully!");
    mqttClient.subscribe("esp32/face_tracker/control");
  } else {
    Serial.print("AWS IoT connection failed, rc=");
    Serial.println(mqttClient.state());
  }
}

// Globals for thread-safe MQTT publishing
volatile bool newFaceDataReady = false;
volatile int global_face_id = -2;
volatile int global_face_x = 0;
volatile int global_face_y = 0;

void publishFaceData(int face_id, int x, int y) {
  // Transmit to Nucleo over Serial (since UART0/Serial is damaged)
  Serial.printf("FACE:%d,%d,%d\n", face_id, x, y);

  // This function is called by the Camera FreeRTOS task.
  // DO NOT call mqttClient.publish() here, it causes socket crashes!
  // Instead, hand it off to the main loop() task.
  global_face_id = face_id;
  global_face_x = x;
  global_face_y = y;
  newFaceDataReady = true;
}

#define BUTTON_PIN 13
extern int led_duty; // Declared in app_httpd.cpp

// Globals for Webserver Telemetry
volatile int telemetry_co2 = 0;
volatile int telemetry_tvoc = 0;
volatile int telemetry_distance = 0;
volatile int telemetry_emotion = 0;

// Globals for Webserver WiFi Status
char wifi_ssid_str[33] = "Connecting...";
int wifi_rssi_val = -100;
char wifi_ip_str[16] = "0.0.0.0";

// Variables for button debouncing and state
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // 50 ms
int currentLevelIndex = 0;
const int brightnessLevels[] = {0, 10, 50, 120, 255};
const int numLevels = sizeof(brightnessLevels) / sizeof(brightnessLevels[0]);

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector to survive Wi-Fi power spikes
  
  Serial.begin(115200, SERIAL_8N1, 3, 1);
  Serial.setTimeout(10); // Reduce default blocking read timeout from 1000ms to 10ms
  Serial.setDebugOutput(true);
  Serial.println();

  // Configure onboard red LED (GPIO33) as output for serial diagnostics
  pinMode(33, OUTPUT);
  digitalWrite(33, HIGH); // Turn off initially (Active Low)

  // 1. Connect Wi-Fi first before turning on the power-hungry camera sensor
  connectWiFiOnBoot();
  WiFi.setSleep(false);

  // 2. Now initialize camera sensor configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 16;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_QVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  if(config.pixel_format == PIXFORMAT_JPEG){
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  // 3. NTP Time Synchronization
  if (WiFi.status() == WL_CONNECTED) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Syncing time");
    time_t now_t = time(nullptr);
    int time_retries = 20;
    while (now_t < 100000 && time_retries > 0) { 
      delay(500);
      Serial.print(".");
      now_t = time(nullptr);
      time_retries--;
    }
    if (now_t >= 100000) {
      Serial.println("\nTime synced successfully!");
    } else {
      Serial.println("\nTime sync timed out!");
    }
  }

  // Allow Nucleo to start up, then send the parsed WiFi SSIDs
  delay(1000);
  sendWiFiListToNucleo();

  // Blink LED to signal connection status
  const int safe_brightness = 2;
  ledcWrite(2, safe_brightness); 
  delay(200);
  ledcWrite(2, 0); 
  delay(200);
  ledcWrite(2, safe_brightness); 
  delay(200);
  ledcWrite(2, 0); 

  startCameraServer();

  // Configure physical button pin at the very end of setup() to prevent SD override
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

unsigned long lastMqttRetry = 0;

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      if (millis() - lastMqttRetry > 5000) {
        lastMqttRetry = millis();
        connectAWS();
      }
    } else {
      mqttClient.loop();
    }
  }

  // Safely publish face data from the main task
  if (newFaceDataReady) {
    newFaceDataReady = false;
    static bool wasFaceDetected = false;
    int f_id = global_face_id;
    int f_x = global_face_x;
    int f_y = global_face_y;

    bool shouldPublish = false;

    if (f_id == -2) {
      // If we previously had a face, we want to publish the "lost" message exactly once
      if (wasFaceDetected) {
        wasFaceDetected = false;
        shouldPublish = true; 
      }
    } else {
      // Face actively detected, publish it!
      wasFaceDetected = true;
      shouldPublish = true;
    }

    if (shouldPublish) {
      if (mqttClient.connected()) {
        char payload[64];
        snprintf(payload, sizeof(payload), "{\"face_id\": %d, \"x\": %d, \"y\": %d}", f_id, f_x, f_y);
        mqttClient.publish(AWS_IOT_TOPIC, payload);
      }
    }
  }

  // Heartbeat blink to verify code is running and LED is on Pin 33
  static uint32_t last_heartbeat = 0;
  static bool ledState = true;
  if (millis() - last_heartbeat >= 500) {
    last_heartbeat = millis();
    ledState = !ledState;
    digitalWrite(33, ledState ? HIGH : LOW);
  }

  // Read Telemetry from Nucleo
  if (Serial.available()) {
    digitalWrite(33, LOW); // Turn on red LED (active LOW) to show physical data reception
    String incoming = Serial.readStringUntil('\n');
    digitalWrite(33, HIGH); // Turn off red LED
    incoming.trim(); // Clean up any stray \r
    
    if (incoming.length() > 0) {
      if (incoming.startsWith("REQWIFILIST")) {
        Serial.printf("[ESP32 DEBUG] Received REQWIFILIST. wifiCount = %d. Sending list...\n", wifiCount);
        sendWiFiListToNucleo();
      } else if (incoming.startsWith("SELWIFI:")) {
        int idx = incoming.substring(8).toInt();
        connectToSelectedWiFi(idx);
      } else if (incoming.startsWith("PING")) {
        Serial.println("PONG");
      }

      // Publish raw string to debug topic to see what the ESP32 is physically hearing!
      if (mqttClient.connected()) {
        char dbgPayload[128];
        snprintf(dbgPayload, sizeof(dbgPayload), "{\"debug_raw\": \"%s\"}", incoming.c_str());
        mqttClient.publish("esp32/face_tracker/debug", dbgPayload);
      }
    }

    int telIdx = incoming.indexOf("TELEMETRY:");
    if (telIdx != -1) {
      int startIdx = telIdx + 10;
      int nextComma = incoming.indexOf(',', startIdx);
      if (nextComma != -1) {
        int co2 = incoming.substring(startIdx, nextComma).toInt();
        startIdx = nextComma + 1;
        nextComma = incoming.indexOf(',', startIdx);
        int tvoc = incoming.substring(startIdx, nextComma).toInt();
        startIdx = nextComma + 1;
        nextComma = incoming.indexOf(',', startIdx);
        int dist = incoming.substring(startIdx, nextComma).toInt();
        startIdx = nextComma + 1;
        int emotion = incoming.substring(startIdx).toInt();

        // Update globals for web page telemetry
        telemetry_co2 = co2;
        telemetry_tvoc = tvoc;
        telemetry_distance = dist;
        telemetry_emotion = emotion;

        if (mqttClient.connected()) {
          char payload[128];
          snprintf(payload, sizeof(payload), "{\"co2\": %d, \"tvoc\": %d, \"distance\": %d, \"emotion\": %d}", co2, tvoc, dist, emotion);
          mqttClient.publish(AWS_IOT_TOPIC, payload);
        }
      }
    }
  }

  // Read and debounce physical button to cycle Flash LED intensity
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    if (millis() - lastDebounceTime > debounceDelay) {
      if (reading == LOW) { // Button press detected
        currentLevelIndex = (currentLevelIndex + 1) % numLevels;
        led_duty = brightnessLevels[currentLevelIndex];
        ledcWrite(2, led_duty);
        Serial.printf("[BUTTON] LED Flash Intensity set to: %d (Level %d/%d)\n", 
                      led_duty, currentLevelIndex, numLevels - 1);
      }
      lastDebounceTime = millis();
    }
  }
  lastButtonState = reading;

  // Update WiFi status globals for thread-safe access by webserver
  static uint32_t last_wifi_check = 0;
  if (millis() - last_wifi_check >= 2000) {
    last_wifi_check = millis();
    if (WiFi.status() == WL_CONNECTED) {
      String current_ssid = WiFi.SSID();
      current_ssid.replace("\"", "'");
      strncpy(wifi_ssid_str, current_ssid.c_str(), sizeof(wifi_ssid_str) - 1);
      wifi_ssid_str[sizeof(wifi_ssid_str) - 1] = '\0';
      
      wifi_rssi_val = WiFi.RSSI();
      
      IPAddress ip = WiFi.localIP();
      snprintf(wifi_ip_str, sizeof(wifi_ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    } else {
      strcpy(wifi_ssid_str, "Disconnected");
      wifi_rssi_val = -100;
      strcpy(wifi_ip_str, "0.0.0.0");
    }
  }

  delay(10);
}