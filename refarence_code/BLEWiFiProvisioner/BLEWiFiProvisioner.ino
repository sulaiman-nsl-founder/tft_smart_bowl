/*
 * ESP32 BLE WiFi Provisioner + ST7735 TFT + QR PoP PIN
 * + Device Info (MAC address, firmware version, chip info)
 * Libraries: NimBLE-Arduino, SPI, Adafruit_GFX, Adafruit_ST7735, qrcode.h
 */

#include <NimBLEDevice.h>
#include <WiFi.h>
#include <Preferences.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "qrcode.h"
// #include "esp_system.h"   // for esp_efuse_mac_get_default

// ── Firmware version — change this when you update ───────────────────────────
#define FW_VERSION "1.0.0"
#define MODEL_ID "BOWL.1.0.0"

// ── TFT Display (ESP32-S3 FSPI) ──────────────────────────────────────────────
#define TFT_CS   47
#define TFT_DC   14
#define TFT_RST  21
#define TFT_MOSI 38
#define TFT_SCLK 39

SPIClass TFTSPI(FSPI);
Adafruit_ST7735 tft = Adafruit_ST7735(&TFTSPI, TFT_CS, TFT_DC, TFT_RST);

// ── BLE UUIDs ─────────────────────────────────────────────────────────────────
#define SERVICE_UUID      "12345678-1234-1234-1234-123456789abc"
#define CHAR_PIN_UUID     "12345678-1234-1234-1234-123456789000"
#define CHAR_SSID_UUID    "12345678-1234-1234-1234-123456789001"
#define CHAR_PASS_UUID    "12345678-1234-1234-1234-123456789002"
#define CHAR_STATUS_UUID  "12345678-1234-1234-1234-123456789003"
#define CHAR_DEVINFO_UUID "12345678-1234-1234-1234-123456789004"

// ── PoP PIN ───────────────────────────────────────────────────────────────────
#define POP_PIN    "1234"
#define QR_CONTENT "W:" SERVICE_UUID ":" POP_PIN

// ── Screen IDs ────────────────────────────────────────────────────────────────
typedef enum {
  SCR_NONE = -1,
  SCR_BOOT,
  SCR_QR,
  SCR_ADVERTISING,
  SCR_PHONE_CONNECTED,
  SCR_ENTER_PIN,
  SCR_PIN_OK,
  SCR_PIN_FAIL,
  SCR_LOCKED,
  SCR_SENDING,
  SCR_WIFI_CONNECTING,
  SCR_WIFI_CONNECTED,
  SCR_WIFI_FAILED,
  SCR_AUTO_CONNECTED,
  SCR_DEVICE_INFO,
} Screen;

// ── Pending action flags ──────────────────────────────────────────────────────
volatile Screen pendingScreen    = SCR_NONE;
volatile bool   pendingConnect   = false;
volatile bool   pendingDisconnect = false;
char   pendingStatus[128]        = "";
bool   hasPendingStatus          = false;

// ── Runtime state ─────────────────────────────────────────────────────────────
bool   pinVerified    = false;
String receivedSSID   = "";
String receivedPass   = "";
String connectedIP    = "";
String connectedSSID  = "";
int    disconnectReason = 0;
Screen currentScreen  = SCR_BOOT;

// ── Device info strings ───────────────────────────────────────────────────────
String deviceMAC      = "";
String deviceInfo     = "";

NimBLECharacteristic* pStatusChar;
NimBLECharacteristic* pDevInfoChar;
NimBLEServer* pBLEServer;

