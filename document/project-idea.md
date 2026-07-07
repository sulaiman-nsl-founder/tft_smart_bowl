# SMART-BOWL 🥣
## Project Idea

**Version:** 1.0  
**Project Type:** Embedded IoT System  
**Status:** Active Development

---

# Overview

SMART-BOWL is an IoT-enabled smart pet food bowl designed to accurately monitor pet food consumption while providing real-time cloud synchronization and a modern embedded user interface.

The project focuses on creating a reliable, production-ready embedded firmware capable of operating both online and offline. The firmware is designed using a modular architecture so that additional features can be integrated without major redesign.

The system measures food weight using a high-precision load cell, displays information on a TFT display, synchronizes data with cloud services using MQTT, stores data locally when offline, captures images using an ESP32-S3 camera, and supports secure OTA firmware updates.

The goal is to create a commercial-grade embedded platform rather than a prototype.

---

# Objectives

- Accurately measure pet food weight.
- Track food consumption over time.
- Synchronize feeding data with cloud services.
- Continue operating when internet connectivity is unavailable.
- Provide an intuitive embedded user interface.
- Allow remote monitoring through a mobile application.
- Build firmware that is modular, scalable, and maintainable.

---

# Current Features

## Food Weight Measurement

- High precision load cell measurement
- Gram-level resolution
- Real-time weight updates
- Stable filtered readings

---

## Load Cell Calibration

Supports:

- Zero calibration (Tare)
- Span calibration
- Calibration factor storage
- Persistent calibration values

---

## Food Menu Management

Food information is **not stored locally**.

The firmware downloads available food options from the cloud through MQTT.

Each food profile contains information such as:

- Food ID
- Food Name
- Food Type
- Recommended Quantity

The downloaded menu is displayed on the TFT, allowing the user to select the correct food before placing it in the bowl.

---

## Food Consumption Tracking

After food selection, the firmware continuously monitors bowl weight to determine:

- Initial weight
- Current weight
- Remaining food
- Consumed quantity
- Feeding duration

Consumption records are uploaded to the cloud.

---

## TFT User Interface

Interactive graphical interface including:

- Boot Screen
- Dashboard
- Food Selection
- Weight Screen
- Battery Status
- Wi-Fi Status
- Cloud Status
- Settings
- Diagnostics
- OTA Progress

Navigation is performed using physical buttons.

---

## Wi-Fi Connectivity

Features include:

- Auto Connect
- Auto Reconnect
- Connection Status
- Signal Strength Monitoring

---

## BLE Provisioning

BLE is used only during device setup.

It allows:

- Wi-Fi configuration
- Device registration
- Initial configuration

BLE is disabled after provisioning to reduce power consumption.

---

## Cloud Synchronization

Synchronizes:

- Food consumption
- Weight
- Device status
- Battery level
- System health

---

## MQTT Communication

The SMART-BOWL communicates with the cloud using MQTT over TLS (MQTTS) to ensure secure and encrypted communication.

Authentication is certificate-based, providing a secure connection between the device and the MQTT broker.

### MQTT Features

- Secure MQTT over TLS
- Certificate-based authentication
- Automatic reconnect
- Publish/Subscribe architecture
- Offline message queue
- QoS support where required
- Keep Alive monitoring
- Connection status monitoring

### Subscribe Topics

The firmware subscribes to topics for:

- Food Menu Updates
- Device Configuration
- OTA Update Notifications
- Remote Commands
- Device Settings

### Publish Topics

The firmware publishes:

- Current Food Weight
- Food Consumption Records
- Battery Status
- Device Health
- Device Online Status
- Error Logs
- Diagnostics Information

### Security

- TLS encrypted communication
- Root CA certificate verification
- Server certificate validation
- Secure MQTT client authentication
- Automatic certificate loading during boot
- Connection retry with exponential backoff

### Subscribe Topics

- Food Menu
- Device Configuration
- OTA Notification
- Remote Commands

### Publish Topics

- Food Weight
- Food Consumption
- Battery Status
- Device Health
- Event Logs

---



# Cloud Communication

SMART-BOWL uses different communication protocols depending on the type of data being transferred.

| Data Type | Protocol |
|-----------|----------|
| Telemetry | MQTT over TLS |
| Configuration | MQTT over TLS |
| Food Menu | MQTT over TLS |
| OTA Notification | MQTT over TLS |
| Device Commands | MQTT over TLS |
| Image Upload | HTTPS |
| Firmware Download | HTTPS |

