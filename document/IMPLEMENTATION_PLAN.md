# SMART-BOWL Firmware Implementation Plan and SOP

**Document ID:** SB-IMP-001  
**Version:** 0.1  
**Date:** 2026-07-06  
**Inputs:** `FSD.md`, `project-idea.md`, approved hardware documents  
**Target:** ESP32-S3-WROOM-1U-N16R8 / Arduino-ESP32 / PlatformIO

## 1. Purpose

This SOP defines how the SMART-BOWL firmware shall be designed, implemented, integrated, verified, and released. It converts the functional specification into controlled engineering work.

The plan deliberately builds the reliable offline bowl first. Cloud, camera, and OTA features are added only after measurement, storage, and recovery behavior are trustworthy.

For day-to-day programming, execute the tasks in Section 25 in numerical order. The earlier phase gates remain the release-control layer; Section 25 is the code-level work queue.

## 2. Rules of Execution

1. Treat `FSD.md` as the product requirement baseline.
2. Treat the approved schematic and pin map as the electrical source of truth.
3. Do not invent missing pins, thresholds, credentials, URLs, or security policies.
4. Do not begin a phase until its entry criteria are met.
5. Do not close a phase until its exit tests and documentation pass.
6. Keep drivers, services, application logic, and protocols separate.
7. Use non-blocking operations and bounded timeouts.
8. Make offline operation the default design path, not an error path.
9. Persist critical events before reporting success.
10. Never commit credentials, certificates, private keys, production URLs, or customer data.
11. Every FSD “Must” requirement shall map to code and at least one test.
12. Update this plan, the FSD, and the requirement traceability matrix when scope changes.

## 3. Roles

| Role | Responsibility |
|---|---|
| Product owner | Approves behavior, priorities, acceptance limits |
| Hardware engineer | Approves schematic, pins, voltage levels, power sequencing |
| Firmware lead | Owns architecture, implementation, reviews, releases |
| Backend engineer | Owns MQTT, REST, blob, registration, and idempotency contracts |
| Mobile engineer | Owns BLE provisioning and ownership workflow |
| Security reviewer | Approves identity, certificate, secure boot, encryption, OTA trust |
| QA engineer | Owns test procedures, evidence, regression, release recommendation |
| Manufacturing engineer | Owns factory flashing, provisioning, calibration, and test station |

One person may fill multiple roles, but approvals shall remain recorded.

## 4. Required Project Artifacts

```text
document/
  FSD.md
  IMPLEMENTATION_PLAN.md
  ARCHITECTURE.md
  TRACEABILITY.md
  TEST_PLAN.md
  RELEASE_CHECKLIST.md
tft_smart_bowl/
  include/
  src/
    app/
    drivers/
    platform/
    protocols/
    services/
  test/
    native/
    embedded/
    integration/
  partitions/
  scripts/
  platformio.ini
```

Each phase shall update the relevant artifacts. Architecture decisions that affect multiple modules shall be recorded in `document/ARCHITECTURE.md`.

## 5. Phase and Gate Overview

| Phase | Outcome | Gate |
|---|---|---|
| 0 | Requirements and hardware baseline approved | G0 |
| 1 | Reproducible project foundation | G1 |
| 2 | Board support and hardware abstraction | G2 |
| 3 | Reliable weight measurement and calibration | G3 |
| 4 | Local UI and physical controls | G4 |
| 5 | Feeding state machine and session records | G5 |
| 6 | Durable SD storage and offline queue | G6 |
| 7 | Wi-Fi, time, provisioning, and identity | G7 |
| 8 | MQTT and cloud configuration/menu | G8 |
| 9 | Camera and HTTPS image upload | G9 |
| 10 | Battery, diagnostics, and resilience | G10 |
| 11 | Secure OTA | G11 |
| 12 | System validation and production release | G12 |

Phases may be developed on branches in parallel only when their interfaces are approved. Gate order remains mandatory for release.

## 6. Phase 0 — Requirements and Hardware Baseline

### Entry criteria

- `FSD.md` and `project-idea.md` available.
- Current schematic, PCB revision, BOM, pin map, and component datasheets available.

### Procedure

1. Reconcile the schematic with the pin-map workbook.
2. Resolve NAU7802 versus legacy HX711 usage.
3. Confirm ST7735 variant, resolution, rotation, color order, voltage, and SPI limit.
4. Confirm microSD interface: shared SPI or SD_MMC.
5. Confirm camera sensor, full data mapping, reset, power, clock, and IR-cut design.
6. Confirm MCP23017 address, reset, interrupt polarity, button bias, and supply voltage.
7. Review GPIO0, 3, 45, and 46 strapping risks.
8. Confirm battery chemistry, divider ratio, charger/status signals, and safe thresholds.
9. Approve weight capacity, accuracy, repeatability, stability, and calibration reference masses.
10. Approve MQTT, REST, blob, BLE, and OTA contracts.
11. Approve partition layout and device security lifecycle.
12. Replace all release-blocking FSD TBDs with approved values or tracked decisions.

### Deliverables

- Approved hardware interface table.
- Approved cloud and BLE interface schemas.
- Approved security architecture.
- Initial `TRACEABILITY.md`.
- Risk register with owner and target date.

### Gate G0

Do not start production driver integration while ADC, display, storage interface, or unsafe pin usage remains unresolved. Gate passes when Hardware, Firmware, Backend, QA, and Security approve their interfaces.

## 7. Phase 1 — Project Foundation

### Procedure

1. Replace the starter `main.cpp` with minimal application bootstrap.
2. Establish the layered directory structure.
3. Pin the PlatformIO platform, framework, board, compiler options, and dependencies.
4. Add debug and release environments.
5. Define firmware version and build metadata generation.
6. Add formatting, static analysis, warnings, and CI build checks.
7. Add native unit-test and embedded smoke-test environments.
8. Add typed result/error codes, units, logging interface, and clock abstraction.
9. Add configuration headers without secrets.
10. Define coding and review rules.

