#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <Update.h>
#include "mbedtls/md.h"
#include <time.h>

     
// SMART BOWL PHASE-1 HTTPS + PHASE-2 MQTT X.509 + OTA
//
// HTTPS provisioning:
//   - normal HTTPS /provision-test
//   - no client certificate
//   - no provision_key
//   - uses setInsecure() because old code worked like this
//
// MQTT runtime:
//   - X.509 TLS on 8883
//   - PEM credentials are NOT hardcoded in firmware
//   - device receives MQTT CA, client cert, and private key from provisioning API
//   - credentials are stored in LittleFS and reused on next boot
//   - no username/password
//
// OTA:
//   - MQTT command topic: <base_topic>/ota/cmd
//   - MQTT status topic : <base_topic>/ota/status
//   - HTTP/HTTPS firmware download
//   - SHA-256 verification is optional for test, recommended for production
     

// ---------------- WIFI ----------------
#define WIFI_SSID "Nsl"
#define WIFI_PASS "12345678"

// ---------------- BACKEND ----------------
// Keep your real working backend here.
#define BACKEND_BASE_URL "https://smartpetdev.ckdigital.in"
#define PROVISION_API_PATH "/api/v1/device/provision-test"

// ---------------- DEVICE ----------------
#define PRODUCT_CODE  "smartbowl"
#define MODEL_CODE    "sbwc001"
#define FW_VERSION    "FW-V1.0.0"
#define HW_VERSION    "HW-V1.0"

// ---------------- SERIAL OVERRIDE ----------------
// Test/production fixed serial number.
// This replaces the old MAC-derived serial: PW-SBWC001-<MAC>.
#define DEVICE_SERIAL_NUMBER "PW-SBWC001-2602-00001"

// ---------------- TEST CONTROL ----------------
// TRUE  = call provisioning API every boot, even if NVS already has MQTT identity.
// FALSE = production-like behavior: use NVS if available.
#define FORCE_PROVISION_API_ON_EVERY_BOOT true

// Set true only once if you want to clear old/corrupted NVS.
// After one successful flash/test, set it back to false.
#define RESET_NVS_ON_BOOT false

// If backend returns MQTT port 1883 from Phase-1 but we are using X.509,
// force MQTT port to 8883.
#define FORCE_MQTT_X509_PORT_8883 true

// ---------------- MQTT CREDENTIAL SOURCE ----------------
// No MQTT host/client/topic/certificate is hardcoded.
// Backend provisioning API must return MQTT identity + PEM material.

// ---------------- TIMING ----------------
#define WIFI_TIMEOUT_MS        20000UL
#define MQTT_RETRY_MS          5000UL
#define TELEMETRY_INTERVAL_MS  5000UL
#define HTTP_TIMEOUT_MS        20000UL
#define OTA_HTTP_TIMEOUT_MS    20000UL

// ---------------- OTA CONTROL ----------------

#define OTA_ALLOW_HTTP  true
#define OTA_ALLOW_HTTPS true

// Test mode:
// false = allow OTA/download test without SHA/file size.
// true  = require SHA/file size before OTA starts. Recommended for production.
#define OTA_REQUIRE_SHA256   false
#define OTA_REQUIRE_FILESIZE false


// ---------------- MANUAL OTA TEST AT BOOT ----------------
// This bypasses backend MQTT OTA trigger.
// ESP will boot, connect WiFi + MQTT, wait a few seconds,
// then directly download the OTA .bin from the URL below.
//
// IMPORTANT:
// 1. Set true only in the OLD firmware, example FW-V1.0.0.
// 2. The OTA target .bin must be built with FW_VERSION = "FW-V1.0.1".
// 3. In the OTA target .bin, set MANUAL_OTA_TEST_ON_BOOT false,
//    otherwise device may keep trying OTA again after reboot.

#define MANUAL_OTA_TEST_ON_BOOT true
#define MANUAL_OTA_TEST_DELAY_MS 30000UL

#define MANUAL_OTA_TARGET_VERSION "FW-V1.0.1"

// Paste exact direct Blob/SAS .bin URL here.
// It must point to the application .bin file only.
#define MANUAL_OTA_DOWNLOAD_URL "https://distributorfileupload.blob.core.windows.net/smartbirdbowl/SmartBowl_Local_Provisioning_OTA_Test.ino.bin?sp=racwdl&st=2026-02-03T06:27:49Z&se=2028-12-31T14:42:49Z&sv=2024-11-04&sr=c&sig=0SyvHC0F%2FS7iIsMV2KdHkiIddswojd%2BPrNeqfOa1JG0%3D "

// Optional for test. For production, paste raw SHA-256 only. Do not include "sha256:" prefix.
// Empty string means: skip SHA verification.
#define MANUAL_OTA_SHA256 ""

// Optional for test. 0 means: use HTTP Content-Length and skip exact size validation.
#define MANUAL_OTA_FILE_SIZE_BYTES 0UL

// MQTT X.509 PEMs are loaded from provisioning API and LittleFS.
// No MQTT certificate/private key is embedded in firmware.

// GLOBALS
     

Preferences prefs;

WiFiClientSecure mqttSecureClient;
PubSubClient mqttClient(mqttSecureClient);

String deviceUuid;
String mqttHost;
int mqttPort = 8883;
String mqttClientId;
String mqttBaseTopic;
String runtimeSerialNumber;

String mqttRootCaPem;
String mqttClientCertPem;
String mqttPrivateKeyPem;

const char *MQTT_CA_FILE          = "/mqtt_ca.pem";
const char *MQTT_CLIENT_CERT_FILE = "/mqtt_client_cert.pem";
const char *MQTT_PRIVATE_KEY_FILE = "/mqtt_private_key.pem";

String topicOnline;
String topicOffline;
String topicTelemetry;

String otaCmdTopic;
String otaStatusTopic;

uint32_t lastTelemetryMs = 0;
uint32_t seqNo = 1;

bool otaInProgress = false;
bool manualOtaTestStarted = false;

     
// BASIC HELPERS

String getMacAddress() {
  return WiFi.macAddress();
}

