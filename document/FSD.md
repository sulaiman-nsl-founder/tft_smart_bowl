# SMART-BOWL Firmware Functional Specification

**Document ID:** SB-FSD-001  
**Version:** 0.2-draft  
**Date:** 2026-07-06  
**Product code:** `iotbowl`  
**Model code:** `ibw001`  
**Target:** ESP32-S3-WROOM-1U-N16R8  
**Classification:** Engineering controlled document

> **Baseline notice:** This document specifies the intended production firmware. The repository currently contains only a minimal Arduino/PlatformIO starter and does not implement the requirements below. “Shall” denotes a requirement; values marked **TBD** require approval before release.

## 1. Document Control

Owner: Firmware Engineering. Reviewers: Hardware, Backend, Mobile, Cloud, QA, Manufacturing, Security, and Product. Changes require a revision entry and review of affected requirement and test IDs.

### 1.1 Revision History

| Revision | Date | Author | Change |
|---|---|---|---|
| 0.1 | 2026-07-06 | Engineering draft | Initial evidence-based specification |
| 0.2 | 2026-07-06 | Engineering update | Confirmed direct ST7735 wiring and revised MCP23017 button/LED mapping |

### 1.2 Source Hierarchy

Conflicts are resolved in this order:

1. Approved schematic and PCB release package.
2. Approved pin-map workbook.
3. This FSD.
4. `project-idea.md`.
5. Legacy SmartBowl workflow document.
6. Current firmware behavior.

The legacy workflow references HX711, SSD1306, and SD_MMC. Current product intent references NAU7802, ST7735, and microSD. These are not considered interchangeable without an approved change. The direct ST7735 and MCP23017 mappings in revision 0.2 supersede conflicting assignments in the pin-map workbook.

## 2. Scope

This FSD covers device firmware, hardware interfaces, provisioning, weight measurement, user interaction, feeding-session logic, local persistence, MQTT/HTTPS communication, camera operation, OTA, diagnostics, security, manufacturing, service, and verification.

Backend internals, mobile UI implementation, enclosure design, charger design, analytics models, and regulatory certification are out of scope except where they define a device interface.

## 3. Product Overview

SMART-BOWL is a connected pet-food bowl that measures food mass, records feeding sessions, shows device state locally, operates through network outages, captures bowl images, and synchronizes trusted records to cloud services.

Primary users are pet owners, installers, factory operators, service technicians, backend systems, and QA personnel.

## 4. System Overview

```text
Mobile app --BLE setup--> ESP32-S3 <--buttons/display--> User
                              |
              +---------------+---------------+
              |               |               |
         Weight ADC       Camera/SD      Battery/I/O
              |               |               |
              +--------- event bus ------------+
                              |
                    Wi-Fi / TLS / HTTPS
                              |
                    MQTT + REST + Blob store
```

The device shall remain useful without cloud connectivity. Network availability shall affect synchronization, not measurement, local UI, calibration, or queued event integrity.

## 5. Requirement Conventions

IDs use `FR-<domain>-NNN`, `NFR-<domain>-NNN`, and `TC-<domain>-NNN`. Priority is Must, Should, or May. Each Must requirement requires at least one test before production release.

Implementation status:

- **Implemented:** verified in current source.
- **Planned:** required but absent.
- **TBD:** decision or value not yet approved.

At this revision, all product requirements are Planned unless explicitly stated otherwise.

## 6. Functional Requirements