### Verification

- Clean clone builds without local-only files.
- Debug and release builds succeed.
- Unit-test runner executes a sample test.
- Build embeds version, commit, build type, and hardware revision.
- Compiler warnings are treated according to the approved policy.

### Gate G1

Pass when CI reproduces the local build and no secret or machine-specific path is committed.

## 8. Phase 2 — Board Support and Hardware Abstraction

### Procedure

1. Create a single approved pin-definition module.
2. Initialize all outputs to safe states before peripheral startup.
3. Implement service UART and structured boot logging.
4. Implement I2C manager with locking, timeout, scan diagnostics, and bus recovery.
5. Implement SPI manager with serialized access and per-device transaction settings.
6. Implement MCP23017 driver and interrupt/input service.
7. Implement button debounce and LED/buzzer output control.
8. Implement ST7735 low-level initialization and test pattern.
9. Implement SD mount and basic read/write test.
10. Implement camera power/reset and sensor-probe test.
11. Implement reset-reason capture and watchdog supervision.

### Verification

- Every approved pin is checked against the schematic.
- Boot produces no unintended LED, buzzer, backlight, camera, or IR-cut activation.
- I2C recovers after a simulated stuck bus.
- TFT and SD can share the bus without simultaneous chip select.
- Missing optional devices produce a degraded status, not a boot hang.

### Gate G2

Pass when the hardware smoke-test checklist succeeds on at least two representative boards and results are saved by board serial number.

## 9. Phase 3 — Weight Measurement and Calibration

### Procedure

1. Implement the approved NAU7802 driver and readiness timeout.
2. Capture raw sample diagnostics without applying guessed calibration.
3. Implement sample validation, outlier rejection, filtering, and stability detection.
4. Implement physical units and calibrated conversion.
5. Implement tare and span workflows.
6. Store two validated calibration records using version and CRC.
7. Detect missing, corrupt, or incompatible calibration.
8. Publish typed weight snapshots to the event system.
9. Add calibration audit events.
10. Characterize noise, drift, settling time, temperature sensitivity, and creep.

### Verification

- Run FSD tests TC-WGT-001, TC-WGT-002, and TC-CAL-001.
- Test zero and approved reference masses across the operating range.
- Power-cycle during calibration commit.
- Disconnect or hold the ADC bus and confirm bounded recovery.
- Confirm invalid calibration cannot produce a trusted consumption result.

### Gate G3

Pass when approved accuracy, repeatability, stability, and recovery limits are met and test data is attached to the release evidence.

## 10. Phase 4 — Local UI and Controls

### Procedure

1. Implement display manager as the only TFT owner.
2. Implement reusable screen, widget, status-icon, and dialog primitives.
3. Add Boot, Provisioning, Dashboard, Food Selection, Weight, Feeding, Settings, Calibration, Diagnostics, Error, and OTA screens.
4. Implement button press, release, repeat, and long-press events.
5. Implement focus and navigation state independent of rendering.
6. Distinguish valid, unstable, stale, and unavailable weight states.
7. Add Wi-Fi, cloud, battery, storage, and degraded-mode indicators.
8. Add confirmation flows for calibration, credential clearing, and factory reset.
9. Add bounded partial redraw and backlight control.

### Verification

- Run TC-UI-001.
- Confirm visible response within the FSD latency target.
- Confirm navigation works without network or SD.
- Confirm critical states are not indicated by color alone.
- Soak-test display updates while weight sampling continues.

### Gate G4

Pass after Product and QA approve every screen state and destructive operations resist accidental input.

## 11. Phase 5 — Bowl Logic and Feeding Sessions

### Procedure

1. Implement hardware-independent bowl states: Unavailable, Empty, Food Present, Consuming, Leftover, and Session Aborted.
2. Define thresholds through versioned configuration.
3. Apply hysteresis, stability windows, and minimum state durations.
4. Implement food selection and initial-mass capture.
5. Calculate current, leftover, consumed mass, and duration.
6. Generate a UUID and monotonic event sequence for every session.
7. Persist state-transition intent through the storage interface.
8. Recover or explicitly close an interrupted session after reboot.
9. Unit-test the state machine with recorded and synthetic weight traces.

### Verification

- Run TC-FEED-001.
- Cover normal feeding, noise, bowl removal, food addition, refill, negative readings, sensor failure, reboot, and timeout.
- Confirm consumed mass never becomes negative.
- Confirm duplicate input events do not create duplicate sessions.

### Gate G5

Pass when recorded traces produce approved states and session totals, independent of cloud connectivity.

## 12. Phase 6 — Storage and Offline Queue

### Procedure

1. Implement the FSD SD directory structure.
2. Implement versioned, checksummed event records.
3. Use temporary file, flush, and atomic rename for commits.
4. Implement queue indexes and monotonic sequence recovery.
5. Define event retention and storage-reserve policies.
6. Coalesce replaceable telemetry but never silently drop critical events.
7. Implement acknowledgement-based deletion.
8. Implement startup recovery for temporary, partial, or corrupt files.
9. Implement image metadata and session linkage.
10. Expose queue depth and storage health to diagnostics.

### Verification

- Run TC-OFF-001.
- Remove power during each write stage.
- Fill the card, remove it, replace it, corrupt records, and reboot repeatedly.
- Confirm event order and logical exactly-once ingestion through message IDs.

### Gate G6

Pass after power-failure testing demonstrates no silent loss of committed critical events.

## 13. Phase 7 — Connectivity, Provisioning, and Identity

### Procedure

