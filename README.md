# 🛰️ Radar System – ESP32-S3 Application

A **smart radar system** built on the **ESP32-S3** microcontroller that detects nearby objects using a **Time-of-Flight(ToF)**distance sensor.  
The system combines a **servo motor**, **RGB LED**, **buzzer**, and **push button** — all running concurrently under **FreeRTOS**for real-time performance.

* * *

## ⚙️ Setup

**Board:** ESP32-S3 DevKitC-1  
**Power:** 5 V (shared with servo and board, common ground)

### Components

-   SG90/MG90S servo motor
    
-   VL53L1X ToF distance sensor
    
-   Built-in NeoPixel RGB LED (GPIO 38)
    
-   Active buzzer (GPIO 39)
    
-   Push button (GPIO 37, pull-up)
    

* * *

## 💡 Idea

The system mimics a mini-radar:

-   The **servo** sweeps the ToF sensor across 180°.
    
-   The **sensor** measures distance ahead.
    
-   When an object is closer than the threshold, the **buzzer** beeps and the **LED** flashes red.
    
-   Otherwise, the LED stays green.
    
-   The **button** toggles sweeping on or off.
    

* * *

## 🔄 Program Flow

1.  **Startup**
    
    -   LED flashes red/green while the servo performs one sweep and the buzzer beeps.
        
    -   Ends with LED solid green → system ready.
        
2.  **Operation**
    
    -   **Button press:** toggles sweeping.
        
    -   **When sweeping:**
        
        -   Servo rotates 0°↔180°.
            
        -   ToF sensor measures continuously.
            
        -   Object ≤ 10 cm → red LED + buzzer.
            
        -   Otherwise → green LED.
            
    -   **When stopped:** servo centers (90°) and LED stays green.
        
3.  **Parallel Execution**
    
    -   Each function (servo, sensor, LED, buzzer, button) runs in its own FreeRTOS task.
        
    -   Tasks delay themselves with `vTaskDelay()` to yield control and keep timing stable.
        

* * *

## 🧠 Concepts Applied

| Concept | Description |
| --- | --- |
| **Multitasking (FreeRTOS)** | Each component handled in a separate task. |
| **Semaphore** | Signals button press from ISR to Button Task. |
| **Mutex** | Protects shared distance data between tasks. |
| **PWM** | Drives servo and buzzer using hardware timers. |
| **I²C Communication** | Handles VL53L1X sensor readings via SDA 41 / SCL 42. |
| **Task Scheduling** | Ensures smooth, non-blocking operation. |

* * *

## 📡 Pin Assignments

| Component | Function | GPIO |
| --- | --- | --- |
| Servo Motor | Sweep control | 40 |
| ToF Sensor | SDA / SCL | 41 / 42 |
| Buzzer | Audio alert | 39 |
| RGB LED | Status indicator | 38 |
| Button | Sweep toggle | 37 |

* * *

## 🧩 Summary

This radar system demonstrates how the ESP32 can manage **multiple peripherals in real time** using FreeRTOS.