| ID | Requirement | Priority | Verification |
|---|---|---:|---|
| FR-BOOT-001 | Firmware shall perform deterministic staged initialization and expose failures. | Must | TC-BOOT-001 |
| FR-WGT-001 | Firmware shall sample, calibrate, filter, and report bowl weight in grams. | Must | TC-WGT-001 |
| FR-WGT-002 | Firmware shall distinguish unstable and stable readings. | Must | TC-WGT-002 |
| FR-CAL-001 | Firmware shall support tare and span calibration with persistent CRC-protected data. | Must | TC-CAL-001 |
| FR-UI-001 | Firmware shall provide boot, dashboard, food, weight, settings, diagnostics, and OTA views. | Must | TC-UI-001 |
| FR-MENU-001 | Firmware shall download and cache a versioned food menu. | Must | TC-MENU-001 |
| FR-FEED-001 | Firmware shall record initial, current, leftover, consumed mass, and session duration. | Must | TC-FEED-001 |
| FR-WIFI-001 | Firmware shall connect, report RSSI, and reconnect with bounded backoff. | Must | TC-WIFI-001 |
| FR-BLE-001 | Firmware shall support authenticated setup and disable BLE after provisioning. | Must | TC-BLE-001 |
| FR-MQTT-001 | Firmware shall exchange versioned JSON messages over authenticated TLS. | Must | TC-MQTT-001 |
| FR-CAM-001 | Firmware shall capture JPEG images and persist them before upload. | Must | TC-CAM-001 |
| FR-OFF-001 | Firmware shall queue records offline and replay them without duplication. | Must | TC-OFF-001 |
| FR-PWR-001 | Firmware shall report battery voltage, percentage, and charging/low state. | Must | TC-PWR-001 |
| FR-OTA-001 | Firmware shall verify and install signed firmware with rollback. | Must | TC-OTA-001 |
| FR-DIAG-001 | Firmware shall expose subsystem health and a sanitized diagnostic export. | Must | TC-DIAG-001 |
| FR-RESET-001 | Firmware shall require deliberate confirmation for factory reset. | Must | TC-RESET-001 |

## 7. Non-Functional Requirements

| ID | Requirement |
|---|---|
| NFR-REL-001 | A power loss shall not corrupt the last committed configuration or queue record. |
| NFR-REL-002 | The watchdog shall recover a stalled task and record the reset reason. |
| NFR-PERF-001 | UI input shall receive visible acknowledgement within 150 ms under normal load. |
| NFR-PERF-002 | No application task shall busy-wait or use an unbounded blocking operation. |
| NFR-SEC-001 | Production communication shall reject invalid, expired, or untrusted certificates. |
| NFR-SEC-002 | Secrets shall never appear in logs, MQTT payloads, diagnostics, or UI screens. |
| NFR-MAINT-001 | Hardware drivers shall be isolated from application state and cloud schemas. |
| NFR-MAINT-002 | Every persistent or network schema shall carry an explicit version. |
| NFR-OBS-001 | Logs shall include timestamp, severity, module, event code, and safe context. |
| NFR-ACC-001 | Critical states shall not be communicated by color alone. |

Quantitative accuracy, battery runtime, boot time, queue capacity, environmental range, and service lifetime are TBD and must be established by product validation.

## 8. Hardware Specification

| Component | Approved/target role | Status |
|---|---|---|
| ESP32-S3-WROOM-1U-N16R8 | MCU, Wi-Fi, BLE, camera interface | Pin-map baseline |
| ST7735 TFT | Local UI using direct ESP32-S3 control and SPI pins | Pin wiring confirmed; resolution/rotation TBD |
| MCP23017 | Three buttons and three indicator LEDs | Application port mapping confirmed |
| NAU7802 | Load-cell ADC over I2C | Target; schematic/pins unresolved |
| 50 kg half-bridge load cell | Mass sensing | Target; bridge topology TBD |
| Camera, likely OV2640 | JPEG capture | Sensor and pin timing validation required |
| microSD | Queue and image storage | SPI versus SD_MMC decision required |
| Li-ion subsystem | Portable power | Divider and charger signals TBD |

All inputs shall remain inside device voltage limits. Firmware shall initialize outputs to a safe state before enabling loads.

## 9. Complete Hardware Connection and GPIO Mapping

The table combines the pin-map workbook with the confirmed revision 0.2 overrides. Revision 0.2 assignments take precedence where they conflict with the workbook. Schematic review remains mandatory.