1. Implement Wi-Fi station connection with bounded exponential backoff and jitter.
2. Implement RSSI and connection-state reporting.
3. Implement trusted time synchronization and clock-valid state.
4. Implement BLE setup service with authenticated session and strict field limits.
5. Store Wi-Fi credentials atomically.
6. Implement device registration and ownership confirmation.
7. Provision or enroll unique device credentials using the approved security design.
8. Disable BLE after successful setup.
9. Implement physical authorization for ownership reset and credential clearing.

### Verification

- Run TC-WIFI-001 and TC-BLE-001.
- Test invalid credentials, unavailable AP, captive portal, time failure, repeated setup, oversized fields, unauthorized reset, and reboot during save.
- Confirm secrets never appear in serial logs or diagnostics.

### Gate G7

Pass after Mobile, Backend, Security, and QA complete an end-to-end setup on a factory-reset device.

## 14. Phase 8 — MQTT and Cloud Data

### Procedure

1. Implement TLS MQTT client using approved trust material.
2. Implement base-topic construction and topic validation.
3. Implement versioned serializers and parsers with size limits.
4. Publish retained status, telemetry, events, calibration, and OTA status.
5. Subscribe to acknowledgements and authorized commands.
6. Implement command ID deduplication and cached responses.
7. Implement food-menu download, validation, activation, and cache fallback.
8. Implement configuration versioning and safe application.
9. Connect offline replay to durable server acknowledgements.
10. Add reconnect backoff, session policy, keepalive, and metrics.

### Verification

- Run TC-MQTT-001 and TC-MENU-001.
- Test invalid/expired certificates, malformed JSON, oversized payloads, duplicate commands, retained messages, broker restart, network flapping, old/new schemas, and acknowledgement loss.

### Gate G8

Pass when backend contract tests prove compatible schemas, authorization, idempotency, replay, and offline recovery.

## 15. Phase 9 — Camera and Image Upload

### Procedure

1. Implement camera manager with exclusive ownership, timeout, and PSRAM-aware buffers.
2. Implement manual, eating-start, feeding-complete, and remote triggers.
3. Apply privacy enablement, cooldown, burst, and storage limits.
4. Validate JPEG size and structure.
5. Persist images before upload.
6. Obtain approved short-lived upload authorization.
7. Upload with HTTPS certificate validation and bounded streaming buffers.
8. Post metadata and publish the blob result only after confirmed success.
9. Retry offline images according to retention and backoff policy.
10. Remove uploaded media according to the approved policy.

### Verification

- Run TC-CAM-001.
- Test missing camera, missing/full SD, bad JPEG, low memory, expired URL, HTTP timeout, duplicate response, reboot during upload, and privacy-disabled state.
- Confirm weighing and UI remain responsive during capture/upload.

### Gate G9

Pass when images and metadata remain correctly linked to sessions with no token leakage or duplicate logical records.

## 16. Phase 10 — Power, Diagnostics, and Resilience

### Procedure

1. Implement battery ADC conversion using the approved divider and calibration.
2. Implement percentage/state model and charger detection.
3. Implement low-battery warnings and safe shutdown policy.
4. Implement subsystem health states and diagnostic screen/export.
5. Implement task heartbeats and watchdog escalation.
6. Implement brownout and reset-reason records.
7. Implement safe mode after repeated boot failures.
8. Add heap, PSRAM, stack, queue, storage, and reconnect metrics.
9. Redact all diagnostic output.
10. Run long-duration fault-injection and resource-leak tests.

### Verification

- Run TC-PWR-001 and TC-DIAG-001.
- Perform 24-hour minimum soak, reconnect storm, storage pressure, camera memory pressure, peripheral disconnect, brownout, and watchdog tests.

### Gate G10

Pass when no unbounded resource growth, reboot loop, secret exposure, or loss of core weighing is observed.

## 17. Phase 11 — Secure OTA

### Procedure

1. Freeze and approve the partition table.
2. Define signed manifest and firmware signature formats.
3. Enforce model, version, size, digest, signature, and anti-rollback checks.
4. Check battery, storage, session, and connectivity preconditions.
5. Stream firmware into the inactive partition.
6. Report safe progress to UI and MQTT.
7. Reboot into self-test and mark the image valid only after success.
8. Roll back automatically after failed self-test or boot timeout.
9. Preserve configuration, calibration, sessions, and queues.
10. Document factory recovery for devices with security fuses enabled.

### Verification

- Run TC-OTA-001.
- Test bad signature/digest, wrong model, downgrade, expired certificate, power loss at multiple percentages, full storage, failed self-test, and network loss.

### Gate G11

Pass only after Security and QA approve signing custody, rollback, anti-rollback, and recovery evidence.

## 18. Phase 12 — Validation and Production Release

### Procedure

1. Run all FSD unit, integration, system, security, and production tests.
2. Validate on the approved number of boards and hardware revisions.
3. Complete environmental, endurance, power, and weight characterization.
4. Perform provisioning-to-dashboard end-to-end testing.
5. Perform factory flash, identity, calibration, and test-station dry run.
6. Audit requirements traceability.
7. Resolve all Critical and High defects.
8. Generate release build in controlled CI.
9. Sign firmware and produce hashes, manifest, symbols, map, and release notes.
10. Archive source revision, dependencies, test evidence, approvals, and rollback image.
11. Execute a limited pilot rollout before general release.
12. Monitor crashes, reconnects, queue depth, OTA, battery, and data-quality indicators.

### Gate G12

Release is allowed only when:

- every Must requirement has passed evidence;
- zero Critical or High defects remain open;
- hardware, cloud, mobile, security, factory, and service interfaces are approved;
- production identity and signing processes are operational;
- rollback and recovery procedures have been demonstrated;
- the release checklist is signed.

## 19. Definition of Done for Any Work Item

