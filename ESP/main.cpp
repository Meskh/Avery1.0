#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>  

// motor pin and channel definition 
const int motorPins[5] = {13, 2, 12, 15, 14}; 
const int channels[5] = {0, 1, 2, 3, 4}; 
const int freq = 5000;    
const int resolutionMotor = 8; // 8-bit resolution (0-255)
int motorIntensities[5] = {0,0,0,0,0};
int motorsOn[5] = {255, 255, 255, 255, 255};
int motorsOff[5] = {0, 0, 0, 0, 0};

#define CAMERA_MODEL_AI_THINKER  // ESP32-CAM model for camera_pins.h value retrieval 

#include "camera_pins.h"


const char* ssid = "Glide";
const char* password = "";

// Web server on port 80
WebServer server(80);

// IP address - will be set when connected
IPAddress localIP;

// Camera configuration
camera_config_t config;

// map motorIntesities array onto output pins
void setIntensities(int intensities[5]) {
  for (int pin = 0; pin < 5; pin++) {
      ledcWrite(channels[pin], intensities[pin]); 
  }
}
// Serve the HTML page for testing
void handleRoot() {
  String html = "<!DOCTYPE html>\n";
  html += "<html>\n";
  html += "<head>\n";
  html += "  <title>ESP32-CAM Stream</title>\n";
  html += "  <style>\n";
  html += "    body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }\n";
  html += "    img { max-width: 800px; border: 1px solid #ddd; }\n";
  html += "    .container { margin: 0 auto; max-width: 840px; }\n";
  html += "    h1 { color: #333; }\n";
  html += "    .info { background-color: #f8f9fa; padding: 10px; border-radius: 4px; margin: 20px 0; }\n";
  html += "  </style>\n";
  html += "</head>\n";
  html += "<body>\n";
  html += "  <div class='container'>\n";
  html += "    <h1>ESP32-CAM Stream</h1>\n";
  html += "    <div class='stream'>\n";
  html += "      <img src='/stream' id='stream'>\n";
  html += "    </div>\n";
  html += "    <div class='info'>\n";
  html += "      <p>For torchcam integration, use this URL:</p>\n";
  html += "      <code>http://" + localIP.toString() + "/capture</code>\n";
  html += "      <p>To send data to ESP32, use:</p>\n";
  html += "      <code>http://" + localIP.toString() + "/send_data</code> (POST request with JSON data)\n";
  html += "    </div>\n";
  html += "  </div>\n";
  html += "</body>\n";
  html += "</html>\n";
  
  server.send(200, "text/html", html);
}
// Handle video streaming
void handleStream() {
  WiFiClient client = server.client();
  
  // MJPEG header
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println();
  
  while (client.connected()) {
    // Capture a frame
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      delay(1000);
      continue;
    }
    
    // Send frame with MJPEG format
    client.println("--frame");
    client.println("Content-Type: image/jpeg");
    client.print("Content-Length: ");
    client.println(fb->len);
    client.println();
    client.write(fb->buf, fb->len);
    client.println();
    
    // Return the frame buffer to be reused
    esp_camera_fb_return(fb);
    
    // Small delay to prevent flooding
    delay(40); // ~25 fps                         
  }
}
// Handle single image capture 
void handleCapture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }
  
  // Set correct content type
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Disposition", "inline; filename=capture.jpg");
  
  // Send the image
  server.sendHeader("Content-Length", String(fb->len));
  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
  
  // Return the frame buffer to be reused
  esp_camera_fb_return(fb);
}
// Handle receiving data from Python
void handleSendData() {
  // Check if we have data
  if (server.hasArg("plain") == false) {
    server.send(400, "text/plain", "Missing data");
    return;
  }
  
  // Get the data from the request
  String json = server.arg("plain");
  Serial.println("Received data from Python:");
  Serial.println(json);
  
  // Parse the JSON data
  StaticJsonDocument<1024> doc;  // Adjust size as needed
  DeserializationError error = deserializeJson(doc, json);
  
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    server.send(400, "text/plain", "Invalid JSON data");
    return;
  }
  
  // Process and print the array data
  if (doc.containsKey("data") && doc["data"].is<JsonArray>()) {
    JsonArray array = doc["data"].as<JsonArray>();

    // map from 0-1 to 0-255 for intensities
    for (int i = 0; i < 5; i++) {
      motorIntensities[i] = round(array[i].as<float>() * 255);
      if (motorIntensities[i] > 255) {
        motorIntensities[i] = 255;
      } 
    }
    setIntensities(motorIntensities);
  }
  // Send success response
  server.send(200, "text/plain", "Data received and processed");
}
// demo used for initialisation 
void simpleDemo() {
    for (int i = 0; i < 5; i++) {
      delay(200);
      ledcWrite(channels[i], 255); 
      delay(300);
      ledcWrite(channels[i], 0); 
    }
}
// demo for sensory effect
void fastDemo() {
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 5; j++) {
      ledcWrite(channels[j], 255); 
      delay(60);
      ledcWrite(channels[j], 0); 
    }
    for (int j = 4; j > -1; j--) {
      ledcWrite(channels[j], 255); 
      delay(60);
      ledcWrite(channels[j], 0); 
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Camera Web Server");

  // Initialize motor PWM channels
  for (int pin = 0; pin < 5; pin++) {
    ledcSetup(channels[pin], freq, resolutionMotor);
    ledcAttachPin(motorPins[pin], channels[pin]);
  }

  // Configure camera
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Initialize with high specs for higher JPEG quality
  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;  // 800x600
    config.jpeg_quality = 10;            // 0-63, lower means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_VGA;   // 640x480
    config.jpeg_quality = 12;            // 0-63, lower means higher quality
    config.fb_count = 1;
  }

  // Camera initialization
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    setIntensities(motorsOn);
    delay(1000);
    setIntensities(motorsOff);
    ESP.restart();
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    setIntensities(motorsOn);
    delay(50);
    setIntensities(motorsOff);
  }
  Serial.println("");
  
  // Print the IP address
  localIP = WiFi.localIP();
  Serial.print("Camera Stream Ready! Connect to http://");
  Serial.println(localIP);

  // indicate wifi connection
  for (int i = 0; i < 3; i++) {
    setIntensities(motorsOn);
    delay(50);
    setIntensities(motorsOff);
    delay(100);
  }
  // demos for correct motor connection check
  delay(1000);
  simpleDemo();
  fastDemo();

  // Route for a simple HTML interface
  server.on("/", HTTP_GET, handleRoot);
  
  // Route for streaming MJPEG video
  server.on("/stream", HTTP_GET, handleStream);
  
  // Route for getting a single image
  server.on("/capture", HTTP_GET, handleCapture);
  
  // NEW: Route for receiving data from Python
  server.on("/send_data", HTTP_POST, handleSendData);
  
  // Start server
  server.begin();
}

void loop() {
  server.handleClient();
}