| Signal | MCU GPIO | Owner/direction | Notes |
|---|---:|---|---|
| RESET_PB | 4 | Boot/input | Pull and debounce TBD |
| CAMERA_SIOD/SIOC | 5/6 | Camera, bidirectional/output | SCCB |
| CAMERA_VSYNC/HREF/XCLK | 7/15/16 | Camera | XCLK output |
| CAMERA_D7/D6/D5/D2 | 17/18/8/9 | Camera input | Remaining data below |
| CAMERA_D1/D3/D0/D4 | 10/11/12/13 | Camera input | |
| CAMERA_PCLK | TBD | Camera input | Workbook GPIO14 assignment conflicts with confirmed TFT_DC |
| USB D-/D+ | 19/20 | USB | Reserve when USB used |
| MAX17055_ALERT | 3 | Input/optional | Strapping pin; validate |
| SHUTDN_CTRL | 46 | Output | Boot strapping constraints |
| Legacy HX711_SCK/DOUT | Not assigned | Legacy weight ADC | Workbook GPIO21/47 assignments are superseded by TFT_RST/TFT_CS |
| TFT_CS | 47 | SPI output | Confirmed direct ESP32-S3 connection |
| TFT_DC | 14 | GPIO output | Confirmed direct ESP32-S3 connection |
| TFT_RST | 21 | GPIO output | Confirmed direct ESP32-S3 connection |
| TFT_MOSI | 38 | SPI output | Confirmed direct ESP32-S3 connection |
| TFT_SCLK | 39 | SPI output | Confirmed direct ESP32-S3 connection |
| SDCARD_CS | 45 | SPI output | Strapping constraints |
| MCP23017_INTA | 0 | Input | Boot strapping pin; risk review |
| SPI MISO | 40 | SPI input | Not used by write-only TFT; SD use remains TBD |
| I2C SCL/SDA | 41/42 | Shared I2C | NAU7802 + MCP23017 target |
| UART0 RX/TX | 44/43 | Service UART | |
| LDR | 2 | ADC input | Optional |
| Battery voltage | 1 | ADC input | Divider ratio TBD |

GPIO35–37 are unavailable on N16R8 because of octal PSRAM. GPIO0, 3, 45, and 46 require strapping analysis. Firmware shall not repurpose reserved pins.

### 9.1 MCP23017 Mapping

| Port | Function | Direction/default |
|---|---|---|
| GPA0/GPA1/GPA2 | BUTTON_1/BUTTON_2/BUTTON_3 | Input with validated bias; library pin numbers 0/1/2 |
| GPA3 | PIR sensor | Input |
| GPA4 | Power button | Input |
| GPA5/GPA6/GPA7 | Reserved | Input-safe until assigned |
| GPB0/GPB1/GPB2 | LED_1/LED_2/LED_3 | Output/off; library pin numbers 8/9/10 |
| GPB3/GPB4 | IR-cut controls 1/2 | Output/inactive |
| GPB5/GPB6 | Camera reset/power | Output/reset and off |
| GPB7 | Buzzer | Output/off |

MCP23017 address straps, reset wiring, interrupt polarity, and supply voltage are TBD.

## 10. Bus Architecture

### 10.1 SPI

The ST7735 shall use MOSI GPIO38, SCLK GPIO39, and CS GPIO47. DC GPIO14 and reset GPIO21 are direct GPIO controls. The write-only TFT interface does not require MISO. If microSD shares MOSI/SCLK, it shall use a separate chip select and SPI access shall be serialized. Per-device mode and clock shall be restored for every transaction. SD writes shall not starve display refresh or weight processing.

### 10.2 I2C

MCP23017 and target NAU7802 shall share GPIO41/42 only after address and voltage compatibility are confirmed. The driver shall use timeouts, recover a stuck bus, and mark the affected device degraded after bounded retries.

### 10.3 Camera

The camera driver shall control power/reset, validate sensor identity, allocate buffers preferentially in PSRAM, and bound capture time. Camera failure shall not disable weighing.

### 10.4 Power

High-current peripherals shall start sequentially. Brownout, reset reason, and battery state shall be recorded. Low-battery policy and shutdown voltage are TBD and shall align with cell and charger specifications.

## 11. Firmware Architecture

```text
Application: Boot | UI | Feeding | Provisioning | Diagnostics
Services:    Events | Config | Storage | Sync | OTA | Security
Protocols:   MQTT | HTTPS | BLE | JSON
Drivers:     Weight | TFT | MCP23017 | Camera | SD | Battery
Platform:    Arduino-ESP32 / FreeRTOS / ESP-IDF services
```

Modules shall own their state. Cross-module calls shall use typed commands/events rather than shared mutable globals. Drivers shall return structured errors.

Recommended repository structure:

```text
src/app  src/services  src/protocols  src/drivers  src/platform
include/ tests/unit  tests/integration  partitions/
```

## 12. Boot Sequence