A work item is done only when:

- acceptance criteria and requirement IDs are identified;
- code follows architecture and coding standards;
- errors, timeouts, concurrency, and power-loss behavior are handled;
- unit tests and relevant hardware tests pass;
- logs and diagnostics contain no secrets;
- documentation and traceability are updated;
- another engineer reviews the change;
- CI passes in debug and release configurations;
- test evidence is linked to the work item.

## 20. Branch, Review, and Change SOP

1. Create a small branch named by feature or issue.
2. Link the change to FSD requirement and test IDs.
3. Keep commits buildable and narrowly scoped.
4. Run formatter, static analysis, unit tests, and affected hardware tests.
5. Open review with behavior, risks, rollback, and evidence.
6. Require specialist review for hardware, security, storage schema, or protocol changes.
7. Merge only after CI and approvals pass.
8. Never rewrite released tags.
9. Use an approved migration for persistent or network schema changes.
10. Update release notes for user-visible or compatibility changes.

## 21. Requirement Traceability Seed

| Requirement | Primary phase | Primary module | Test |
|---|---:|---|---|
| FR-BOOT-001 | 2 | Boot/Supervisor | TC-BOOT-001 |
| FR-WGT-001, FR-WGT-002 | 3 | Weight service | TC-WGT-001/002 |
| FR-CAL-001 | 3 | Calibration service | TC-CAL-001 |
| FR-UI-001 | 4 | UI manager | TC-UI-001 |
| FR-FEED-001 | 5 | Bowl/session app | TC-FEED-001 |
| FR-OFF-001 | 6 | Storage/sync | TC-OFF-001 |
| FR-WIFI-001 | 7 | Connectivity | TC-WIFI-001 |
| FR-BLE-001 | 7 | Provisioning | TC-BLE-001 |
| FR-MQTT-001 | 8 | MQTT protocol | TC-MQTT-001 |
| FR-MENU-001 | 8 | Menu service | TC-MENU-001 |
| FR-CAM-001 | 9 | Camera/upload | TC-CAM-001 |
| FR-PWR-001 | 10 | Power service | TC-PWR-001 |
| FR-DIAG-001 | 10 | Diagnostics | TC-DIAG-001 |
| FR-OTA-001 | 11 | OTA service | TC-OTA-001 |
| FR-RESET-001 | 7/10 | Provisioning/system | TC-RESET-001 |
| NFR-REL-001 | 3/6 | Config/storage | Power-loss tests |
| NFR-REL-002 | 2/10 | Supervisor | Watchdog fault injection |
| NFR-PERF-001 | 4 | UI manager | Input-latency measurement |
| NFR-PERF-002 | 1–11 | All tasks | Static review and soak test |
| NFR-SEC-001 | 7–11 | TLS/security | Invalid-chain tests |
| NFR-SEC-002 | 1–12 | All modules | Secret-leak audit |
| NFR-MAINT-001 | 1 | Architecture | Dependency review |
| NFR-MAINT-002 | 3/6/8/11 | Schemas | Migration tests |
| NFR-OBS-001 | 1/10 | Logging | Log-format tests |
| NFR-ACC-001 | 4 | UI manager | UI state review |

The complete matrix shall include every functional and non-functional requirement before Gate G12.

## 22. Initial Backlog

### Blockers

- Reconcile NAU7802 with HX711 pin-map entries.
- Approve ST7735 variant and SD bus architecture.
- Approve final camera and battery circuits.
- Approve backend, BLE, certificate, and OTA contracts.

### First implementation increment

1. Establish project structure and CI.
2. Add pin definitions and boot logging.
3. Bring up I2C and MCP23017.
4. Bring up ST7735 test screen and buttons.
5. Bring up NAU7802 raw readings.
6. Implement calibrated stable weight display.

### First demonstrable milestone

A device boots safely, accepts button input, displays live calibrated weight on the ST7735, reports explicit sensor faults, and preserves calibration across power loss. It requires no Wi-Fi or cloud service.

## 23. Schedule Guidance

Estimate duration only after Phase 0 decisions and staffing are known. Track progress by gate evidence, not percentage-complete claims. Hardware availability, backend readiness, security provisioning, characterization, and certification are critical-path dependencies.

## 24. Implementation Start Approval

Before coding Phase 2 or later, record:

```text
Hardware revision:
Approved schematic:
Approved pin-map revision:
Firmware owner:
Backend contract revision:
BLE contract revision:
Security design revision:
Test hardware quantity:
G0 approval date:
Approvers:
```

Blank fields mean Gate G0 has not passed.

## 25. Sequential Programming Task Backlog

### 25.1 How to execute a task

For each task:

1. Confirm every dependency is marked complete.
2. Create a branch such as `task/fw-010-project-layout`.
3. Implement only the stated scope.
4. Add or update the listed tests.
5. Build both debug and release environments.
6. Run the task acceptance check on hardware when required.
7. Record the result, board serial number, and evidence link.
8. Update `TRACEABILITY.md`.
9. Commit using the task ID, for example `FW-010: add project layout`.
10. Mark the task complete only after review.

Task status values are `TODO`, `IN PROGRESS`, `BLOCKED`, and `DONE`.

### 25.2 Milestone A — Project Skeleton

#### FW-001 — Freeze confirmed pin assignments

- **Depends on:** Hardware review.
- **Create/update:** `include/board/Pins.h`, `document/FSD.md`.
- **Implement:** TFT CS 47, DC 14, RST 21, MOSI 38, SCLK 39; MCP23017 buttons GPA0–2 and LEDs GPB0–2.
- **Do not implement:** Camera PCLK until its GPIO14 conflict is resolved.
- **Pass:** Pin definitions match the approved schematic and contain compile-time duplicate-pin checks.
- **Status:** DONE.