String chipIdString() {
  uint64_t chipid = ESP.getEfuseMac();
  char id[17];
  snprintf(id, sizeof(id), "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  return String(id);
}

String macHexNoColonFromEfuse() {
  uint64_t mac = ESP.getEfuseMac();

  // ESP.getEfuseMac() order is reverse of normal WiFi MAC display.
  uint8_t b0 = mac & 0xFF;
  uint8_t b1 = (mac >> 8) & 0xFF;
  uint8_t b2 = (mac >> 16) & 0xFF;
  uint8_t b3 = (mac >> 24) & 0xFF;
  uint8_t b4 = (mac >> 32) & 0xFF;
  uint8_t b5 = (mac >> 40) & 0xFF;

  char buf[13];
  snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X", b0, b1, b2, b3, b4, b5);
  return String(buf);
}

String getDeviceSerialNumber() {
  // Hardcoded serial for current test/device lifecycle.
  // Do not derive from MAC here, otherwise backend/NVS identity changes per board.
  return String(DEVICE_SERIAL_NUMBER);
}

String lowerCaseCopy(String s) {
  s.toLowerCase();
  return s;
}

String sanitizeTopicSerial(const String &s) {
  String out = s;
  out.toLowerCase();
  out.replace("_", "-");
  return out;
}

String defaultBaseTopic() {
  return String("petwatch/") + PRODUCT_CODE + "/" + MODEL_CODE + "/" + sanitizeTopicSerial(getDeviceSerialNumber());
}

void printLine(char c = '=', int count = 60) {
  for (int i = 0; i < count; i++) {
    Serial.print(c);
  }
  Serial.println();
}

void buildTopics() {
  mqttBaseTopic = lowerCaseCopy(mqttBaseTopic);

  topicOnline    = mqttBaseTopic + "/status/online";
  topicOffline   = mqttBaseTopic + "/status/offline";
  topicTelemetry = mqttBaseTopic + "/telemetry";

  otaCmdTopic    = mqttBaseTopic + "/ota/cmd";
  otaStatusTopic = mqttBaseTopic + "/ota/status";
}

bool hasValidMqttIdentity() {
  return (
    deviceUuid.length() > 0 &&
    mqttHost.length() > 0 &&
    mqttPort > 0 &&
    mqttClientId.length() > 0 &&
    mqttBaseTopic.length() > 0
  );
}

void printCurrentConfig() {
  Serial.println();
  printLine();
  Serial.println("CURRENT CONFIG");
  printLine('-');

  Serial.print("Serial No    : "); Serial.println(getDeviceSerialNumber());
  Serial.print("Product Code : "); Serial.println(PRODUCT_CODE);
  Serial.print("Model Code   : "); Serial.println(MODEL_CODE);
  Serial.print("FW Version   : "); Serial.println(FW_VERSION);
  Serial.print("HW Version   : "); Serial.println(HW_VERSION);
  Serial.print("MAC Address  : "); Serial.println(getMacAddress());
  Serial.print("Chip ID      : "); Serial.println(chipIdString());

  Serial.print("Device UUID  : "); Serial.println(deviceUuid);
  Serial.print("MQTT Host    : "); Serial.println(mqttHost);
  Serial.print("MQTT Port    : "); Serial.println(mqttPort);
  Serial.print("MQTT Client  : "); Serial.println(mqttClientId);
  Serial.print("MQTT Base    : "); Serial.println(mqttBaseTopic);

  Serial.println("MQTT Auth    : X.509 certificate from API/LittleFS");
  Serial.print("MQTT PEMs    : "); Serial.println(hasValidPemMaterial() ? "LOADED" : "MISSING");
  Serial.println("MQTT User    : NOT USED");
  Serial.println("MQTT Pass    : NOT USED");

  Serial.print("Online Topic : "); Serial.println(topicOnline);
  Serial.print("Telemetry    : "); Serial.println(topicTelemetry);
  Serial.print("OTA CMD      : "); Serial.println(otaCmdTopic);
  Serial.print("OTA Status   : "); Serial.println(otaStatusTopic);

  printLine();
}

     
// TIME SYNC
     

void syncTimeForTls() {
  Serial.println();
  Serial.println("[TIME] Syncing time for TLS...");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);
  int retry = 0;

  while (now < 1700000000 && retry < 30) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    retry++;
  }

  Serial.println();

  if (now < 1700000000) {
    Serial.println("[TIME] WARNING: Time sync failed. MQTT TLS may fail.");
  } else {
    Serial.print("[TIME] OK: ");
    Serial.println(ctime(&now));
  }
}

     

// PEM / LITTLEFS STORAGE

String trimCopy(String s) {
  s.trim();
  return s;
}

String normalizePem(String pem) {
  pem.trim();

  // If backend accidentally sends literal "\\n" instead of JSON escaped newlines,
  // convert them into real PEM line breaks.
  pem.replace("\\n", "\n");
  pem.replace("\\r", "");

  return pem;
}

bool isValidCertificatePem(const String &pem) {
  return pem.indexOf("-----BEGIN CERTIFICATE-----") >= 0 &&
         pem.indexOf("-----END CERTIFICATE-----") >= 0;
}

bool isValidPrivateKeyPem(const String &pem) {
  return (
    (pem.indexOf("-----BEGIN PRIVATE KEY-----") >= 0 && pem.indexOf("-----END PRIVATE KEY-----") >= 0) ||
    (pem.indexOf("-----BEGIN RSA PRIVATE KEY-----") >= 0 && pem.indexOf("-----END RSA PRIVATE KEY-----") >= 0) ||
    (pem.indexOf("-----BEGIN EC PRIVATE KEY-----") >= 0 && pem.indexOf("-----END EC PRIVATE KEY-----") >= 0)
  );
}

bool hasValidPemMaterial() {
  return isValidCertificatePem(mqttRootCaPem) &&
         isValidCertificatePem(mqttClientCertPem) &&
         isValidPrivateKeyPem(mqttPrivateKeyPem);
}

bool writeTextFile(const char *path, const String &content) {
  File f = LittleFS.open(path, "w");
  if (!f) {
    Serial.print("[FS] Failed to open for write: ");
    Serial.println(path);
    return false;
  }

  size_t written = f.print(content);
  f.close();

  if (written != content.length()) {
    Serial.print("[FS] Short write: ");
    Serial.println(path);
    return false;
  }

  Serial.print("[FS] Saved: ");
  Serial.print(path);
  Serial.print(" bytes=");
  Serial.println(written);

  return true;
}

String readTextFile(const char *path) {
  if (!LittleFS.exists(path)) {
    return "";
  }

  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.print("[FS] Failed to open for read: ");
    Serial.println(path);
    return "";
  }

  String content;
  content.reserve(f.size() + 1);

  while (f.available()) {
    content += (char)f.read();
  }

  f.close();
  return normalizePem(content);
}

void deleteTextFile(const char *path) {
  if (LittleFS.exists(path)) {
    LittleFS.remove(path);
    Serial.print("[FS] Deleted: ");
    Serial.println(path);
  }
}

bool saveMqttPemToFs() {
  mqttRootCaPem     = normalizePem(mqttRootCaPem);
  mqttClientCertPem = normalizePem(mqttClientCertPem);
  mqttPrivateKeyPem = normalizePem(mqttPrivateKeyPem);

  if (!hasValidPemMaterial()) {
    Serial.println("[FS] Refusing to save invalid/incomplete MQTT PEM material");
    Serial.print("[FS] CA valid          : "); Serial.println(isValidCertificatePem(mqttRootCaPem) ? "yes" : "no");
    Serial.print("[FS] Client cert valid : "); Serial.println(isValidCertificatePem(mqttClientCertPem) ? "yes" : "no");
    Serial.print("[FS] Private key valid : "); Serial.println(isValidPrivateKeyPem(mqttPrivateKeyPem) ? "yes" : "no");
    return false;
  }

  bool ok = true;
  ok &= writeTextFile(MQTT_CA_FILE, mqttRootCaPem);
  ok &= writeTextFile(MQTT_CLIENT_CERT_FILE, mqttClientCertPem);
  ok &= writeTextFile(MQTT_PRIVATE_KEY_FILE, mqttPrivateKeyPem);

  Serial.println(ok ? "[FS] MQTT PEM material saved" : "[FS] MQTT PEM material save failed");
  return ok;
}