1. ROM/bootloader verifies application and selects valid partition.
2. Firmware captures reset reason and starts watchdog.
3. Initialize logging, NVS/config, GPIO safe states, I2C, MCP23017, display.
4. Mount SD and recover incomplete queue transactions.
5. Initialize weight ADC and load calibration.
6. Initialize camera without blocking core bowl functions.
7. Start application tasks and render dashboard/degraded state.
8. Connect Wi-Fi; synchronize time before TLS.
9. Connect MQTT, publish online status, fetch configuration/menu.
10. Start normal measurement and synchronization.

A failed optional subsystem shall enter degraded mode. A failed configuration store or weight path shall enter safe/service mode.

## 13. Provisioning and Device Registration

An unprovisioned device shall advertise a non-secret setup identifier over BLE. The mobile application shall establish an authenticated session, transmit Wi-Fi credentials, and request backend registration. Firmware shall validate field sizes, save credentials atomically, disconnect BLE, join Wi-Fi, synchronize time, register, obtain device identity/certificates through an approved secure mechanism, and acknowledge completion.

Provisioning retries shall not erase existing valid credentials. A physical confirmation is required to replace ownership or clear credentials.

## 14. Certificate Lifecycle and Security

Each production device shall have a unique identity. Shared private keys are prohibited. Private keys shall be generated or injected using the approved factory process and stored using ESP32 secure storage capabilities. Production shall enable secure boot and flash encryption after recovery implications are approved.

Certificate states are Absent, Valid, RenewalDue, Expired, Revoked, and Invalid. Renewal shall begin before expiry and use the still-valid credential. Clock uncertainty shall block certificate validity decisions and authenticated cloud operation, while offline measurement continues.

Remote commands shall be authenticated, authorized, schema-validated, replay-resistant, bounded, and audited. Factory reset shall erase customer credentials, tokens, cached personal data, and queued uploads.

## 15. MQTT over TLS

Base topic:

```text
petwatch/{product_code}/{model_code}/{serial_number}
```

All segments shall be lowercase and validated. Proposed topics:

| Direction | Suffix | QoS | Purpose |
|---|---|---:|---|
| Publish | `/status` | 1 | retained online/offline state |
| Publish | `/telemetry` | 0 | periodic live values |
| Publish | `/event` | 1 | feeding and device events |
| Publish | `/blob` | 1 | image-upload result |
| Publish | `/calibration` | 1 | calibration audit |
| Publish | `/ota/status` | 1 | OTA progress/result |
| Subscribe | `/ack` | 1 | server acknowledgements |
| Subscribe | `/cmd/tare` | 1 | authorized tare |
| Subscribe | `/cmd/config` | 1 | versioned configuration |
| Subscribe | `/cmd/menu` | 1 | food menu |
| Subscribe | `/cmd/capture` | 1 | image capture |
| Subscribe | `/cmd/ota` | 1 | OTA manifest |

Every message shall include `schema_version`, `message_id`, device identity, timestamp, sequence number, firmware version, and payload. Duplicate command IDs shall return the cached result without repeating side effects. Payload size limits, broker URI, keepalive, session policy, and CA chain are deployment configuration.

## 16. HTTPS Communication

HTTPS shall use certificate validation, bounded connect/read timeouts, response-size limits, and idempotency keys. Images shall upload through an authorized short-lived URL or backend-mediated endpoint. Firmware shall never log query tokens. Success requires the documented HTTP status and, where applicable, metadata acknowledgement.

## 17. Configuration Storage

Persistent configuration shall be namespaced and versioned:

- identity and manufacturing data;
- Wi-Fi and cloud endpoints;
- certificates and keys;
- calibration and filter parameters;
- UI preferences;
- last accepted menu/config versions;
- queue sequence and OTA state.

Writes shall use copy-validate-commit semantics with CRC/integrity metadata. Migration shall be forward-controlled. Unknown future schemas shall not be silently overwritten.

## 18. Load Cell and Calibration

### FR-WGT-001 — Weight Measurement

**Purpose:** produce calibrated, filtered bowl mass.  
**Input:** NAU7802 target ADC samples.  
**Output:** raw count, gross grams, filtered grams, stability, error state.  
**Normal flow:** validate readiness, read sample, reject electrical/outlier faults, apply zero and span, filter, evaluate stability, publish snapshot.  
**Errors:** bounded retry; retain last stable value marked stale; raise fault.  
**Timing:** sample/filter rates TBD through characterization.  
**Acceptance:** accuracy and repeatability limits TBD; no false stable result during active disturbance.