// ── Build device info JSON ────────────────────────────────────────────────────
// ── Build device info JSON ────────────────────────────────────────────────────
void buildDeviceInfo() {
  // 1. Get MAC Address easily using the WiFi library
  deviceMAC = WiFi.macAddress();

  // 2. Get Chip and Flash info using the built-in ESP class
  String chipModel = ESP.getChipModel();
  uint8_t cores = ESP.getChipCores();
  uint32_t flashSizeMB = ESP.getFlashChipSize() / (1024 * 1024);

  // 3. Build JSON — app parses this directly
  char buf[200];
  snprintf(buf, sizeof(buf),
    "{\"mac\":\"%s\",\"fw\":\"%s\",\"chip\":\"%s\",\"cores\":%d,\"flash\":\"%dMB\",\"ModelID\":\"%s\"}",
    deviceMAC.c_str(),
    FW_VERSION,
    chipModel.c_str(),
    cores,
    (int)flashSizeMB,
    MODEL_ID
  );
  deviceInfo = String(buf);

  Serial.println("[INFO] " + deviceInfo);
}

// ── Queue helpers ─────────────────────────────────────────────────────────────
void queueScreen(Screen s) { pendingScreen = s; }
void queueStatus(const char* msg) {
  strncpy(pendingStatus, msg, sizeof(pendingStatus) - 1);
  hasPendingStatus = true;
}
void sendStatus(const String& msg) {
  pStatusChar->setValue(msg.c_str());
  pStatusChar->notify();
  Serial.println("[→ APP] " + msg);
}

// ── TFT UI Helpers ────────────────────────────────────────────────────────────
void drawTitleBar(const char* title, uint16_t bgColor = ST77XX_WHITE, uint16_t textColor = ST77XX_BLACK) {
  tft.fillRect(0, 0, tft.width(), 18, bgColor);
  tft.setTextColor(textColor);
  tft.setTextSize(1);
  
  // Calculate center
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((tft.width() - w) / 2, 5);
  tft.print(title);
  
  // Reset colors for body
  tft.setTextColor(ST77XX_WHITE);
}

void drawCentered(const char* text, uint8_t y, uint8_t size, uint16_t color = ST77XX_WHITE) {
  tft.setTextSize(size);
  tft.setTextColor(color);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int x = (tft.width() - w) / 2;
  if (x < 0) x = 0;
  tft.setCursor(x, y);
  tft.print(text);
  tft.setTextColor(ST77XX_WHITE); // Reset
}

void drawWrapped(const char* label, const String& value, uint8_t y) {
  tft.setTextSize(1);
  tft.setCursor(5, y);
  tft.print(label);
  tft.setCursor(5, y + 15);
  tft.print(value.substring(0, 25)); // Cap string so it doesn't run off screen
}

// ── QR callback for TFT ───────────────────────────────────────────────────────
static void qrDisplayCallback(esp_qrcode_handle_t qrcode) {
  uint8_t size = esp_qrcode_get_size(qrcode);
  
  // Dynamically scale the QR code to fill the screen height beautifully
  uint8_t scale = 120 / size; 
  if (scale < 1) scale = 1;
  uint8_t margin = (128 - (size * scale)) / 2; // Vertically center
  uint8_t qrW = size * scale;

  // Draw a solid white box behind the QR code
  tft.fillRect(0, 0, (margin * 2) + qrW, tft.height(), ST77XX_WHITE);

  // Draw black modules
  for (uint8_t y = 0; y < size; y++) {
    for (uint8_t x = 0; x < size; x++) {
      if (esp_qrcode_get_module(qrcode, x, y)) {
        tft.fillRect(margin + (x * scale), margin + (y * scale), scale, scale, ST77XX_BLACK);
      }
    }
  }

  // Draw instructions on the right side
  uint8_t tx = (margin * 2) + qrW + 5;
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(tx, 20); tft.print("SCAN");
  tft.setCursor(tx, 35); tft.print("QR TO");
  tft.setCursor(tx, 50); tft.print("SETUP");
  tft.setCursor(tx, 70); tft.print("PIN:");
  
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(tx, 85); tft.print(POP_PIN);
  tft.setTextColor(ST77XX_WHITE); // reset
}

