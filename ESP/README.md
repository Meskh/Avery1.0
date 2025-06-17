## 📟 ESP32 Operation Overview (main.cpp)

The ESP32-CAM acts as the client-side controller for capturing environmental data and driving the haptic motors based on feedback received from the server.

### 🔄 Workflow

1. **Access Point Setup**  
   The ESP32 sets up its own Wi-Fi access point. The processing server connects to this network to allow for direct communication.

2. **Camera Input**  
   It continuously captures frames using the onboard camera (ESP32-CAM).

3. **Data Transmission**  
   Captured images are sent to the server running locally on a laptop or other machine.

4. **Motor Feedback Reception**  
   The server processes the images and responds with motor intensity values mapped to represent obstacle distances.

5. **Vibration Mapping**  
   The ESP32 receives the motor data and maps each value to a corresponding PWM signal, which drives the vibration motors accordingly.

## 🔌 Hardware Overview

- Each vibration motor is powered by an **external power source** embedded within the wearable device (e.g., a Li-ion battery).
- **BJTs (Bipolar Junction Transistors)** are used to regulate the amount of power delivered to each motor, controlled via the ESP32’s PWM output.
- To ensure safe operation and prevent damage:
  - A **diode** is placed in parallel with each motor to protect against back EMF (electromotive force).
  - A **capacitor** is also included in parallel to smooth voltage fluctuations and absorb transients.
- This setup ensures reliable and safe haptic feedback without compromising the stability of the power system or the microcontroller.

This combination of hardware control and software logic enables efficient, responsive, and safe operation of the haptic system.