#### FW-002 — Resolve remaining hardware blockers

- **Depends on:** FW-001.
- **Output:** Approved ADC, I2C, SD, camera, battery, and interrupt assignments.
- **Pass:** No active peripheral shares an incompatible pin or voltage level.
- **Status:** BLOCKED pending hardware approval.

#### FW-010 — Create firmware directory structure

- **Depends on:** None.
- **Create:** `src/app`, `src/drivers`, `src/platform`, `src/protocols`, `src/services`, and matching `include` folders.
- **Implement:** Minimal `setup()` and `loop()` that call an `Application` object.
- **Pass:** Project builds without the sample `myFunction`.
- **Status:** DONE.

#### FW-011 — Configure PlatformIO environments

- **Depends on:** FW-010.
- **Update:** `platformio.ini`.
- **Implement:** Common settings plus debug, release, native-test, and embedded-test environments; pin dependency versions.
- **Pass:** Debug and release builds succeed from a clean checkout.
- **Status:** DONE.

#### FW-012 — Add build information

- **Depends on:** FW-011.
- **Create:** `include/platform/BuildInfo.h`, build script if required.
- **Implement:** Semantic version, Git revision, build date, environment, and hardware revision.
- **Pass:** Firmware prints the correct build identity at boot.
- **Status:** DONE.

#### FW-013 — Add core types and result codes

- **Depends on:** FW-010.
- **Create:** `include/core/Result.h`, `Units.h`, `Errors.h`.
- **Implement:** Typed grams, milliseconds, voltage, error category, and non-exception result handling.
- **Pass:** Unit tests cover success and error propagation.
- **Status:** DONE.

#### FW-014 — Add logging service

- **Depends on:** FW-013.
- **Create:** `services/Logger`.
- **Implement:** Timestamp, severity, module, event code, and redacted context.
- **Pass:** Logs never print registered secret fields.
- **FSD:** NFR-OBS-001, NFR-SEC-002.
- **Status:** DONE.

#### FW-015 — Add test and CI baseline

- **Depends on:** FW-011.
- **Create:** native sample test, embedded smoke test, CI workflow.
- **Implement:** Build, format, static-check, and test jobs.
- **Pass:** CI passes from a clean clone and fails for a deliberately failing test.
- **Status:** DONE.

### 25.3 Milestone B — Safe Board Bring-up

#### FW-020 — Implement safe boot GPIO initialization

- **Depends on:** FW-001, FW-010, FW-014.
- **Create:** `drivers/GpioSafeState`.
- **Implement:** LEDs, buzzer, backlight, camera power, and IR controls start inactive.
- **Pass:** Power-up produces no unintended output pulse.
- **FSD:** FR-BOOT-001.
- **Status:** DONE.

#### FW-021 — Implement reset reason and watchdog

- **Depends on:** FW-020.
- **Create:** `platform/ResetInfo`, `services/Supervisor`.
- **Implement:** Reset-reason capture, task heartbeats, watchdog registration, and safe-mode counter.
- **Pass:** A deliberately stalled test task resets the device and records the reason.
- **FSD:** NFR-REL-002.
- **Status:** DONE.

#### FW-022 — Implement I2C bus manager

- **Depends on:** FW-002, FW-020.
- **Create:** `drivers/I2cBus`.
- **Implement:** Initialization, mutex, bounded transaction, scan, error counters, and stuck-bus recovery.
- **Pass:** Bus recovers after SDA is temporarily held low.
- **Status:** DONE.

#### FW-023 — Implement MCP23017 driver

- **Depends on:** FW-022.
- **Create:** `drivers/Mcp23017`.
- **Implement:** Address probe, direction, pull-up, input, output, interrupt, and fault state.
- **Pass:** Register read/write test passes after reboot and simulated I2C failure.
- **Status:** DONE.

#### FW-024 — Implement buttons

- **Depends on:** FW-023.
- **Create:** `drivers/Buttons`.
- **Implement:** BUTTON_1/2/3 on GPA0/1/2 with debounce, press, release, repeat, and long press.
- **Pass:** Each physical press creates exactly one normal press event.
- **Status:** DONE.

#### FW-025 — Implement indicator LEDs

- **Depends on:** FW-023.
- **Create:** `drivers/Leds`.
- **Implement:** LED_1/2/3 on GPB0/1/2, default off, steady and timed patterns.
- **Pass:** Button-to-LED hardware test maps 1→1, 2→2, and 3→3.
- **Status:** DONE.

#### FW-026 — Implement SPI bus manager

- **Depends on:** FW-001, FW-020.
- **Create:** `drivers/SpiBus`.
- **Implement:** GPIO38 MOSI, GPIO39 SCLK, optional GPIO40 MISO, mutex, and per-device transaction settings.
- **Pass:** Concurrent test clients cannot overlap chip-select transactions.
- **Status:** DONE.

#### FW-027 — Bring up ST7735

- **Depends on:** FW-026.
- **Create:** `drivers/TftDisplay`.
- **Implement:** CS47, DC14, RST21, initialization, rotation, clear, text, rectangles, and test pattern.
- **Pass:** Color bars and orientation labels display correctly for 30 minutes.
- **Status:** DONE.

#### FW-028 — Add board self-test

- **Depends on:** FW-021–FW-027.
- **Create:** `app/BoardSelfTest`.
- **Implement:** I2C probe, MCP test, button/LED test, TFT test, memory report, and structured result.
- **Pass:** Self-test identifies each disconnected device without hanging.
- **Status:** DONE.

### 25.4 Milestone C — Weight Measurement

#### FW-030 — Implement NAU7802 low-level driver