bool loadMqttPemFromFs() {
  mqttRootCaPem     = readTextFile(MQTT_CA_FILE);
  mqttClientCertPem = readTextFile(MQTT_CLIENT_CERT_FILE);
  mqttPrivateKeyPem = readTextFile(MQTT_PRIVATE_KEY_FILE);

  if (!hasValidPemMaterial()) {
    Serial.println("[FS] MQTT PEM material missing/incomplete");
    Serial.print("[FS] CA valid          : "); Serial.println(isValidCertificatePem(mqttRootCaPem) ? "yes" : "no");
    Serial.print("[FS] Client cert valid : "); Serial.println(isValidCertificatePem(mqttClientCertPem) ? "yes" : "no");
    Serial.print("[FS] Private key valid : "); Serial.println(isValidPrivateKeyPem(mqttPrivateKeyPem) ? "yes" : "no");
    return false;
  }

  Serial.println("[FS] MQTT PEM material loaded");
  return true;
}

void clearMqttPemFiles() {
  deleteTextFile(MQTT_CA_FILE);
  deleteTextFile(MQTT_CLIENT_CERT_FILE);
  deleteTextFile(MQTT_PRIVATE_KEY_FILE);

  mqttRootCaPem = "";
  mqttClientCertPem = "";
  mqttPrivateKeyPem = "";
}

// NVS
     

void clearNvs() {
  prefs.begin("onboard", false);

  prefs.remove("provisioned");
  prefs.remove("serial_number");
  prefs.remove("device_uuid");
  prefs.remove("mqtt_host");
  prefs.remove("mqtt_port");
  prefs.remove("mqtt_client_id");
  prefs.remove("mqtt_base_topic");

  prefs.end();

  clearMqttPemFiles();

  Serial.println("[NVS] Cleared");
}

bool saveMqttIdentityToNvs() {
  if (!hasValidMqttIdentity()) {
    Serial.println("[NVS] Refusing to save invalid MQTT identity");
    return false;
  }

  prefs.begin("onboard", false);

  bool ok = true;

  ok &= prefs.putString("serial_number", getDeviceSerialNumber()) > 0;
  ok &= prefs.putString("device_uuid", deviceUuid) > 0;
  ok &= prefs.putString("mqtt_host", mqttHost) > 0;
  ok &= prefs.putInt("mqtt_port", mqttPort) > 0;
  ok &= prefs.putString("mqtt_client_id", mqttClientId) > 0;
  ok &= prefs.putString("mqtt_base_topic", mqttBaseTopic) > 0;

  if (ok) {
    prefs.putBool("provisioned", true);
  } else {
    prefs.putBool("provisioned", false);
  }

  prefs.end();

  Serial.println(ok ? "[NVS] MQTT identity saved" : "[NVS] MQTT identity save failed");

  return ok;
}

bool loadMqttIdentityFromNvs() {
  prefs.begin("onboard", true);

  bool provisioned = prefs.getBool("provisioned", false);
  String storedSerialNumber = prefs.getString("serial_number", "");

  deviceUuid    = prefs.getString("device_uuid", "");
  mqttHost      = prefs.getString("mqtt_host", "");
  mqttPort      = prefs.getInt("mqtt_port", 8883);
  mqttClientId  = prefs.getString("mqtt_client_id", "");
  mqttBaseTopic = prefs.getString("mqtt_base_topic", "");

  prefs.end();

  if (!provisioned) {
    Serial.println("[NVS] provisioned=false");
    return false;
  }

  if (storedSerialNumber != getDeviceSerialNumber()) {
    Serial.println("[NVS] Stored MQTT identity belongs to old/different serial. Ignoring NVS.");
    Serial.print("[NVS] Stored serial : "); Serial.println(storedSerialNumber);
    Serial.print("[NVS] Current serial: "); Serial.println(getDeviceSerialNumber());
    return false;
  }

  if (!hasValidMqttIdentity()) {
    Serial.println("[NVS] Stored MQTT identity incomplete/corrupted");
    return false;
  }

  if (FORCE_MQTT_X509_PORT_8883 && mqttPort != 8883) {
    Serial.print("[NVS] MQTT port from NVS was ");
    Serial.print(mqttPort);
    Serial.println(". Overriding to 8883 for X.509.");
    mqttPort = 8883;
  }

  if (!loadMqttPemFromFs()) {
    Serial.println("[NVS] MQTT identity exists but PEM material is missing. Ignoring stored identity.");
    return false;
  }

  buildTopics();

  Serial.println("[NVS] MQTT identity and PEM material loaded");
  return true;
}

     
// WIFI
     

bool connectWiFi() {
  Serial.println();
  printLine();
  Serial.println("[WiFi] Connecting");
  printLine('-');

  Serial.print("[WiFi] SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t startMs = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startMs < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Connected");
    Serial.print("[WiFi] IP      : ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] RSSI    : ");
    Serial.println(WiFi.RSSI());
    Serial.print("[WiFi] MAC     : ");
    Serial.println(getMacAddress());
    Serial.print("[WiFi] Gateway : ");
    Serial.println(WiFi.gatewayIP());
    return true;
  }

  Serial.println("[WiFi] Failed");
  return false;
}

     
// HTTPS PROVISIONING API
//
// IMPORTANT:
// This is normal HTTPS only.
// MQTT X.509 PEM files are NOT used here.
     

String firstNonEmpty(String a, String b) {
  a.trim();
  b.trim();
  return a.length() > 0 ? a : b;
}