MQTT is used for lightweight real-time communication, while HTTPS is used for transferring large binary data such as camera images and firmware packages.

---

## Camera Support

The ESP32-S3 camera captures images of the bowl.

Images are uploaded using HTTPS instead of MQTT because image files are significantly larger than MQTT payloads.

Supported features:

- Manual Capture
- Remote Capture
- HTTPS Upload
- Upload Status Monitoring

The cloud stores the image URL rather than the image itself.

---

## Offline Storage

If Wi-Fi is unavailable:

- Sensor data is stored on the SD card.
- Records are queued locally.
- Data is automatically synchronized once the connection is restored.

---

## Battery Monitoring

Supports:

- Battery Voltage
- Battery Percentage
- Low Battery Warning
- Charging Detection

---

## OTA Firmware Update

Supports secure firmware updates including:

- Version Check
- Firmware Download
- Integrity Verification
- Installation
- Automatic Restart

---

## Device Configuration

Configuration stored in flash memory:

- Wi-Fi Credentials
- MQTT Configuration
- Calibration Values
- Device Name
- Display Settings

---

## Diagnostics

System diagnostics include:

- Wi-Fi
- MQTT
- Camera
- SD Card
- Battery
- Sensor Status
- Memory Usage
- Firmware Version

---

## User Input

Physical buttons allow:

- Menu Navigation
- Food Selection
- Calibration
- Settings
- Diagnostics

---

## AI Assistant Notes

When contributing to this project:

- Use MQTT over TLS for all messaging.
- Never use unsecured MQTT connections.
- Image uploads must always use HTTPS.
- Do not store food menus locally unless explicitly required.
- Configuration should persist in NVS.
- Offline data should be stored on the SD card and synchronized automatically once connectivity is restored.
- Follow a modular architecture with clear separation between hardware drivers, services, business logic, and UI.
- Prefer FreeRTOS tasks, queues, timers, and event groups over blocking loops.
- Keep all code production-ready and scalable.

---

# System Architecture

```
                  Mobile Application
                           │
                  BLE Provisioning
                           │
                           ▼
                    ESP32-S3 Firmware
      ┌────────────────────────────────────┐
      │                                    │
      │        Application Layer           │
      └────────────────────────────────────┘
                           │
     ┌───────────────┬───────────────┬───────────────┐
     ▼               ▼               ▼
 Sensors         Display          Storage
     │               │               │
 Load Cell       ST7735 TFT       SD Card
 Battery ADC
 Camera
                           │
                           ▼
                Communication Layer
               Wi-Fi • MQTT • HTTPS
                           │
                           ▼
                    Cloud Platform
```

---

# Design Philosophy

The firmware is designed using independent software modules.

Each module has a single responsibility and communicates with other modules using defined interfaces.

Major modules include:

- Boot Manager
- Sensor Manager
- Display Manager
- Camera Manager
- Storage Manager
- MQTT Manager
- Wi-Fi Manager
- OTA Manager
- Configuration Manager
- Diagnostics Manager
- Power Manager

This modular architecture allows future features to be added with minimal impact on existing code.

---

# Development Principles

- Modular architecture
- Hardware abstraction
- Event-driven design
- Non-blocking code
- Offline-first operation
- Production-ready firmware
- Easy maintenance
- Extensible architecture

---

# Current Hardware

- ESP32-S3
- ST7735 TFT Display
- NAU7802 Load Cell ADC
- 50 kg Half-Bridge Load Cell
- MCP23017 IO Expander
- ESP32-S3 Camera
- MicroSD Card
- Li-ion Battery
- Battery Charging Circuit
- Battery Protection Circuit

---

# Future Features

Future firmware versions may include:

- Water monitoring
- Automatic feeder motor
- RFID pet identification
- AI feeding analytics
- Multi-pet support
- Mobile notifications
- Edge AI
- Voice assistant integration

---

# AI Assistant Notes

When generating code or documentation for this project:

- Prioritize modular, maintainable firmware.
- Avoid blocking delays.
- Use FreeRTOS tasks where appropriate.
- Keep hardware abstraction separate from business logic.
- Prefer event-driven communication between modules.
- Optimize for low power consumption.
- Maintain backward compatibility with existing configuration storage.
- Follow production-quality coding practices rather than prototype code.
- All new features should integrate cleanly into the existing architecture without tightly coupling modules.

The firmware should always be written with scalability, reliability, and maintainability as primary goals.