- **Depends on:** FW-002, FW-022.
- **Create:** `drivers/Nau7802`.
- **Implement:** Reset, probe, channel/gain/rate setup, internal calibration, ready timeout, and raw read.
- **Pass:** Raw readings change consistently when load is applied; missing ADC returns a bounded error.
- **Status:** DONE.

#### FW-031 — Implement sample filtering

- **Depends on:** FW-030.
- **Create:** `services/WeightFilter`.
- **Implement:** Outlier rejection and approved low-pass/moving filter without dynamic allocation in the sample loop.
- **Pass:** Recorded-noise unit tests meet the approved response/noise limit.
- **Status:** DONE.

#### FW-032 — Implement stability detection

- **Depends on:** FW-031.
- **Create:** `services/StabilityDetector`.
- **Implement:** Window, variation threshold, minimum duration, and hysteresis.
- **Pass:** Stable, disturbed, drifting, and step-change traces classify correctly.
- **FSD:** FR-WGT-002.
- **Status:** DONE.

#### FW-033 — Implement calibration model

- **Depends on:** FW-030, FW-013.
- **Create:** `services/Calibration`.
- **Implement:** Raw zero, raw span, reference grams, factor, validation, version, and CRC.
- **Pass:** Known raw values convert to expected grams; invalid span is rejected.
- **Status:** DONE.

#### FW-034 — Persist calibration safely

- **Depends on:** FW-033.
- **Create:** `services/ConfigStore`.
- **Implement:** Two-copy copy/validate/commit storage with schema version and CRC.
- **Pass:** Power interruption during save leaves one valid calibration record.
- **FSD:** FR-CAL-001, NFR-REL-001.
- **Status:** TODO.

#### FW-035 — Implement tare and span procedures

- **Depends on:** FW-032–FW-034, FW-024.
- **Create:** `app/CalibrationController`.
- **Implement:** Stable empty check, tare, reference entry, stable span, validation, commit, and audit event.
- **Pass:** TC-CAL-001 passes with approved reference weights.
- **Status:** TODO.

#### FW-036 — Implement weight service

- **Depends on:** FW-031–FW-034.
- **Create:** `services/WeightService`.
- **Implement:** Periodic sampling task and immutable raw/filtered/grams/stability/fault snapshot.
- **Pass:** UI or logging load does not violate the approved sample timing.
- **FSD:** FR-WGT-001, NFR-PERF-002.
- **Status:** TODO.

#### FW-037 — Display live calibrated weight

- **Depends on:** FW-027, FW-036.
- **Create:** First operational dashboard.
- **Implement:** Grams, stability, stale/fault state, and calibration warning.
- **Pass:** The first milestone works completely without Wi-Fi.
- **Status:** TODO.

### 25.5 Milestone D — UI and Feeding Logic

#### FW-040 — Implement UI framework

- **Depends on:** FW-024, FW-027.
- **Create:** `app/ui/Screen`, `UiManager`, widgets, theme.
- **Implement:** Screen lifecycle, focus, navigation, dialogs, and bounded partial redraw.
- **Pass:** Button input receives visible acknowledgement within 150 ms.
- **FSD:** NFR-PERF-001.
- **Status:** DONE.

#### FW-041 — Implement required screens

- **Depends on:** FW-040.
- **Implement:** Boot, Provisioning, Dashboard, Food, Weight, Feeding, Settings, Calibration, Diagnostics, Error, and OTA screens.
- **Pass:** TC-UI-001 passes, including degraded states and non-color-only alerts.
- **FSD:** FR-UI-001, NFR-ACC-001.
- **Status:** DONE.

#### FW-042 — Implement event bus

- **Depends on:** FW-013.
- **Create:** `services/EventBus`.
- **Implement:** Typed fixed-size events, queue depth, overflow metrics, and ownership rules.
- **Pass:** Critical queue overflow is explicit; replaceable telemetry can coalesce.
- **Status:** DONE.

#### FW-043 — Implement bowl state machine

- **Depends on:** FW-032, FW-036, FW-042.
- **Create:** `app/BowlStateMachine`.
- **Implement:** Unavailable, Empty, Food Present, Consuming, Leftover, and Aborted with hysteresis.
- **Pass:** Synthetic and recorded trace tests cover every state and transition.
- **Status:** DONE.

#### FW-044 — Implement food model and selection

- **Depends on:** FW-040, FW-042.
- **Create:** `app/FoodMenu`, `app/FoodSelection`.
- **Implement:** Versioned menu model, limits, selection, and no-menu state.
- **Pass:** Valid menu selections persist for the active session; malformed entries are rejected.
- **Status:** DONE.

#### FW-045 — Implement feeding session

- **Depends on:** FW-043, FW-044.
- **Create:** `app/FeedingSession`.
- **Implement:** UUID, event sequence, initial/current/leftover/consumed grams, duration, abort, and reboot-recovery model.
- **Pass:** TC-FEED-001 passes and consumed grams never becomes negative.
- **FSD:** FR-FEED-001.
- **Status:** DONE.

### 25.6 Milestone E — SD and Offline Operation

#### FW-050 — Bring up microSD

- **Depends on:** FW-002, FW-026.
- **Create:** `drivers/SdCard`.
- **Implement:** Mount, unmount, card identity, capacity, free space, bounded file operations, and fault state.
- **Pass:** Reinsert, missing-card, full-card, and read-only tests do not block core weighing.
- **Status:** DONE.

#### FW-051 — Implement atomic file records

- **Depends on:** FW-050.
- **Create:** `services/RecordStore`.
- **Implement:** Versioned header, length, CRC, temporary write, flush, and rename.
- **Pass:** Every simulated power-cut point yields a valid record or a detectable temporary file.
- **Status:** DONE.

#### FW-052 — Implement offline event queue