void drawQRScreen() {
  tft.fillScreen(ST77XX_BLACK);
  esp_qrcode_config_t cfg = {
    .display_func       = qrDisplayCallback,
    .max_qrcode_version = 10,
    .qrcode_ecc_level   = ESP_QRCODE_ECC_LOW,
  };
  esp_qrcode_generate(&cfg, QR_CONTENT);
}

// ── TFT Screen Renderer ───────────────────────────────────────────────────────
void renderScreen(Screen s) {
  currentScreen = s;
  if (s == SCR_QR) { drawQRScreen(); return; }

  // Clear screen immediately for all other views
  tft.fillScreen(ST77XX_BLACK);
  
  switch (s) {
    case SCR_BOOT:
      drawCentered("ESP32", 30, 2, ST77XX_CYAN);
      drawCentered("WiFi Provisioner", 60, 1);
      drawCentered(("FW: " + String(FW_VERSION)).c_str(), 80, 1);
      drawCentered(deviceMAC.c_str(), 100, 1);
      break;

    case SCR_DEVICE_INFO:
      drawTitleBar("DEVICE INFO");
      tft.setCursor(5, 40); tft.print("MAC:");
      tft.setCursor(5, 55); tft.print(deviceMAC);
      tft.setCursor(5, 80); tft.print("Firmware:");
      tft.setCursor(5, 95); tft.print("v" + String(FW_VERSION));
      break;

    case SCR_ADVERTISING:
      drawTitleBar("BLUETOOTH", ST77XX_BLUE, ST77XX_WHITE);
      drawCentered("Advertising...", 50, 1);
      drawCentered("ESP32-WiFi-Setup", 75, 1, ST77XX_YELLOW);
      drawCentered("Scan QR to connect", 100, 1);
      break;

    case SCR_PHONE_CONNECTED:
      drawTitleBar("BLE CONNECTED", ST77XX_GREEN, ST77XX_BLACK);
      drawCentered("Phone connected!", 50, 1);
      drawCentered("Verifying PIN...", 80, 1, ST77XX_CYAN);
      break;

    case SCR_ENTER_PIN:
      drawTitleBar("SECURITY", ST77XX_ORANGE, ST77XX_BLACK);
      drawCentered("Waiting for PIN", 50, 1);
      drawCentered("Scan QR code", 75, 1);
      drawCentered("to authenticate", 90, 1);
      break;

    case SCR_PIN_OK:
      drawTitleBar("PIN ACCEPTED", ST77XX_GREEN, ST77XX_BLACK);
      tft.drawRect(50, 35, 60, 45, ST77XX_GREEN);
      drawCentered("OK", 50, 2, ST77XX_GREEN);
      drawCentered("Send WiFi details", 100, 1);
      break;

    case SCR_PIN_FAIL:
      drawTitleBar("ACCESS DENIED", ST77XX_RED, ST77XX_WHITE);
      tft.drawRect(50, 35, 60, 45, ST77XX_RED);
      drawCentered("X", 50, 2, ST77XX_RED);
      drawCentered("Wrong PIN-rescan", 100, 1);
      break;

    case SCR_LOCKED:
      drawTitleBar("LOCKED", ST77XX_RED, ST77XX_WHITE);
      drawCentered("Scan QR first!", 50, 1);
      drawCentered("PIN required", 80, 1, ST77XX_RED);
      break;

    case SCR_SENDING:
      drawTitleBar("CREDENTIALS RX");
      drawWrapped("SSID:", connectedSSID, 40);
      drawCentered("Password received", 80, 1);
      drawCentered("Connecting...", 100, 1, ST77XX_YELLOW);
      break;

    case SCR_WIFI_CONNECTING:
      drawTitleBar("CONNECTING...", ST77XX_YELLOW, ST77XX_BLACK);
      drawWrapped("SSID:", connectedSSID, 40);
      drawCentered("Please wait", 90, 1);
      break;

    case SCR_WIFI_CONNECTED:
    case SCR_AUTO_CONNECTED:
      drawTitleBar(s == SCR_AUTO_CONNECTED ? "AUTO CONNECTED" : "WIFI CONNECTED", ST77XX_GREEN, ST77XX_BLACK);
      drawCentered(connectedSSID.c_str(), 45, 1);
      tft.setCursor(5, 75); tft.print("IP:");
      drawCentered(connectedIP.c_str(), 95, 1, ST77XX_CYAN);
      break;

    case SCR_WIFI_FAILED:
      drawTitleBar("CONN. FAILED", ST77XX_RED, ST77XX_WHITE);
      drawCentered("Cannot connect to:", 45, 1);
      drawCentered(connectedSSID.c_str(), 70, 1, ST77XX_YELLOW);
      drawCentered("Check credentials", 100, 1);
      break;

    default: break;
  }
}

