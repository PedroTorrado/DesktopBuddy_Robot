# DesktopBuddy Robot

An autonomous, reactive, and expressive robotic companion built with an ESP32-CAM and STM32 Nucleo.

## Architecture
*   **ESP32-CAM**: Handles WiFi, AWS IoT communication, and runs a neural network face-detection algorithm.
*   **STM32 Nucleo-F411RE**: The master controller running the Emotion Engine, Motor PID loop, and I2C sensors.

## Repository Structure
*   `/ESP32_CAM`: Arduino IDE project for the ESP32-CAM.
*   `/Nucleo_F411RE`: STM32CubeIDE project for the Nucleo.
*   `/docs`: Wiring guides, AWS setup instructions, and architecture diagrams.

## Setup Instructions
1. Copy `.env.example` to `.env` and fill in your WiFi and AWS credentials.
2. Copy `ESP32_CAM/secrets.h.example` to `ESP32_CAM/secrets.h` and paste your certificates.
3. Flash the ESP32 using the Arduino IDE.
4. Build and flash the Nucleo using STM32CubeIDE.
5. Refer to `docs/wiring_guide.md` for physical hardware connections.

Enjoy your new desktop companion!