Calibration shall require an unloaded stable bowl for tare and a traceable reference mass for span. The record shall include timestamp, operator/source, raw zero, raw span, reference mass, factor, hardware identity, and CRC. Invalid calibration shall block consumption calculations and display a service prompt.

## 19. Food Menu Workflow

The cloud is authoritative, but the last validated menu shall be cached for offline use. A menu contains version, expiry policy, food IDs, display names, type, recommended quantity, units, and optional constraints. Firmware shall reject malformed, oversized, duplicate-ID, or unsupported-schema menus and retain the prior valid menu.

## 20. Bowl and Feeding State Machine

```text
UNAVAILABLE -> EMPTY -> FOOD_PRESENT -> CONSUMING -> LEFTOVER
                    \-> SESSION_ABORTED
```

Transitions shall use configurable thresholds, hysteresis, stability windows, and minimum durations.

- EMPTY: stable mass at or below empty threshold.
- FOOD_PRESENT: positive stable increase consistent with food placement.
- CONSUMING: sustained net negative trend excluding noise and bowl removal.
- LEFTOVER: post-consumption stable positive mass.
- UNAVAILABLE: sensor/calibration invalid.

Consumed mass is `max(0, initial_food_mass - leftover_mass)`. Every session shall have a UUID and event sequence. Reboots shall recover or explicitly close an interrupted session.

## 21. Camera and Image Upload

Capture triggers may include eating start, user confirmation, feeding completion, and authorized remote request. The camera manager shall enforce privacy configuration, storage reserve, cooldown, and maximum burst size.

Workflow: capture JPEG → validate size/format → write temporary file → sync and atomic rename → enqueue metadata → HTTPS upload → backend metadata acknowledgement → MQTT blob event → retention cleanup.

File names shall use device-safe identifiers and session/event IDs, not customer names. Failed uploads remain queued according to retention policy.

## 22. UI, Buttons, and Display

Screens: Boot, Provisioning, Dashboard, Food Selection, Weight, Feeding Session, Battery/Connectivity, Settings, Calibration, Diagnostics, Error, and OTA.

Up/Down move focus or values; OK selects; long-press behavior shall be screen-specific and visible. Inputs require debounce and press/release/long-press events. Destructive operations require confirmation and shall not share a single accidental gesture.

The display manager shall own ST7735 access, use bounded redraw regions, and show stale/invalid measurements distinctly. TFT resolution, color order, rotation, font set, brightness curve, and backlight policy are TBD.

## 23. FreeRTOS Tasks and Inter-task Communication

Proposed tasks:

| Task | Responsibility | Rule |
|---|---|---|
| Sensor | ADC sampling/filtering | Highest application timing priority |
| App | state machines/session logic | Single owner of bowl state |
| UI | input and rendering | No network/storage blocking |
| Storage | queue/files/config jobs | Serialized writes |
| Network | Wi-Fi/MQTT/time | Backoff and watchdog-safe |
| Upload | HTTPS/image replay | Rate and memory bounded |
| Camera | capture lifecycle | On demand |
| Supervisor | health/watchdog | Monitors heartbeats |

Queues shall have explicit depth, item size, overflow policy, and telemetry. Critical events shall be persisted before acknowledgement. Coalescible telemetry may replace older unsent values; feeding/calibration/security events may not.

## 24. SD Card and Offline Synchronization

```text
/smartbowl/
  queue/events/
  queue/images/
  sessions/
  diagnostics/
  tmp/
```

Records shall use versioned, checksummed framing and atomic rename. Startup shall remove or recover temporary files. Replay order shall preserve per-session causality. Server idempotency uses `message_id`; deletion occurs only after durable acknowledgement. Storage pressure shall delete expired uploaded media first, never silently delete unacknowledged critical events, and notify the user/cloud.

## 25. OTA Workflow

The device receives a signed manifest containing target model, version, minimum version, image URL, size, digest, signature, and rollout metadata. It shall verify model compatibility, anti-rollback policy, power/storage conditions, TLS, size, digest, and signature before selecting the update partition. After reboot, self-test shall mark the image valid; failure or timeout shall roll back.