// ── BLE Callbacks ─────────────────────────────────────────────────────────────
class PINCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
    NimBLEAttValue val = pChar->getValue();
    String entered(val.c_str());
    if (entered == POP_PIN) {
      pinVerified = true;
      queueStatus("STATE:PIN_OK");
      queueScreen(SCR_PIN_OK);
    } else {
      pinVerified = false;
      queueStatus("STATE:PIN_FAIL");
      queueScreen(SCR_PIN_FAIL);
    }
  }
};

class SSIDCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
    if (!pinVerified) { queueStatus("STATE:LOCKED"); queueScreen(SCR_LOCKED); return; }
    NimBLEAttValue val = pChar->getValue();
    receivedSSID  = String(val.c_str());
    connectedSSID = receivedSSID;
    char msg[80];
    snprintf(msg, sizeof(msg), "STATE:SSID_RECEIVED:%s", receivedSSID.c_str());
    queueStatus(msg);
  }
};

class PassCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
    if (!pinVerified) { queueStatus("STATE:LOCKED"); queueScreen(SCR_LOCKED); return; }
    NimBLEAttValue val = pChar->getValue();
    receivedPass = String(val.c_str());
    queueStatus("STATE:PASS_RECEIVED");
    queueScreen(SCR_SENDING);
    pendingConnect = true;
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    queueStatus("STATE:BLE_CONNECTED");
    queueScreen(SCR_PHONE_CONNECTED);
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    pinVerified      = false;
    receivedSSID     = "";
    receivedPass     = "";
    disconnectReason = reason;
    pendingDisconnect = true;
  }
};

