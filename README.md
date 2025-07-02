# Avery 1.0 — Haptic Feedback Band for Spatial Awareness

Avery is a wearable haptic feedback device developed to enhance spatial awareness through real-time environmental sensing. Designed primarily for sensory augmentation, this device provides vibrational feedback based on obstacle proximity detected via a camera, aiding users (especially the visually impaired) in perceiving their surroundings.

## How It Works

The system is composed of two primary components:

### 1. **ESP32-CAM Unit**
- Mounted on the side of the head capturing the front view of the user.
- Continuously captures images of the environment.
- Sends visual data wirelessly to a server for processing.

### 2. **Server (Python-based)**
- Receives images from the ESP32-CAM.
- Uses a depth estimation model to process the input and determine obstacle distance.
- Outputs a vector of motor intensities, which is sent back to the microcontroller to drive vibration motors accordingly.

## Key Features
- **Real-Time Depth Estimation**
- **Wireless Communication (ESP32 ↔ Server) using the esp-32 as an access point**
- **Multi-Motor Haptic Feedback Mapping**
- **Compact and Wearable Design**

## Product Images



<table align="center">
  <tr>
    <th>Real-world Assembly</th>
    <th>Server-Side Interface</th>
    <th>device 3D model</th>
  </tr>
  <tr>
    <td><img src="Product/AveryReal-1.png" width="100%" alt="Real-world Assembly"></td>
    <td><img src="Product/Server.png" width="100%" alt="Server-Side Interface"></td>
    <td> 
      <p align="center">
        <img src="Product/Avery3D-1.png" width="30%" />
        <img src="Product/Avery3D-2.png" width="30%" />
        <img src="Product/Avery3D-3.png" width="30%" />
      </p>
    </td>
  </tr>
</table>

## 🖥️ Code Overview

### `ESP/main.cpp`
Handles:
- Camera initialization (ESP32-CAM)
- WiFi setup
- Sending captured image frames to the server
- Receiving processed motor feedback values
- Driving vibration motors using PWM based on server input

### `Server/server.py`
Performs:
- receives images
- Calls a depth estimation model
- Maps depth data onto vibration levels
- Sends motor commands back to ESP32

---

## 🧠 Future Work

- Integrate smaller depth estimation models (e.g., FastDepth) for faster processing.
- Optimize latency and energy consumption.
- Add support for edge-case detection (stairs, drop-offs).
- Improve haptic feedback resolution and user configurability.

---

## 🙏 Credits

Developed by Aleksandre Meskhi
GitHub - [Meskh](https://github.com/meskh)   
Linkedin - [Aleksandre](https://www.linkedin.com/in/aleksandre-meskhi/)
Part of a personal sensory augmentation initiative.

---
