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

WiFiClientSecure net;
PubSubClient mqttClient(net);

void connectAWS() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  static bool certsConfigured = false;
  if (!certsConfigured) {
    net.setCACert(AWS_CERT_CA);
    net.setCertificate(AWS_CERT_CRT);
    net.setPrivateKey(AWS_CERT_PRIVATE);
    mqttClient.setServer(AWS_IOT_ENDPOINT, AWS_IOT_PORT);
    certsConfigured = true;
  }

  Serial.println("Attempting to connect to AWS IoT...");
  if (mqttClient.connect(AWS_IOT_CLIENT_ID)) {
    Serial.println("Connected to AWS IoT successfully!");
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
  // Transmit to Nucleo over Serial2 (since UART0/Serial is damaged)
  Serial2.printf("FACE:%d,%d,%d\n", face_id, x, y);

  // This function is called by the Camera FreeRTOS task.
  // DO NOT call mqttClient.publish() here, it causes socket crashes!
  // Instead, hand it off to the main loop() task.
  global_face_id = face_id;
  global_face_x = x;
  global_face_y = y;
  newFaceDataReady = true;
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector to survive Wi-Fi power spikes
  
  Serial.begin(115200, SERIAL_8N1, 3, 1);
  Serial.setDebugOutput(true);
  Serial.println();

  // Initialize Serial2 on alternative pins to bypass the dead U0R pin
  // GPIO13 = RX2, GPIO12 = TX2
  Serial2.begin(115200, SERIAL_8N1, 13, 12);

  // Configure onboard red LED (GPIO33) as output for serial diagnostics
  pinMode(33, OUTPUT);
  digitalWrite(33, HIGH); // Turn off initially (Active Low)

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
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 16;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
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
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
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

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  // Sync time via NTP. This is REQUIRED for AWS IoT Core so WiFiClientSecure can validate the TLS certificate expiration date!
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Syncing time");
  time_t now = time(nullptr);
  while (now < 100000) { 
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\nTime synced successfully!");

  // Blink LED at a low brightness to signal connection
  const int safe_brightness = 2;

  ledcWrite(2, safe_brightness); 
  delay(200);
  ledcWrite(2, 0); 
  delay(200);
  ledcWrite(2, safe_brightness); 
  delay(200);
  ledcWrite(2, 0); 

  startCameraServer();

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

  // Exciting status-based LED patterns (GPIO33 is active LOW)
  uint32_t now = millis();
  if (!mqttClient.connected()) {
    // AWS Disconnected -> Rapid Warning Blink (100ms on, 100ms off)
    static uint32_t last_warning_blink = 0;
    if (now - last_warning_blink >= 100) {
      last_warning_blink = now;
      digitalWrite(33, !digitalRead(33));
    }
  } else if (global_face_id >= 0) {
    // Target Locked (Face Tracking) -> Solid ON (Excited state)
    digitalWrite(33, LOW);
  } else {
    // Connected but Searching -> Double-Blink heartbeat (100ms on, 100ms off, 100ms on, 700ms off)
    int cycleTime = now % 1000;
    if (cycleTime < 100) {
      digitalWrite(33, LOW); // 1st blink ON
    } else if (cycleTime < 200) {
      digitalWrite(33, HIGH); // OFF
    } else if (cycleTime < 300) {
      digitalWrite(33, LOW); // 2nd blink ON
    } else {
      digitalWrite(33, HIGH); // OFF
    }
  }

  // Read Telemetry from Nucleo
  if (Serial2.available()) {
    digitalWrite(33, LOW); // Turn on red LED (active LOW) to show physical data reception
    String incoming = Serial2.readStringUntil('\n');
    digitalWrite(33, HIGH); // Turn off red LED
    incoming.trim(); // Clean up any stray \r
    
    if (incoming.length() > 0) {
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

        if (mqttClient.connected()) {
          char payload[128];
          snprintf(payload, sizeof(payload), "{\"co2\": %d, \"tvoc\": %d, \"distance\": %d, \"emotion\": %d}", co2, tvoc, dist, emotion);
          mqttClient.publish(AWS_IOT_TOPIC, payload);
        }
      }
    }
  }

  delay(10);
}