#include <Adafruit_MCP23X17.h>
#include <Wire.h>


Adafruit_MCP23X17 mcp;

// ==========================================
// Define Custom I2C Pins (Change these to your board's pins)
// ==========================================
#define I2C_SDA_PIN 42
#define I2C_SCL_PIN 41

// Define Button Pins (GPA0, GPA1, GPA2 correspond to 0, 1, 2)
#define BUTTON_1 0
#define BUTTON_2 1
#define BUTTON_3 2

// Define LED Pins (GPB0, GPB1, GPB2 correspond to 8, 9, 10)
#define LED_1 10
#define LED_2 9
#define LED_3 8

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;

  Serial.println("MCP23017 Custom I2C Pin Test");

  // Initialize I2C with your manual pins
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // Initialize the MCP23017 at the default I2C address (0x20)
  // We pass the customized '&Wire' instance to the library
  if (!mcp.begin_I2C(0x20, &Wire)) {
    Serial.println("Error: MCP23017 not found. Check your wiring!");
    while (1)
      ;
  }
  Serial.println("MCP23017 connected successfully.");

  // Configure LED pins as OUTPUT
  mcp.pinMode(LED_1, OUTPUT);
  mcp.pinMode(LED_2, OUTPUT);
  mcp.pinMode(LED_3, OUTPUT);

  // Configure Button pins as INPUT_PULLUP
  mcp.pinMode(BUTTON_1, INPUT_PULLUP);
  mcp.pinMode(BUTTON_2, INPUT_PULLUP);
  mcp.pinMode(BUTTON_3, INPUT_PULLUP);
}

void loop() {
  // Read the state of the buttons
  // Since we use INPUT_PULLUP, a pressed button reads LOW (0)
  bool btn1_state = !mcp.digitalRead(BUTTON_1);
  bool btn2_state = !mcp.digitalRead(BUTTON_2);
  bool btn3_state = !mcp.digitalRead(BUTTON_3);

  // Write the state to the LEDs
  mcp.digitalWrite(LED_1, btn1_state ? HIGH : LOW);
  mcp.digitalWrite(LED_2, btn2_state ? HIGH : LOW);
  mcp.digitalWrite(LED_3, btn3_state ? HIGH : LOW);

  // Print to Serial monitor for debugging
  Serial.print("B1: ");
  Serial.print(btn1_state);
  Serial.print(" | B2: ");
  Serial.print(btn2_state);
  Serial.print(" | B3: ");
  Serial.println(btn3_state);

  delay(50); // Small delay for debouncing/stability
}