bool parseProvisionResponse(const String& responseBody) {
  // PEM strings are large. Keep this document size large enough for:
  // root CA + client certificate + private key + normal provisioning data.
  DynamicJsonDocument doc(32768);

  DeserializationError err = deserializeJson(doc, responseBody);

  if (err) {
    Serial.print("[API] JSON parse failed: ");
    Serial.println(err.c_str());
    return false;
  }

  String status    = doc["status"] | "";
  String errorCode = doc["error_code"] | "";
  String message   = doc["message"] | "";

  Serial.print("[API] status     : "); Serial.println(status);
  Serial.print("[API] error_code : "); Serial.println(errorCode);
  Serial.print("[API] message    : "); Serial.println(message);

  // Accept success, and also ALREADY_PROVISIONED only if the response includes
  // full MQTT identity + PEM material. Without PEMs, this firmware cannot connect.
  bool usableStatus = (status == "success") || (errorCode == "ALREADY_PROVISIONED");

  if (!usableStatus) {
    Serial.println("[API] Provisioning failed");
    return false;
  }

  String apiDeviceUuid = doc["device"]["device_uuid"] | "";

  String apiHost = doc["mqtt"]["broker_host"] | "";
  if (apiHost.length() == 0) apiHost = doc["mqtt"]["mqtt_host"] | "";
  if (apiHost.length() == 0) apiHost = doc["mqtt"]["host"] | "";

  int apiPort = doc["mqtt"]["broker_port"] | 8883;
  if (doc["mqtt"]["mqtt_port"].is<int>()) apiPort = doc["mqtt"]["mqtt_port"] | 8883;
  if (doc["mqtt"]["port"].is<int>()) apiPort = doc["mqtt"]["port"] | 8883;

  String apiClientId = doc["mqtt"]["mqtt_client_id"] | "";
  if (apiClientId.length() == 0) apiClientId = doc["mqtt"]["client_id"] | "";

  String apiBaseTopic = doc["mqtt"]["mqtt_base_topic"] | "";
  if (apiBaseTopic.length() == 0) apiBaseTopic = doc["mqtt"]["base_topic"] | "";

  // Supported certificate field names. Backend can use any one of these sets.
  String apiRootCa = doc["mqtt"]["ca_pem"] | "";
  if (apiRootCa.length() == 0) apiRootCa = doc["mqtt"]["root_ca_pem"] | "";
  if (apiRootCa.length() == 0) apiRootCa = doc["mqtt"]["mqtt_root_ca_pem"] | "";
  if (apiRootCa.length() == 0) apiRootCa = doc["certificates"]["ca_pem"] | "";
  if (apiRootCa.length() == 0) apiRootCa = doc["certificates"]["root_ca_pem"] | "";
  if (apiRootCa.length() == 0) apiRootCa = doc["certificates"]["mqtt_root_ca_pem"] | "";
  // Backend currently returns final PEMs under credentials.*
  if (apiRootCa.length() == 0) apiRootCa = doc["credentials"]["mqtt_root_ca_pem"] | "";
  if (apiRootCa.length() == 0) apiRootCa = doc["credentials"]["root_ca_pem"] | "";
  if (apiRootCa.length() == 0) apiRootCa = doc["credentials"]["ca_pem"] | "";

  String apiClientCert = doc["mqtt"]["client_cert_pem"] | "";
  if (apiClientCert.length() == 0) apiClientCert = doc["mqtt"]["certificate_pem"] | "";
  if (apiClientCert.length() == 0) apiClientCert = doc["mqtt"]["mqtt_client_cert_pem"] | "";
  if (apiClientCert.length() == 0) apiClientCert = doc["certificates"]["client_cert_pem"] | "";
  if (apiClientCert.length() == 0) apiClientCert = doc["certificates"]["certificate_pem"] | "";
  if (apiClientCert.length() == 0) apiClientCert = doc["certificates"]["mqtt_client_cert_pem"] | "";
  // Backend currently returns this as credentials.device_client_certificate_pem
  if (apiClientCert.length() == 0) apiClientCert = doc["credentials"]["device_client_certificate_pem"] | "";
  if (apiClientCert.length() == 0) apiClientCert = doc["credentials"]["client_cert_pem"] | "";
  if (apiClientCert.length() == 0) apiClientCert = doc["credentials"]["certificate_pem"] | "";

  String apiPrivateKey = doc["mqtt"]["client_key_pem"] | "";
  if (apiPrivateKey.length() == 0) apiPrivateKey = doc["mqtt"]["private_key_pem"] | "";
  if (apiPrivateKey.length() == 0) apiPrivateKey = doc["mqtt"]["mqtt_private_key_pem"] | "";
  if (apiPrivateKey.length() == 0) apiPrivateKey = doc["certificates"]["client_key_pem"] | "";
  if (apiPrivateKey.length() == 0) apiPrivateKey = doc["certificates"]["private_key_pem"] | "";
  if (apiPrivateKey.length() == 0) apiPrivateKey = doc["certificates"]["mqtt_private_key_pem"] | "";
  // Backend currently returns this as credentials.device_private_key_pem
  if (apiPrivateKey.length() == 0) apiPrivateKey = doc["credentials"]["device_private_key_pem"] | "";
  if (apiPrivateKey.length() == 0) apiPrivateKey = doc["credentials"]["client_key_pem"] | "";
  if (apiPrivateKey.length() == 0) apiPrivateKey = doc["credentials"]["private_key_pem"] | "";

  apiDeviceUuid.trim();
  apiHost.trim();
  apiClientId.trim();
  apiBaseTopic.trim();

  apiRootCa     = normalizePem(apiRootCa);
  apiClientCert = normalizePem(apiClientCert);
  apiPrivateKey = normalizePem(apiPrivateKey);

  if (apiDeviceUuid.length() == 0) {
    Serial.println("[API] Missing device.device_uuid");
    return false;
  }

  if (apiHost.length() == 0 || apiPort <= 0 || apiClientId.length() == 0 || apiBaseTopic.length() == 0) {
    Serial.println("[API] Missing required MQTT identity fields");
    Serial.print("[API] broker_host    : "); Serial.println(apiHost);
    Serial.print("[API] broker_port    : "); Serial.println(apiPort);
    Serial.print("[API] mqtt_client_id : "); Serial.println(apiClientId);
    Serial.print("[API] mqtt_base_topic: "); Serial.println(apiBaseTopic);
    return false;
  }

  mqttRootCaPem     = apiRootCa;
  mqttClientCertPem = apiClientCert;
  mqttPrivateKeyPem = apiPrivateKey;

  if (!hasValidPemMaterial()) {
    Serial.println("[API] Missing/invalid MQTT PEM material from provisioning API");
    Serial.print("[API] CA valid          : "); Serial.println(isValidCertificatePem(mqttRootCaPem) ? "yes" : "no");
    Serial.print("[API] Client cert valid : "); Serial.println(isValidCertificatePem(mqttClientCertPem) ? "yes" : "no");
    Serial.print("[API] Private key valid : "); Serial.println(isValidPrivateKeyPem(mqttPrivateKeyPem) ? "yes" : "no");
    return false;
  }

  deviceUuid    = apiDeviceUuid;
  mqttHost      = apiHost;
  mqttPort      = apiPort;
  mqttClientId  = apiClientId;
  mqttBaseTopic = apiBaseTopic;

  if (FORCE_MQTT_X509_PORT_8883 && mqttPort != 8883) {
    Serial.print("[API] MQTT port from API was ");
    Serial.print(mqttPort);
    Serial.println(". Overriding to 8883 for X.509.");
    mqttPort = 8883;
  }

  buildTopics();

  if (!saveMqttPemToFs()) {
    Serial.println("[API] Failed to save MQTT PEM material");
    return false;
  }

  if (!saveMqttIdentityToNvs()) {
    Serial.println("[API] Failed to save MQTT identity");
    return false;
  }

  Serial.println("[API] MQTT identity and PEM material received/saved from backend");
  return true;
}

bool callProvisioningApi() {
  Serial.println();
  printLine();
  Serial.println("HTTPS PROVISIONING API");
  printLine('-');

  String url = String(BACKEND_BASE_URL) + String(PROVISION_API_PATH);

  StaticJsonDocument<640> req;

  // Backend requires mac_address.
  // Keep the certificate serial, but also send the physical MAC and device metadata.
  req["serial_number"]  = getDeviceSerialNumber();
  req["product_code"]   = PRODUCT_CODE;
  req["model_code"]     = MODEL_CODE;
  req["target_version"] = MANUAL_OTA_TARGET_VERSION;
  req["mac_address"]    = getMacAddress();
  req["fw_version"]     = FW_VERSION;
  req["hw_version"]     = HW_VERSION;
  req["chip_id"]        = chipIdString();

  String requestBody;
  serializeJson(req, requestBody);

  Serial.print("[API] POST ");
  Serial.println(url);
  Serial.print("[API] Request: ");
  Serial.println(requestBody);

  WiFiClientSecure httpsClient;

  // Phase-1 /provision-test is normal HTTPS.
  // Old code worked with setInsecure(), so keep same behavior here.
  httpsClient.setInsecure();
  httpsClient.setTimeout(15);

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);

  if (!http.begin(httpsClient, url)) {
    Serial.println("[API] http.begin failed");
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(requestBody);
  String responseBody = http.getString();

  http.end();

  Serial.print("[API] HTTP Code: ");
  Serial.println(httpCode);
  if (responseBody.indexOf("PRIVATE KEY") >= 0 || responseBody.indexOf("BEGIN CERTIFICATE") >= 0) {
    Serial.print("[API] Response contains PEM credentials. Length: ");
    Serial.println(responseBody.length());
    Serial.println("[API] Response body hidden for security. Parser will validate PEM fields.");
  } else {
    Serial.print("[API] Response: ");
    Serial.println(responseBody);
  }

  if (httpCode <= 0) {
    Serial.println("[API] HTTPS transport failed");
    return false;
  }

  if (httpCode < 200 || httpCode >= 300) {
    Serial.println("[API] Non-2xx response. Trying to parse body.");
  }

  return parseProvisionResponse(responseBody);
}

     
// MQTT DIAGNOSTIC
     