- **Depends on:** FW-042, FW-051.
- **Create:** `services/OfflineQueue`.
- **Implement:** Enqueue, peek, acknowledge, replay ordering, sequence recovery, limits, and telemetry coalescing.
- **Pass:** TC-OFF-001 passes across reboot and repeated acknowledgement loss.
- **FSD:** FR-OFF-001.
- **Status:** DONE.

#### FW-053 — Persist feeding sessions

- **Depends on:** FW-045, FW-051.
- **Implement:** Session snapshots and event records before external acknowledgement.
- **Pass:** Reboot during each feeding state recovers or explicitly closes the session.
- **Status:** DONE.

#### FW-054 — Implement storage pressure policy

- **Depends on:** FW-052.
- **Implement:** Reserve threshold, uploaded-media cleanup, warnings, and critical-event protection.
- **Pass:** Full storage never silently deletes an unacknowledged critical event.
- **Status:** DONE.

### 25.7 Milestone F — Provisioning and Connectivity

#### FW-060 — Implement Wi-Fi manager

- **Depends on:** FW-034, FW-042.
- **Create:** `services/WifiManager`.
- **Implement:** Station connect, state events, RSSI, bounded exponential backoff, jitter, and credential change.
- **Pass:** TC-WIFI-001 passes during AP loss and recovery.
- **Status:** TODO.

#### FW-061 — Implement trusted time

- **Depends on:** FW-060.
- **Create:** `services/TimeService`.
- **Implement:** SNTP synchronization, clock-valid state, monotonic uptime, and timestamp conversion.
- **Pass:** TLS/cloud operations remain blocked until wall-clock validity is established.
- **Status:** TODO.

#### FW-062 — Implement BLE provisioning transport

- **Depends on:** FW-034, FW-042.
- **Create:** `protocols/BleProvisioning`.
- **Implement:** Setup identifier, authenticated session, bounded fields, Wi-Fi transfer, status, timeout, and shutdown.
- **Pass:** Invalid or oversized requests cause no credential change.
- **Status:** TODO.

#### FW-063 — Implement device registration and ownership

- **Depends on:** FW-060–FW-062 and approved backend contract.
- **Create:** `services/DeviceRegistration`.
- **Implement:** Registration request, identity response, retry, and physical confirmation for ownership replacement.
- **Pass:** New, repeated, interrupted, and unauthorized registration tests pass.
- **Status:** TODO.

#### FW-064 — Implement credential and certificate store

- **Depends on:** FW-034 and approved security design.
- **Create:** `services/SecureStore`.
- **Implement:** Unique identity, private-key protection, certificate states, renewal metadata, and redacted access.
- **Pass:** No secret is readable through logs or diagnostic export.
- **FSD:** NFR-SEC-002.
- **Status:** TODO.

#### FW-065 — Implement factory reset

- **Depends on:** FW-024, FW-034, FW-052, FW-064.
- **Create:** `app/FactoryReset` (or embedded in `Application.cpp` via BOOT button).
- **Implement:** Deliberate physical confirmation (2s long press on BOOT/GPIO 0), customer-data erase (WiFi credentials via ProvisioningService), immutable identity policy, and provisioning reboot.
- **Pass:** TC-RESET-001 passes.
- **FSD:** FR-RESET-001.
- **Status:** DONE.

### 25.8 Milestone G — MQTT and Cloud Synchronization

#### FW-070 — Implement versioned JSON codecs

- **Depends on:** FW-013.
- **Create:** `protocols/messages`.
- **Implement:** Telemetry, event, status, command, acknowledgement, menu, calibration, and OTA schemas with strict size/type validation.
- **Pass:** Round-trip, malformed, missing, oversized, and future-version unit tests pass.
- **FSD:** NFR-MAINT-002.
- **Status:** TODO.

#### FW-071 — Implement MQTT TLS connection

- **Depends on:** FW-060, FW-061, FW-064.
- **Create:** `protocols/MqttClient`.
- **Implement:** Certificate validation, base topic, last will, keepalive, session policy, and reconnect backoff.
- **Pass:** Invalid, expired, and untrusted certificates are rejected.
- **FSD:** FR-MQTT-001, NFR-SEC-001.
- **Status:** TODO.

#### FW-072 — Implement MQTT publisher and replay

- **Depends on:** FW-052, FW-070, FW-071.
- **Create:** `services/CloudSync`.
- **Implement:** Publish status, telemetry, events, calibration, acknowledgements, and queued replay.
- **Pass:** Reconnect does not create duplicate logical events.
- **Status:** TODO.

#### FW-073 — Implement command dispatcher

- **Depends on:** FW-070–FW-072.
- **Create:** `services/CommandDispatcher`.
- **Implement:** Authorization, schema checks, message-ID deduplication, cached result, and audit event.
- **Pass:** Repeated command ID never repeats a side effect.
- **Status:** TODO.

#### FW-074 — Implement cloud food menu

- **Depends on:** FW-044, FW-052, FW-070–FW-073.
- **Implement:** Download, validate, atomically cache, activate, expire, and fall back to last valid menu.
- **Pass:** TC-MENU-001 passes.
- **FSD:** FR-MENU-001.
- **Status:** TODO.

### 25.9 Milestone H — Camera and HTTPS

#### FW-080 — Resolve and implement camera pins

- **Depends on:** FW-002.
- **Update:** `Pins.h`, FSD, approved schematic.
- **Implement:** Sensor probe, power, reset, clock, data pins, and safe shutdown.
- **Pass:** No conflict remains with TFT_DC GPIO14.
- **Status:** BLOCKED pending camera PCLK assignment.

#### FW-081 — Implement camera capture service

- **Depends on:** FW-080.
- **Create:** `drivers/Camera`, `services/CameraService`.
- **Implement:** PSRAM-aware buffers, timeout, JPEG validation, cooldown, and burst limit.
- **Pass:** Repeated capture does not leak heap/PSRAM.
- **Status:** TODO.