// ── WiFi Connect Routine ──────────────────────────────────────────────────────
void doWifiConnect() {
  if (receivedSSID.isEmpty()) {
    sendStatus("STATE:WIFI_FAILED:no_ssid");
    renderScreen(SCR_WIFI_FAILED);
    return;
  }

  renderScreen(SCR_WIFI_CONNECTING);
  sendStatus("STATE:WIFI_CONNECTING");
  WiFi.disconnect(true);
  WiFi.begin(receivedSSID.c_str(), receivedPass.c_str());

  uint8_t tries = 0, dots = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500); tries++;
    
    // Instead of flashing the whole screen, just overwrite the bottom part
    tft.fillRect(0, 80, tft.width(), 48, ST77XX_BLACK);
    
    String dotStr = "";
    for (uint8_t i = 0; i < (dots % 4); i++) dotStr += " .";
    drawCentered(dotStr.c_str(), 100, 2);
    
    dots++;
    if (tries % 4 == 0)
      sendStatus(String("STATE:WIFI_CONNECTING:attempt=") + tries + "/20");
  }

  if (WiFi.status() == WL_CONNECTED) {
    connectedIP = WiFi.localIP().toString();
    sendStatus(String("STATE:WIFI_CONNECTED:") + connectedIP + ":ssid=" + connectedSSID);
    renderScreen(SCR_WIFI_CONNECTED);
    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.putString("ssid", receivedSSID);
    prefs.putString("pass", receivedPass);
    prefs.end();
  } else {
    sendStatus(String("STATE:WIFI_FAILED:ssid=") + connectedSSID);
    renderScreen(SCR_WIFI_FAILED);
    WiFi.disconnect(true);
  }
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  buildDeviceInfo();

  // 1. Initialize TFT
  TFTSPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB); 
  tft.setRotation(1); // Set to landscape
  tft.fillScreen(ST77XX_BLACK);

  // 2. Initial Boot Screens
  renderScreen(SCR_BOOT);
  delay(1500);
  renderScreen(SCR_DEVICE_INFO);
  delay(1500);

  // 3. Auto-reconnect
  Preferences prefs;
  prefs.begin("wifi", true);
  String savedSSID = prefs.getString("ssid", "");
  String savedPass = prefs.getString("pass", "");
  prefs.end();

  if (!savedSSID.isEmpty()) {
    connectedSSID = savedSSID;
    renderScreen(SCR_WIFI_CONNECTING);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    uint8_t t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 10) { delay(500); t++; }
    if (WiFi.status() == WL_CONNECTED) {
      connectedIP = WiFi.localIP().toString();
      renderScreen(SCR_AUTO_CONNECTED);
    }
  }

  // 4. BLE Init
  NimBLEDevice::init("");
  NimBLEDevice::setMTU(185);

  pBLEServer = NimBLEDevice::createServer();
  pBLEServer->setCallbacks(new ServerCallbacks());
  NimBLEService* pService = pBLEServer->createService(SERVICE_UUID);

  NimBLECharacteristic* pPINChar = pService->createCharacteristic(CHAR_PIN_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  pPINChar->setCallbacks(new PINCallback());

  NimBLECharacteristic* pSSIDChar = pService->createCharacteristic(CHAR_SSID_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  pSSIDChar->setCallbacks(new SSIDCallback());

  NimBLECharacteristic* pPassChar = pService->createCharacteristic(CHAR_PASS_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  pPassChar->setCallbacks(new PassCallback());

  pStatusChar = pService->createCharacteristic(CHAR_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  pStatusChar->setValue("STATE:BOOT");

  pDevInfoChar = pService->createCharacteristic(CHAR_DEVINFO_UUID, NIMBLE_PROPERTY::READ);
  pDevInfoChar->setValue(deviceInfo.c_str());

  pService->start();

  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  pAdv->setName("ESP32-WiFi-Setup");
  pAdv->addServiceUUID(SERVICE_UUID);
  pAdv->enableScanResponse(true);
  pAdv->start();

  if (WiFi.status() == WL_CONNECTED) {
    renderScreen(SCR_AUTO_CONNECTED);
    sendStatus(String("STATE:AUTO_CONNECTED:") + connectedIP);
  } else {
    renderScreen(SCR_QR);
    sendStatus("STATE:ADVERTISING");
  }
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  if (hasPendingStatus) {
    hasPendingStatus = false;
    String msg = String(pendingStatus);
    sendStatus(msg);
    if (msg == "STATE:PIN_OK") {
      delay(50);
      sendStatus("STATE:DEVICE_INFO:" + deviceInfo);
    }
  }

  if (pendingScreen != SCR_NONE) {
    Screen s      = pendingScreen;
    pendingScreen = SCR_NONE;
    if (s == SCR_PHONE_CONNECTED) {
      renderScreen(SCR_PHONE_CONNECTED);
      delay(800);
      renderScreen(SCR_ENTER_PIN);
      sendStatus("STATE:ENTER_PIN");
    } else {
      renderScreen(s);
    }
  }

  if (pendingDisconnect) {
    pendingDisconnect = false;
    char msg[64];
    snprintf(msg, sizeof(msg), "STATE:BLE_DISCONNECTED:reason=%d", disconnectReason);
    sendStatus(msg);
    NimBLEDevice::startAdvertising();
    if (WiFi.status() != WL_CONNECTED) {
      renderScreen(SCR_QR);
      sendStatus("STATE:ADVERTISING");
    }
  }

  if (pendingConnect) {
    pendingConnect = false;
    doWifiConnect();
  }

  delay(10);
}