bool testTcpPort(const String& host, int port) {
  WiFiClient tcpTest;
  tcpTest.setTimeout(8);

  Serial.print("[TCP] Testing ");
  Serial.print(host);
  Serial.print(":");
  Serial.println(port);

  bool ok = tcpTest.connect(host.c_str(), port);

  if (ok) {
    Serial.println("[TCP] SUCCESS");
    tcpTest.stop();
    return true;
  }

  Serial.println("[TCP] FAILED");
  return false;
}

bool testMqttTlsHandshake() {
  WiFiClientSecure tlsTest;

  if (!hasValidPemMaterial()) {
    Serial.println("[TLS] Missing MQTT PEM material. Cannot test X.509 handshake.");
    return false;
  }

  tlsTest.setCACert(mqttRootCaPem.c_str());
  tlsTest.setCertificate(mqttClientCertPem.c_str());
  tlsTest.setPrivateKey(mqttPrivateKeyPem.c_str());
  tlsTest.setTimeout(10);

  Serial.print("[TLS] Testing MQTT X.509 handshake ");
  Serial.print(mqttHost);
  Serial.print(":");
  Serial.println(mqttPort);

  bool ok = tlsTest.connect(mqttHost.c_str(), mqttPort);

  if (ok) {
    Serial.println("[TLS] SUCCESS");
    tlsTest.stop();
    return true;
  }

  Serial.println("[TLS] FAILED");
  return false;
}

void diagnoseMqttNetwork() {
  Serial.println();
  printLine();
  Serial.println("MQTT DIAGNOSTIC");
  printLine('-');

  IPAddress brokerIp;

  if (WiFi.hostByName(mqttHost.c_str(), brokerIp)) {
    Serial.print("[DNS] Resolved IP: ");
    Serial.println(brokerIp);
  } else {
    Serial.println("[DNS] FAILED");
  }

  testTcpPort(mqttHost, mqttPort);
  testMqttTlsHandshake();

  printLine();
}

     
// MQTT PUBLISH HELPERS
     