#### FW-082 — Persist captured images

- **Depends on:** FW-051, FW-081.
- **Implement:** Temporary file, validation, atomic rename, session/event naming, and metadata record.
- **Pass:** Power loss never produces a falsely valid image record.
- **Status:** TODO.

#### FW-083 — Implement HTTPS client

- **Depends on:** FW-060, FW-061, FW-064.
- **Create:** `protocols/HttpsClient`.
- **Implement:** TLS validation, bounded streaming, connect/read timeouts, response-size limit, and token-redacted errors.
- **Pass:** Bad certificate, timeout, and oversized-response tests fail safely.
- **Status:** TODO.

#### FW-084 — Implement image upload workflow

- **Depends on:** FW-072, FW-082, FW-083.
- **Create:** `services/ImageUploader`.
- **Implement:** Obtain authorization, upload, metadata confirmation, MQTT blob event, retry, and retention.
- **Pass:** TC-CAM-001 passes and duplicate retries remain idempotent.
- **FSD:** FR-CAM-001.
- **Status:** TODO.

### 25.10 Milestone I — Power, Diagnostics, and OTA

#### FW-090 — Implement battery monitor

- **Depends on:** FW-002, FW-042.
- **Create:** `drivers/BatteryAdc`, `services/PowerManager`.
- **Implement:** Approved divider conversion, calibration, percentage, charging, low state, and filtering.
- **Pass:** TC-PWR-001 passes against calibrated meter readings.
- **FSD:** FR-PWR-001.
- **Status:** BLOCKED pending battery design.

#### FW-091 — Implement diagnostics

- **Depends on:** FW-021, FW-028, FW-036, FW-050, FW-060, FW-071, FW-081, FW-090.
- **Create:** `services/Diagnostics`.
- **Implement:** Health states, memory/stacks, reset, buses, peripherals, queue, connectivity, battery, and sanitized export.
- **Pass:** TC-DIAG-001 passes with zero secret leakage.
- **FSD:** FR-DIAG-001.
- **Status:** TODO.

#### FW-092 — Implement signed OTA manifest

- **Depends on:** FW-064, FW-070, approved security contract.
- **Create:** `services/OtaManifest`.
- **Implement:** Model, version, size, digest, signature, URL, policy, and anti-rollback validation.
- **Pass:** Invalid signature, digest, model, or downgrade is rejected before writing firmware.
- **Status:** TODO.

#### FW-093 — Implement OTA download and install

- **Depends on:** FW-083, FW-090, FW-092, approved partition table.
- **Create:** `services/OtaManager`.
- **Implement:** Preconditions, inactive-partition streaming, progress, reboot, self-test, mark-valid, and rollback.
- **Pass:** TC-OTA-001 passes including power interruption.
- **FSD:** FR-OTA-001.
- **Status:** TODO.

### 25.11 Milestone J — Release Hardening

#### FW-100 — Complete requirement traceability

- **Depends on:** FW-001–FW-093.
- **Create/update:** `document/TRACEABILITY.md`.
- **Implement:** Map every FR/NFR to design, source file, test, result, and evidence.
- **Pass:** All 26 current FSD requirement IDs have complete links.
- **Status:** TODO.

#### FW-101 — Run full regression and soak

- **Depends on:** FW-100.
- **Implement:** Unit, embedded, integration, power-loss, storage-full, reconnect-storm, memory-pressure, and minimum 24-hour soak tests.
- **Pass:** No Critical/High defect, leak, reboot loop, or silent critical-event loss.
- **Status:** TODO.

#### FW-102 — Validate factory workflow

- **Depends on:** FW-101.
- **Implement:** Flash, identity, security provisioning, self-test, calibration, result upload, and label traceability.
- **Pass:** Multiple blank devices complete the process with unique identities and reproducible results.
- **Status:** TODO.

#### FW-103 — Produce release candidate

- **Depends on:** FW-102.
- **Output:** Signed binary, manifest, hashes, map, symbols, dependencies, release notes, test evidence, and rollback image.
- **Pass:** Gate G12 approval and successful pilot rollout.
- **Status:** TODO.

## 26. Recommended First Programming Sequence

Start with this exact order:

```text
FW-001 -> FW-010 -> FW-011 -> FW-012 -> FW-013 -> FW-014 -> FW-015
       -> FW-020 -> FW-021 -> FW-022 -> FW-023 -> FW-024 -> FW-025
       -> FW-026 -> FW-027 -> FW-028
       -> FW-030 -> FW-031 -> FW-032 -> FW-033 -> FW-034 -> FW-035
       -> FW-036 -> FW-037
```

Stop at FW-001 if the schematic does not confirm the supplied TFT and MCP23017 mapping. Stop before FW-030 until the NAU7802 wiring and voltage levels are approved. Completion of FW-037 is the first useful firmware release: safe boot, working buttons/LEDs, live calibrated TFT weight, explicit faults, and calibration retained across power loss.

## 27. Task Progress Board

| Milestone | Task range | Exit demonstration |
|---|---|---|
| A | FW-001–FW-015 | Reproducible firmware skeleton and CI |
| B | FW-020–FW-028 | Safe board self-test with TFT/buttons/LEDs |
| C | FW-030–FW-037 | Calibrated live weight without cloud |
| D | FW-040–FW-045 | Local food selection and feeding session |
| E | FW-050–FW-054 | Power-safe offline records |
| F | FW-060–FW-065 | Secure device provisioning |
| G | FW-070–FW-074 | MQTT synchronization and cloud menu |
| H | FW-080–FW-084 | Durable camera capture and upload |
| I | FW-090–FW-093 | Power diagnostics and signed OTA |
| J | FW-100–FW-103 | Production release candidate |
