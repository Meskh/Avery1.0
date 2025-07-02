## 💻 Server Operation Overview (server.py)

This Python server acts as the processing and control center for the wearable haptic device.

### 🖥️ Setup

- The server runs on a **local machine** (tested on macOS, but can work on other systems).
- The machine connects to the **Wi-Fi access point created by the ESP32-CAM**, establishing a direct link.

---

### 🔄 Workflow

1. **Image Reception**
   - The ESP32-CAM captured image is sent via an HTTP POST request to the server.

2. **Depth Map Generation**
   - Upon receiving the image, the server processes it to generate a **depth map**, estimating distances to obstacles in the scene.
   - The depth map, alongside the mapped motor intensities, is also **visualized locally** on the machine to allow for debugging and monitoring.
   - Note: Visualization introduces a major delay for the entire operation of the device, its removal decreases latency significantly
     
3. **Motor Intensity Mapping**
   - The depth data is analyzed and converted into an array of motor intensity values. 
   - Each value corresponds to a specific vibration motor on the wearable device, representing nearby obstacles through varying vibration strengths.

4. **Feedback Transmission**
   - The server sends the array of motor intensities back to the ESP32.
   - The ESP32 then uses these intensities to drive the motors in real-time.

---

### 🛠️ Key Features

- Simple HTTP-based communication protocol.
- Real-time depth visualization.
- Flexible design — can integrate different depth estimation models as needed.
- Easy to run and test locally on a MacBook or other machine.