bool publishJson(const String& topic, const String& payload, bool retained = false) {
  if (!mqttClient.connected()) return false;

  bool ok = mqttClient.publish(topic.c_str(), payload.c_str(), retained);

  Serial.print("[MQTT-PUB] ");
  Serial.print(ok ? "OK   " : "FAIL ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(payload);

  return ok;
}

bool publishJsonDoc(const String& topic, const JsonDocument& doc, bool retained = false) {
  String payload;
  serializeJson(doc, payload);
  return publishJson(topic, payload, retained);
}

String makeOfflinePayload(const char* reason) {
  StaticJsonDocument<512> doc;

  doc["msg_type"]          = "status";
  doc["device_uuid"]       = deviceUuid;
  doc["serial_number"]     = getDeviceSerialNumber();
  doc["product_code"]      = PRODUCT_CODE;
  doc["model_code"]        = MODEL_CODE;
  doc["connection_status"] = "offline";
  doc["reason"]            = reason;

  String payload;
  serializeJson(doc, payload);

  return payload;
}

void publishOnlineStatus() {
  StaticJsonDocument<768> doc;

  doc["msg_type"]           = "status";
  doc["device_uuid"]        = deviceUuid;
  doc["serial_number"]      = getDeviceSerialNumber();
  doc["product_code"]       = PRODUCT_CODE;
  doc["model_code"]         = MODEL_CODE;
  doc["connection_status"]  = "online";
  doc["fw_version"]         = FW_VERSION;
  doc["hw_version"]         = HW_VERSION;
  doc["mac_address"]        = getMacAddress();
  doc["chip_id"]            = chipIdString();
  doc["ip"]                 = WiFi.localIP().toString();
  doc["rssi"]               = WiFi.RSSI();
  doc["sleep_status"]       = "awake";
  doc["ota_status"]         = otaInProgress ? "in_progress" : "idle";
  doc["calibration_status"] = "idle";
  doc["uptime_ms"]          = millis();

  publishJsonDoc(topicOnline, doc, true);
}

void publishOfflineStatus(const char* reason) {
  if (!mqttClient.connected()) return;

  String payload = makeOfflinePayload(reason);
  publishJson(topicOffline, payload, false);

  delay(500);
}

String makeTelemetryJson() {
  StaticJsonDocument<768> doc;

  int weight = random(20, 250);
  int battery = random(75, 101);

  doc["msg_type"]              = "telemetry";
  doc["device_uuid"]           = deviceUuid;
  doc["serial_number"]         = getDeviceSerialNumber();
  doc["product_code"]          = PRODUCT_CODE;
  doc["model_code"]            = MODEL_CODE;
  doc["fw_version"]            = FW_VERSION;
  doc["hw_version"]            = HW_VERSION;
  doc["mac_address"]           = getMacAddress();

  doc["seq"]                   = seqNo++;
  doc["uptime_ms"]             = millis();

  doc["connection_status"]     = "online";
  doc["sleep_status"]          = "awake";
  doc["ota_status"]            = otaInProgress ? "in_progress" : "idle";
  doc["calibration_status"]    = "idle";

  doc["battery_level"]         = battery;
  doc["battery_status"]        = battery > 85 ? "good" : "medium";
  doc["rssi"]                  = WiFi.RSSI();
  doc["heap_free"]             = ESP.getFreeHeap();

  doc["actual_weight"]         = weight;
  doc["tracked_weight"]        = weight;
  doc["planned_portion_size"]  = 150;
  doc["weight_unit"]           = "grams";

  doc["stability_status"]      = "stable";
  doc["bowl_status"]           = weight < 60 ? "low_food" : "food_present";

  doc["camera_available"]      = true;
  doc["image_upload_status"]   = "not_triggered";

  String payload;
  serializeJson(doc, payload);

  return payload;
}

void publishTelemetry() {
  if (!mqttClient.connected()) return;

  String payload = makeTelemetryJson();
  publishJson(topicTelemetry, payload, false);
}

     
// OTA SHA-256 HELPERS
     

String bytesToHex(const unsigned char *buf, size_t len) {
  const char *hex = "0123456789abcdef";
  String out;
  out.reserve(len * 2);

  for (size_t i = 0; i < len; i++) {
    out += hex[(buf[i] >> 4) & 0x0F];
    out += hex[buf[i] & 0x0F];
  }

  return out;
}

void publishOtaStatus(const char *status, const char *message = nullptr, int progress = -1) {
  if (otaStatusTopic.length() == 0) return;
  if (!mqttClient.connected()) return;

  StaticJsonDocument<512> doc;

  doc["msg_type"]      = "ota_status";
  doc["device_uuid"]   = deviceUuid;
  doc["serial_number"] = getDeviceSerialNumber();
  doc["fw_version"]    = FW_VERSION;
  doc["status"]        = status;
  doc["uptime_ms"]     = millis();

  if (message != nullptr) {
    doc["message"] = message;
  }

  if (progress >= 0) {
    doc["progress"] = progress;
  }

  publishJsonDoc(otaStatusTopic, doc, false);
}

bool performHttpOta(
  const String &downloadUrl,
  const String &expectedSha256,
  size_t expectedSize,
  const String &targetVersion
) {
  if (otaInProgress) {
    Serial.println("[OTA] OTA already in progress");
    return false;
  }

  String otaUrl = downloadUrl;
  String otaSha256 = expectedSha256;
  String otaTargetVersion = targetVersion;

  otaUrl.trim();
  otaSha256.trim();
  otaTargetVersion.trim();

  if (otaSha256 == "PASTE_SHA256_HERE") {
    otaSha256 = "";
  }

  otaInProgress = true;

  Serial.println();
  printLine();
  Serial.println("OTA START");
  printLine('-');

  Serial.print("[OTA] URL             : "); Serial.println(otaUrl);
  Serial.print("[OTA] Target version  : "); Serial.println(otaTargetVersion);
  Serial.print("[OTA] Current version : "); Serial.println(FW_VERSION);

  if (otaSha256.length() > 0) {
    Serial.print("[OTA] Expected SHA256 : "); Serial.println(otaSha256);
  } else {
    Serial.println("[OTA] Expected SHA256 : NOT PROVIDED - hash verification skipped");
  }

  if (expectedSize > 0) {
    Serial.print("[OTA] Expected size   : "); Serial.println(expectedSize);
  } else {
    Serial.println("[OTA] Expected size   : NOT PROVIDED - exact size check skipped");
  }

  publishOtaStatus("pending", "OTA command accepted", 0);

  bool isHttp = otaUrl.startsWith("http://");
  bool isHttps = otaUrl.startsWith("https://");

  if (!isHttp && !isHttps) {
    Serial.println("[OTA] Invalid URL. Only http:// or https:// allowed");
    publishOtaStatus("failed", "Invalid URL. Only http:// or https:// allowed");
    otaInProgress = false;
    return false;
  }

  if (isHttp && !OTA_ALLOW_HTTP) {
    Serial.println("[OTA] HTTP OTA disabled");
    publishOtaStatus("failed", "HTTP OTA disabled");
    otaInProgress = false;
    return false;
  }

  if (isHttps && !OTA_ALLOW_HTTPS) {
    Serial.println("[OTA] HTTPS OTA disabled");
    publishOtaStatus("failed", "HTTPS OTA disabled");
    otaInProgress = false;
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[OTA] WiFi disconnected");
    publishOtaStatus("failed", "WiFi disconnected");
    otaInProgress = false;
    return false;
  }

  HTTPClient http;
  WiFiClient plainOtaClient;
  WiFiClientSecure secureOtaClient;

  http.setTimeout(OTA_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  bool beginOk = false;

  if (isHttps) {
    // For production, replace setInsecure() with firmware storage server CA.
    // Firmware integrity is still verified by SHA-256 below.
    secureOtaClient.setInsecure();
    secureOtaClient.setTimeout(20);
    beginOk = http.begin(secureOtaClient, otaUrl);
  } else {
    beginOk = http.begin(plainOtaClient, otaUrl);
  }

  if (!beginOk) {
    Serial.println("[OTA] http.begin failed");
    publishOtaStatus("failed", "http.begin failed");
    otaInProgress = false;
    return false;
  }

  int httpCode = http.GET();

  Serial.print("[OTA] HTTP Code: ");
  Serial.println(httpCode);

  if (httpCode != HTTP_CODE_OK) {
    String msg = String("Firmware HTTP GET failed code=") + httpCode;
    Serial.println("[OTA] " + msg);
    publishOtaStatus("failed", msg.c_str());
    http.end();
    otaInProgress = false;
    return false;
  }

  int contentLength = http.getSize();

  Serial.print("[OTA] Content length: ");
  Serial.println(contentLength);

  if (expectedSize > 0 && contentLength > 0 && (size_t)contentLength != expectedSize) {
    Serial.println("[OTA] Warning: HTTP content length differs from expected file_size_bytes");
  }

  if (!Update.begin(contentLength > 0 ? contentLength : UPDATE_SIZE_UNKNOWN)) {
    Serial.print("[OTA] Update.begin failed: ");
    Serial.println(Update.errorString());
    publishOtaStatus("failed", Update.errorString());
    http.end();
    otaInProgress = false;
    return false;
  }

  mbedtls_md_context_t shaCtx;
  const mbedtls_md_info_t *shaInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

  mbedtls_md_init(&shaCtx);

  if (mbedtls_md_setup(&shaCtx, shaInfo, 0) != 0) {
    Serial.println("[OTA] SHA256 setup failed");
    publishOtaStatus("failed", "SHA256 setup failed");

    Update.abort();
    mbedtls_md_free(&shaCtx);
    http.end();
    otaInProgress = false;
    return false;
  }

  mbedtls_md_starts(&shaCtx);

  publishOtaStatus("downloading", "Downloading firmware", 1);

  WiFiClient *stream = http.getStreamPtr();

  uint8_t buffer[4096];
  size_t totalWritten = 0;
  int lastProgress = -1;
  unsigned long lastDataMs = millis();

  while (http.connected() && (contentLength < 0 || totalWritten < (size_t)contentLength)) {
    size_t available = stream->available();

    if (available > 0) {
      size_t toRead = available;

      if (toRead > sizeof(buffer)) {
        toRead = sizeof(buffer);
      }

      int readLen = stream->readBytes(buffer, toRead);

      if (readLen <= 0) {
        continue;
      }

      mbedtls_md_update(&shaCtx, buffer, readLen);

      size_t written = Update.write(buffer, readLen);

      if (written != (size_t)readLen) {
        Serial.print("[OTA] Update.write failed: ");
        Serial.println(Update.errorString());

        publishOtaStatus("failed", Update.errorString());

        Update.abort();
        mbedtls_md_free(&shaCtx);
        http.end();
        otaInProgress = false;
        return false;
      }

      totalWritten += written;
      lastDataMs = millis();

      int progress = 0;

      if (contentLength > 0) {
        progress = (int)((totalWritten * 100UL) / (size_t)contentLength);
      } else if (expectedSize > 0) {
        progress = (int)((totalWritten * 100UL) / expectedSize);
      }

      if (progress > 100) {
        progress = 100;
      }

      if (progress != lastProgress && progress % 10 == 0) {
        lastProgress = progress;

        Serial.print("[OTA] Progress: ");
        Serial.print(progress);
        Serial.println("%");

        publishOtaStatus("downloading", "Downloading firmware", progress);
      }
    } else {
      delay(1);

      if (millis() - lastDataMs > OTA_HTTP_TIMEOUT_MS) {
        Serial.println("[OTA] Download timeout");

        publishOtaStatus("failed", "Download timeout");

        Update.abort();
        mbedtls_md_free(&shaCtx);
        http.end();
        otaInProgress = false;
        return false;
      }
    }
  }

  unsigned char shaOut[32];
  mbedtls_md_finish(&shaCtx, shaOut);
  mbedtls_md_free(&shaCtx);

  String calculatedSha256 = bytesToHex(shaOut, 32);

  Serial.print("[OTA] Total written     : ");
  Serial.println(totalWritten);

  Serial.print("[OTA] Calculated SHA256 : ");
  Serial.println(calculatedSha256);

  if (otaSha256.length() > 0 && !calculatedSha256.equalsIgnoreCase(otaSha256)) {
    Serial.println("[OTA] SHA-256 mismatch");

    publishOtaStatus("failed", "SHA-256 mismatch");

    Update.abort();
    http.end();
    otaInProgress = false;
    return false;
  }

  if (expectedSize > 0 && totalWritten != expectedSize) {
    Serial.println("[OTA] File size mismatch");

    publishOtaStatus("failed", "File size mismatch");

    Update.abort();
    http.end();
    otaInProgress = false;
    return false;
  }

  publishOtaStatus("installing", "Installing firmware", 100);

  if (!Update.end(true)) {
    Serial.print("[OTA] Update.end failed: ");
    Serial.println(Update.errorString());

    publishOtaStatus("failed", Update.errorString());

    http.end();
    otaInProgress = false;
    return false;
  }

  if (!Update.isFinished()) {
    Serial.println("[OTA] Update not finished");

    publishOtaStatus("failed", "Update not finished");

    http.end();
    otaInProgress = false;
    return false;
  }

  http.end();

  Serial.println("[OTA] SUCCESS. Rebooting...");
  publishOtaStatus("success", "OTA success. Rebooting", 100);

  delay(1000);
  ESP.restart();

  return true;
}

     

     
// MANUAL OTA TEST AT BOOT
     

void maybeRunManualOtaTestAtBoot() {
  if (!MANUAL_OTA_TEST_ON_BOOT) {
    return;
  }

  if (manualOtaTestStarted) {
    return;
  }

  if (millis() < MANUAL_OTA_TEST_DELAY_MS) {
    return;
  }

  if (otaInProgress) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[MANUAL-OTA] WiFi not connected. Waiting...");
    return;
  }

  // Wait until MQTT is connected so OTA progress can be published to /ota/status.
  if (!mqttClient.connected()) {
    Serial.println("[MANUAL-OTA] MQTT not connected yet. Waiting...");
    return;
  }

  String targetVersion = String(MANUAL_OTA_TARGET_VERSION);
  String downloadUrl   = String(MANUAL_OTA_DOWNLOAD_URL);
  String sha256        = String(MANUAL_OTA_SHA256);
  size_t fileSize      = (size_t)MANUAL_OTA_FILE_SIZE_BYTES;

  targetVersion.trim();
  downloadUrl.trim();
  sha256.trim();

  if (sha256 == "PASTE_SHA256_HERE") {
    sha256 = "";
  }

  if (targetVersion.length() == 0) {
    Serial.println("[MANUAL-OTA] Missing target version. Aborting manual OTA test.");
    manualOtaTestStarted = true;
    return;
  }

  if (downloadUrl.length() == 0 || downloadUrl == "PASTE_FULL_BLOB_BIN_URL_HERE") {
    Serial.println("[MANUAL-OTA] Missing blob download URL. Aborting manual OTA test.");
    manualOtaTestStarted = true;
    return;
  }

  if (OTA_REQUIRE_SHA256 && sha256.length() == 0) {
    Serial.println("[MANUAL-OTA] Missing SHA-256. Aborting because OTA_REQUIRE_SHA256=true.");
    manualOtaTestStarted = true;
    return;
  }

  if (OTA_REQUIRE_FILESIZE && fileSize == 0) {
    Serial.println("[MANUAL-OTA] Missing file size. Aborting because OTA_REQUIRE_FILESIZE=true.");
    manualOtaTestStarted = true;
    return;
  }

  if (sha256.length() == 0) {
    Serial.println("[MANUAL-OTA] SHA-256 not provided. Test mode: hash verification will be skipped.");
  }

  if (fileSize == 0) {
    Serial.println("[MANUAL-OTA] File size not provided. Test mode: exact size check will be skipped.");
  }

  if (targetVersion == String(FW_VERSION)) {
    Serial.println("[MANUAL-OTA] Target version is same as current FW_VERSION. Skipping.");
    Serial.print("[MANUAL-OTA] Current FW : ");
    Serial.println(FW_VERSION);
    Serial.print("[MANUAL-OTA] Target FW  : ");
    Serial.println(targetVersion);

    manualOtaTestStarted = true;
    publishOtaStatus("skipped", "Manual OTA target version same as current firmware");
    return;
  }

  manualOtaTestStarted = true;

  Serial.println();
  printLine();
  Serial.println("MANUAL OTA TEST AT BOOT");
  printLine('-');

  Serial.print("[MANUAL-OTA] Current FW : ");
  Serial.println(FW_VERSION);

  Serial.print("[MANUAL-OTA] Target FW  : ");
  Serial.println(targetVersion);

  Serial.print("[MANUAL-OTA] URL        : ");
  Serial.println(downloadUrl);

  Serial.print("[MANUAL-OTA] SHA256     : ");
  Serial.println(sha256.length() > 0 ? sha256 : "NOT PROVIDED - SKIP VERIFY");

  Serial.print("[MANUAL-OTA] File size  : ");
  if (fileSize > 0) {
    Serial.println(fileSize);
  } else {
    Serial.println("NOT PROVIDED - USE HTTP CONTENT-LENGTH");
  }

  publishOtaStatus("pending", "Manual OTA boot test started", 0);

  performHttpOta(
    downloadUrl,
    sha256,
    fileSize,
    targetVersion
  );
}

     
// MQTT OTA CALLBACK
     

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String topicStr(topic);

  String message;
  message.reserve(length + 1);

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.println();
  printLine();
  Serial.println("MQTT RX");
  printLine('-');

  Serial.print("[MQTT-RX] Topic   : ");
  Serial.println(topicStr);
  Serial.print("[MQTT-RX] Payload : ");
  Serial.println(message);

  if (topicStr != otaCmdTopic) {
    Serial.println("[MQTT-RX] Ignored: not OTA command topic");
    return;
  }

  StaticJsonDocument<1536> doc;

  DeserializationError err = deserializeJson(doc, message);

  if (err) {
    Serial.print("[OTA] Bad OTA JSON: ");
    Serial.println(err.c_str());
    publishOtaStatus("failed", "Bad OTA JSON");
    return;
  }

  String cmd = doc["cmd"] | "";

  if (cmd != "start_ota") {
    Serial.println("[OTA] Ignored: cmd is not start_ota");
    return;
  }

  String targetVersion = doc["target_version"] | "";
  String downloadUrl   = doc["download_url"] | "";
  String checksumType  = doc["checksum_type"] | "sha256";
  String checksum      = doc["checksum"] | "";
  size_t fileSize      = doc["file_size_bytes"] | 0;

  targetVersion.trim();
  downloadUrl.trim();
  checksumType.trim();
  checksum.trim();

  if (checksum == "PASTE_SHA256_HERE") {
    checksum = "";
  }

  if (downloadUrl.length() == 0) {
    Serial.println("[OTA] Missing download_url");
    publishOtaStatus("failed", "Missing download_url");
    return;
  }

  if (checksumType != "sha256") {
    Serial.println("[OTA] Unsupported checksum_type");
    publishOtaStatus("failed", "Unsupported checksum_type");
    return;
  }

  if (targetVersion.length() > 0 && targetVersion == FW_VERSION) {
    Serial.println("[OTA] Target version same as current. Skipping.");
    publishOtaStatus("skipped", "Target version same as current firmware");
    return;
  }

  performHttpOta(downloadUrl, checksum, fileSize, targetVersion);
}

     
// MQTT X.509 CONNECT
     

