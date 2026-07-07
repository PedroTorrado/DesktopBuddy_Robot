# Wiring Guide

This document outlines the hardware connections required to build DesktopBuddy.

> **IMPORTANT: Common Ground**
> All components (Nucleo, ESP32, Sensors, and Motor Driver) MUST share a common Ground (GND) connection. Failure to connect grounds will result in signals not being recognized.

## 1. Microcontroller Communication (ESP32-CAM to Nucleo)
The ESP32-CAM acts as the vision coprocessor and communicates with the Nucleo master via UART.

| ESP32-CAM Pin | Nucleo F411RE Pin | Function |
| :--- | :--- | :--- |
| U0T (TX) | **PA10** (UART RX) | Serial Data to Nucleo (Arduino D2) |
| U0R (RX) | **PB6** (UART TX) | Serial Data to ESP32 (Arduino D10) |
| Pin 14 | **PC8** | Wake-up / Interrupt Signal |
| GND | GND | Common Ground |

## 2. L298N Motor Driver
We use TIM3 for hardware PWM (speed control) and GPIOB for directional control. Because of the number of peripherals, some direction pins are located on the ST Morpho headers.

**Speed Control (Arduino Headers):**
| L298N Pin | Nucleo Pin | Location | Function |
| :--- | :--- | :--- | :--- |
| ENA | **PA6** | Arduino D12 | Left Motor Speed |
| ENB | **PA7** | Arduino D11 | Right Motor Speed |

**Direction Control (Morpho Headers):**
| L298N Pin | Nucleo Pin | Location | Function |
| :--- | :--- | :--- | :--- |
| IN1 | **PB13** | Morpho CN10, Pin 31 | Left Motor Fwd |
| IN2 | **PB14** | Morpho CN10, Pin 32 | Left Motor Rev |
| IN3 | **PB15** | Morpho CN10, Pin 33 | Right Motor Fwd |
| IN4 | **PB1**  | Morpho CN7, Pin 27 | Right Motor Rev |

*Note: You must remove the ENA/ENB plastic jumpers on the L298N to use PWM speed control.*

## 3. I2C Sensor Suite
The ADXL345 (Accelerometer), SGP30 (Air Quality), and the OLED Display all share the same I2C bus. Connect them in parallel.

| Sensor Pin | Nucleo F411RE Pin |
| :--- | :--- |
| SDA | **PB9** |
| SCL | **PB8** |
| VCC | **3.3V** *(Do not use 5V!)* |
| GND | GND |

## 4. HC-SR04 Ultrasonic Distance Sensor
Provides collision avoidance for the front bumper. The Nucleo F411RE pins used are 5V-tolerant (FT).

| HC-SR04 Pin | Nucleo F411RE Pin |
| :--- | :--- |
| TRIG | **PA8** (Arduino D7) |
| ECHO | **PC7** (Arduino D9) |
| VCC | **5V** |
| GND | GND |

## Power Considerations
*   **Motors:** Use a dedicated power supply (e.g., 5V Power Bank or battery pack) connected to the 12V and GND terminals on the L298N. Do not power the motors from the Nucleo.
*   **ESP32:** Requires up to 400mA during WiFi transmission. Power it from a stable 5V source.
