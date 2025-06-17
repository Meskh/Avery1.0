#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>

// motor pin and channel definition 
const int motorPins[7] = {14, 1, 2, 15, 13, 12, 3}; 
const int channels[7] = {0, 1, 2, 3, 4, 5, 6}; 
const int freq = 5000;    
const int resolutionMotor = 8; // 8-bit resolution (0-255)
int motorIntensities[7] = {0,0,0,0,0,0,0};
int motorsOn[7] = {255, 255, 255, 255, 255, 255, 255};
int motorsOff[7] = {0, 0, 0, 0, 0, 0, 0};

#define CAMERA_MODEL_AI_THINKER  // ESP32-CAM model for camera_pins.h value retrieval 

// Include the camera pins header file
#include "camera_pins.h"

// Access Point settings
const char* ap_ssid = "ESP32-CAM-AP";  
const char* ap_password = "ALAMAJO123"; 

// Web server on port 80
WebServer server(80);

// WebSocket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

IPAddress localIP;

// Camera configuration
camera_config_t config;

// map motorIntesities array onto output pins
void setIntensities(int intensities[5]) {
  for (int pin = 0; pin < 7; pin++) {
      ledcWrite(channels[pin], intensities[pin]); 
  }
}

// WebSocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
      }
      break;
    case WStype_TEXT:
      // Parse JSON from client
      {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
          if (doc.containsKey("data") && doc["data"].is<JsonArray>()) {
            JsonArray array = doc["data"].as<JsonArray>();
            for (int i = 0; i < 7 && i < array.size(); i++) {
              motorIntensities[i] = round(array[i].as<float>() * 255);
              if (motorIntensities[i] > 255) {
                motorIntensities[i] = 255;
              } 
              if (motorIntensities[i] < 60) {
                motorIntensities[i] = 0;
              }
            }
            setIntensities(motorIntensities);
          }
        }
      }
      break;
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
  html += "      <p>WebSocket URL for real-time communication:</p>\n";
  html += "      <code>ws://" + localIP.toString() + ":81</code>\n";
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
      //Serial.println("Camera capture failed");
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
    delay(20); // ~50 fps
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

// not used future inprovement 
void sendFrame() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
      //Serial.println("Camera capture failed");
      return;
  }

  // Frame dimensions (original 320x240)
  int frame_width = fb->width;
  int frame_height = fb->height;

  // Crop parameters
  int crop_width = 240;
  int crop_height = 240;
  int x_offset = (frame_width - crop_width) / 2; // 40 pixels from left
  int y_offset = 0; // Start from top

  // Allocate memory for cropped frame
  size_t crop_size = crop_width * crop_height * (fb->len / (frame_width * frame_height));
  uint8_t *cropped_buffer = (uint8_t *)malloc(crop_size);

  if (!cropped_buffer) {
      //Serial.println("Memory allocation failed");
      esp_camera_fb_return(fb);
      return;
  }

  // Copy cropped portion
  for (int y = 0; y < crop_height; y++) {
      memcpy(
          cropped_buffer + y * crop_width,      // Destination
          fb->buf + ((y + y_offset) * frame_width) + x_offset, // Source
          crop_width                            // Number of bytes per row
      );
  }

  // Send cropped frame
  webSocket.broadcastBIN(cropped_buffer, crop_size);

  // Free memory
  free(cropped_buffer);
  esp_camera_fb_return(fb);
}

// Handle receiving data from Python (HTTP method before)
void handleSendData() {
  // Check if we have data
  if (server.hasArg("plain") == false) {
    server.send(400, "text/plain", "Missing data");
    return;
  }
  
  // Get the data from the request
  String json = server.arg("plain");
  
  // Parse the JSON data
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, json);
  
  if (error) {
    //Serial.print("JSON parsing failed: ");
    //Serial.println(error.c_str());
    server.send(400, "text/plain", "Invalid JSON data");
    return;
  }
  
  // Process and print the array data
  if (doc.containsKey("data") && doc["data"].is<JsonArray>()) {
    JsonArray array = doc["data"].as<JsonArray>();

    // map from 0-1 to 0-255 for intensities
    for (int i = 0; i < 7 && i < array.size(); i++) {
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

void simpleDemo() {
    for (int i = 0; i < 7; i++) {
      delay(200);
      ledcWrite(channels[i], 255); 
      delay(200);
      ledcWrite(channels[i], 0); 
    }
}

void fastDemo() {
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 7; j++) {
      ledcWrite(channels[j], 255); 
      delay(120);
      ledcWrite(channels[j], 0); 
    }
    for (int j = 6; j > -1; j--) {
      ledcWrite(channels[j], 255); 
      delay(120);
      ledcWrite(channels[j], 0); 
    }
  }
}

void setup() {
  //Serial.begin(115200);
  //Serial.println("ESP32 Camera Web Server - AP Mode");

  // Initialize motor PWM channels
  for (int pin = 0; pin < 7; pin++) {
    ledcSetup(channels[pin], freq, resolutionMotor);
    ledcAttachPin(motorPins[pin], channels[pin]);
  }

  // Configure camera with optimized settings
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
  
  // Initialize with optimized specs for faster transfer
  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;  // 352x288 - smaller for faster transfer
    config.jpeg_quality = 15;           // Lower quality for faster transfer (0-63, higher number = lower quality)
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA; // 320x240
    config.jpeg_quality = 20;           // Lower quality
    config.fb_count = 1;
  }

  // Camera initialization
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    //Serial.printf("Camera init failed with error 0x%x", err);
    setIntensities(motorsOn);
    delay(1000);
    setIntensities(motorsOff);
    ESP.restart();
  }

  // Set up Access Point
  WiFi.softAP(ap_ssid, ap_password);
  delay(100);
  
  // Print the IP address of the AP (usually 192.168.4.1)
  localIP = WiFi.softAPIP();
  //Serial.print("Camera Server Ready! AP IP address: ");
  //Serial.println(localIP);
  //Serial.print("SSID: ");
  //Serial.println(ap_ssid);
  //Serial.print("Password: ");
  //Serial.println(ap_password);
  
  // Brief indicator that we're connected
  for (int i = 0; i < 3; i++) {
    setIntensities(motorsOn);
    delay(50);
    setIntensities(motorsOff);
    delay(100);
  }

  delay(500);
  simpleDemo();
  fastDemo();

  // Initialize WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  //Serial.println("WebSocket server started on port 81");
  for (int i = 0; i < 3; i++) {
    setIntensities(motorsOn);
    delay(50);
    setIntensities(motorsOff);
    delay(100);
  }

  // Route for root / web page - serves a simple HTML interface
  server.on("/", HTTP_GET, handleRoot);
  
  // Route for streaming MJPEG video
  server.on("/stream", HTTP_GET, handleStream);
  
  // Route for getting a single image (for your torchcam application)
  server.on("/capture", HTTP_GET, handleCapture);
  
  // Legacy route for receiving data from Python via HTTP
  server.on("/send_data", HTTP_POST, handleSendData);
  
  // Start server
  server.begin();
}

void loop() {
  server.handleClient();
  webSocket.loop();  // Handle WebSocket events
}