bool connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[MQTT] WiFi not connected");
    return false;
  }

  if (!hasValidMqttIdentity()) {
    Serial.println("[MQTT] Missing MQTT identity");
    return false;
  }

  buildTopics();

  mqttSecureClient.stop();

  if (!hasValidPemMaterial()) {
    Serial.println("[MQTT] Missing MQTT PEM material");
    return false;
  }

  mqttSecureClient.setCACert(mqttRootCaPem.c_str());
  mqttSecureClient.setCertificate(mqttClientCertPem.c_str());
  mqttSecureClient.setPrivateKey(mqttPrivateKeyPem.c_str());
  mqttSecureClient.setTimeout(10);

  mqttClient.setClient(mqttSecureClient);
  mqttClient.setServer(mqttHost.c_str(), mqttPort);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(4096);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(10);

  String willPayload = makeOfflinePayload("unexpected_disconnect");

  Serial.println();
  printLine();
  Serial.println("MQTT CONNECT");
  printLine('-');

  Serial.print("[MQTT] Connecting with X.509 to ");
  Serial.print(mqttHost);
  Serial.print(":");
  Serial.println(mqttPort);

  Serial.print("[MQTT] Client ID: ");
  Serial.println(mqttClientId);

  Serial.print("[MQTT] LWT Topic: ");
  Serial.println(topicOffline);

  bool ok = mqttClient.connect(
    mqttClientId.c_str(),
    topicOffline.c_str(),
    1,
    false,
    willPayload.c_str()
  );

  if (!ok) {
    Serial.print("[MQTT] Failed rc=");
    Serial.println(mqttClient.state());

    Serial.println("[MQTT] Meaning:");
    Serial.println("       -4 = timeout");
    Serial.println("       -3 = connection lost");
    Serial.println("       -2 = TCP/TLS connection failed");
    Serial.println("       -1 = disconnected");
    Serial.println("        1 = bad protocol");
    Serial.println("        2 = bad client ID");
    Serial.println("        3 = server unavailable");
    Serial.println("        4 = bad username/password");
    Serial.println("        5 = not authorized");

    Serial.println("[CHECK]");
    Serial.println("  1. MQTT host and port 8883 are correct");
    Serial.println("  2. client_ca.pem is MQTT broker/server CA");
    Serial.println("  3. client_cert.pem matches private_key.pem");
    Serial.println("  4. Broker allows this cert/client ID");
    Serial.println("  5. ACL allows this topic");
    Serial.println("  6. OTA topics are allowed in ACL");

    return false;
  }

  Serial.println("[MQTT] Connected using X.509");

  if (mqttClient.subscribe(otaCmdTopic.c_str(), 1)) {
    Serial.print("[MQTT] Subscribed OTA command topic: ");
    Serial.println(otaCmdTopic);
  } else {
    Serial.println("[MQTT] OTA command subscribe failed");
  }

  publishOtaStatus("idle", "Device online and OTA listener active", 0);

  return true;
}

     
// SETUP / LOOP
     

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  printLine();
  Serial.println("SMART BOWL PHASE-1 HTTPS + PHASE-2 MQTT X.509 + OTA");
  printLine();
  Serial.print("FW_VERSION        : "); Serial.println(FW_VERSION);
  Serial.print("SERIAL_NUMBER     : "); Serial.println(getDeviceSerialNumber());
  Serial.print("BACKEND_BASE_URL  : "); Serial.println(BACKEND_BASE_URL);
  Serial.print("PROVISION_API     : "); Serial.println(PROVISION_API_PATH);
  Serial.print("RESET_NVS_ON_BOOT : "); Serial.println(RESET_NVS_ON_BOOT ? "true" : "false");
  Serial.print("MANUAL_OTA_TEST_ON_BOOT : "); Serial.println(MANUAL_OTA_TEST_ON_BOOT ? "true" : "false");
  Serial.print("MANUAL_OTA_TARGET       : "); Serial.println(MANUAL_OTA_TARGET_VERSION);
  printLine();

  randomSeed(esp_random());

  if (!LittleFS.begin(true)) {
    Serial.println("[FATAL] LittleFS mount failed");
    while (true) {
      delay(5000);
    }
  }
  Serial.println("[FS] LittleFS mounted");

  if (RESET_NVS_ON_BOOT) {
    clearNvs();
  }

  if (!connectWiFi()) {
    Serial.println("[BOOT] WiFi failed. Restarting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

  syncTimeForTls();

  bool hasNvsIdentity = loadMqttIdentityFromNvs();

  if (FORCE_PROVISION_API_ON_EVERY_BOOT) {
    Serial.println("[BOOT] Calling provisioning API even if already provisioned");

    bool apiOk = callProvisioningApi();

    if (!apiOk) {
      Serial.println("[BOOT] API did not provide usable MQTT identity");

      hasNvsIdentity = loadMqttIdentityFromNvs();

      if (!hasNvsIdentity) {
        Serial.println("[BOOT] No usable stored identity/PEM. MQTT cannot start without backend-provided PEMs.");
      }
    }
  } else {
    if (!hasNvsIdentity) {
      Serial.println("[BOOT] No NVS identity. Calling provisioning API.");

      bool apiOk = callProvisioningApi();

      if (!apiOk) {
        Serial.println("[BOOT] API failed and no stored identity/PEM is available.");
      }
    } else {
      Serial.println("[BOOT] NVS identity available. Skipping API.");
    }
  }

  if (!hasValidMqttIdentity() || !hasValidPemMaterial()) {
    Serial.println("[FATAL] MQTT identity or PEM material unavailable");
    while (true) {
      delay(5000);
    }
  }

  buildTopics();
  printCurrentConfig();

  diagnoseMqttNetwork();

  while (!connectMQTT()) {
    Serial.println("[MQTT] Retry in 5 seconds...");
    delay(MQTT_RETRY_MS);
  }

  publishOnlineStatus();

  Serial.println();
  printLine();
  Serial.println("[READY] Firmware running");
  Serial.println("[READY] Telemetry publishing active");
  Serial.println("[READY] MQTT X.509 active");
  Serial.println("[READY] OTA MQTT listener active");
  printLine();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Lost connection. Restarting...");
    delay(1000);
    ESP.restart();
  }

  if (!mqttClient.connected()) {
    Serial.println("[MQTT] Disconnected. Reconnecting...");

    while (!connectMQTT()) {
      Serial.println("[MQTT] Retry in 5 seconds...");
      delay(MQTT_RETRY_MS);
    }

    publishOnlineStatus();
  }

  mqttClient.loop();

  maybeRunManualOtaTestAtBoot();

  if (!otaInProgress && millis() - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = millis();
    publishTelemetry();
  }

  delay(10);
}