OTA shall preserve configuration and queue data. Progress shall be displayed and published without exposing URLs/tokens.

## 26. Logging, Diagnostics, and Error Handling

Severity levels: Debug, Info, Warning, Error, Critical. Production defaults to Info. Logs use stable event codes and shall redact credentials and personal data.

Subsystem health is OK, Degraded, Failed, or Unknown. Diagnostics include firmware/build, reset reason, uptime, heap/PSRAM, storage, weight ADC, calibration, display/I/O, camera, Wi-Fi/RSSI, time, MQTT, queue depth, battery, and OTA state.

Recovery sequence: detect → classify → bounded local retry → reset affected peripheral/service → degrade while preserving core function → notify UI/cloud → reboot only when necessary. Reboot loops shall enter safe mode.

## 27. SOPs

### 27.1 Manufacturing and Factory Flash

1. Verify PCB revision and device serial against work order.
2. Inspect power rails and current limit before full power.
3. Flash approved bootloader, partitions, firmware, and factory data.
4. Provision unique identity using an audited station; never export private keys.
5. Enable security fuses only after flash/readback and recovery checks.
6. Run factory tests; store signed result against serial.
7. Apply label and release only passing units.

### 27.2 Factory Test

Test rails/current, flash/PSRAM, unique ID, buttons/LEDs/buzzer, TFT pattern/backlight, I2C devices, weight ADC raw response, camera frame, SD read/write, battery ADC, Wi-Fi/BLE, TLS identity, and reset/watchdog. Limits are defined by the production test specification, not hard-coded ad hoc.

### 27.3 Calibration

Warm up → verify level/empty fixture → acquire stable zero → apply traceable reference → acquire stable span → compute and validate → test at zero and at least two masses → commit record → upload audit result.

### 27.4 Service and Maintenance

Technicians shall identify the device, export sanitized diagnostics, inspect physical damage/contamination, run subsystem tests, recalibrate only when justified, replace approved modules, verify security state, and record action. Private keys shall not be copied between devices.

### 27.5 Factory Reset

Require deliberate physical confirmation, stop sessions/uploads, erase customer credentials and data, retain immutable manufacturing identity as policy permits, reboot into provisioning, and record a non-sensitive reset reason.

## 28. Test Cases

| Test ID | Procedure summary | Expected result |
|---|---|---|
| TC-BOOT-001 | Boot with all devices, then each optional device absent. | Normal or explicit degraded mode; no hang. |
| TC-WGT-001 | Apply traceable masses across approved range. | Meets approved accuracy/repeatability limits. |
| TC-WGT-002 | Disturb bowl and apply slow drift. | Stability state changes correctly with hysteresis. |
| TC-CAL-001 | Tare/span, reboot, corrupt one stored copy. | Valid calibration persists or safe fallback occurs. |
| TC-UI-001 | Navigate every screen and inject degraded states. | Correct focus, response, and unambiguous status. |
| TC-MENU-001 | Send valid, duplicate, malformed, and future-schema menus. | Only valid supported menu becomes active. |
| TC-FEED-001 | Run normal, interrupted, bowl-removal, and reboot sessions. | Correct event order and nonnegative consumption. |
| TC-WIFI-001 | Remove AP, rotate credentials, restore network. | Offline operation continues; reconnect is bounded. |
| TC-BLE-001 | Provision valid/invalid data and attempt unauthorized ownership reset. | Validation and physical authorization enforced. |
| TC-MQTT-001 | Duplicate commands, invalid cert, malformed JSON, broker outage. | No duplicate side effect; failures are safe. |
| TC-CAM-001 | Capture with/without SD/network and low storage. | Durable file or explicit error; later upload works. |
| TC-OFF-001 | Generate events offline, reboot, reconnect twice. | Ordered exactly-once logical ingestion via IDs. |
| TC-PWR-001 | Sweep battery ADC and charging states. | Approved thresholds and UI/cloud state match. |
| TC-OTA-001 | Valid image, bad digest/signature, power loss, failed self-test. | Reject or roll back safely; data preserved. |
| TC-DIAG-001 | Inject every subsystem fault and export report. | Correct health; no secrets present. |
| TC-RESET-001 | Attempt accidental and confirmed reset. | Accidental reset blocked; confirmed data erased. |

Integration testing shall include 24-hour measurement/network soak, queue saturation, repeated brownouts, MQTT reconnect storms, concurrent SD/TFT activity, camera memory pressure, expired certificates, backend duplication, and OTA during degraded connectivity.

Production acceptance requires all Must tests passing, zero open Critical/High defects, signed firmware provenance, approved calibration, security provisioning evidence, and traceability to serial number.

## 29. Coding Standards

Use fixed-width types for protocols and storage; RAII where supported; no unbounded string copies; explicit units in names; `const` by default; no credentials in source; no hidden dynamic allocation in timing-critical loops; bounded timeouts; structured errors; and comments that explain decisions, not syntax. Formatting and static analysis shall run in CI. Hardware-independent state machines and codecs require unit tests.

## 30. Configuration Defaults and Open Decisions

No placeholder threshold may ship as a production default without validation. The following block release:

1. Approved schematic versus spreadsheet reconciliation.
2. NAU7802 wiring/address; the legacy HX711 GPIO21/47 mapping is superseded.
3. ST7735 variant, dimensions, rotation, color order, and SPI limits. Its GPIO47/14/21/38/39 wiring is confirmed.
4. SPI versus SD_MMC storage architecture.
5. Camera sensor/module and final power/reset design, including reassignment of the legacy CAMERA_PCLK GPIO14 conflict.
6. Battery divider, chemistry, charger signals, and percentage model.
7. Weight range, accuracy, filtering, stability, and session thresholds.
8. MQTT broker/security profile and finalized schemas.
9. Backend REST/blob contracts and retention policy.
10. BLE ownership/authentication protocol.
11. Partition table, secure boot, encryption, anti-rollback, and factory recovery.
12. Environmental, power, endurance, and regulatory requirements.

## 31. Traceability and Release Checklist

Every release shall link product need → FSD requirement → design/module → code review → test case → result → firmware artifact. Release evidence shall include compiler/platform versions, dependency lock, binary hashes, signed manifest, hardware revision, migrations, known limitations, and rollback compatibility.

## 32. Future Roadmap

Possible extensions include water monitoring, feeder motor control, RFID identity, multiple pets, edge inference, advanced analytics, notifications, and voice integrations. None is part of the current release baseline. New features require safety, privacy, power, storage, and backward-compatibility analysis.

## Appendix A — Example Telemetry

```json
{
  "schema_version": 1,
  "message_id": "uuid",
  "msg_type": "telemetry",
  "product_code": "iotbowl",
  "model_code": "ibw001",
  "serial_number": "pw-ibw001-2602-00001",
  "fw_version": "0.1.0",
  "timestamp": "2026-07-06T12:00:00Z",
  "seq": 120,
  "weight_g": 152.3,
  "stability": "stable",
  "bowl_state": "consuming",
  "battery_percent": 87,
  "rssi_dbm": -42,
  "queue_depth": 0
}
```

## Appendix B — Example Feeding Event

```json
{
  "schema_version": 1,
  "message_id": "uuid",
  "session_id": "uuid",
  "msg_type": "feeding_event",
  "event_type": "feeding_completed",
  "event_status": "success",
  "food_id": "cloud-food-id",
  "initial_food_g": 398.5,
  "leftover_g": 52.3,
  "consumed_g": 346.2,
  "duration_s": 900,
  "timestamp": "2026-07-06T12:15:00Z"
}
```

## Appendix C — Glossary

| Term | Meaning |
|---|---|
| ADC | Analog-to-digital converter |
| FSD | Functional Specification Document |
| NVS | Non-volatile storage |
| OTA | Over-the-air firmware update |
| PSRAM | External pseudo-static RAM |
| SCCB | Camera serial control bus |
| SOP | Standard operating procedure |
| Stable weight | Filtered measurement meeting approved variation/time limits |

## Appendix D — Current Repository Assessment

The PlatformIO environment targets `4d_systems_esp32s3_gen4_r8n16` with Arduino. `src/main.cpp` currently invokes a sample integer-addition function and contains no product implementation. No production dependencies, partitions, CI, tests, pin definitions, or firmware modules are present. Implementation shall begin only after resolving the hardware decisions in Section 30.
