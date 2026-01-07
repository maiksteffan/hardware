#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "ICommandController.h"

#define LED_COUNT 30      // change to your strip length
#define PIN_STRIP_1 5
#define PIN_STRIP_2 10

Adafruit_NeoPixel strip1(LED_COUNT, PIN_STRIP_1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(LED_COUNT, PIN_STRIP_2, NEO_GRB + NEO_KHZ800);

void setColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < LED_COUNT; i++) {
    strip1.setPixelColor(i, r, g, b);
    strip2.setPixelColor(i, r, g, b);
  }
  strip1.show();
  strip2.show();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}   // needed on Uno R4

  strip1.begin();
  strip2.begin();

  strip1.show(); // clear
  strip2.show();

  setColor(1, 1, 1);

  Serial.println("Ready. Commands: red, green, blue, white, off");
}

void loop() {
  static ICommandController* controller = nullptr;
  if (!controller) {
    // Reader reads from Serial when available
    auto reader = [](std::string& out)->bool {
      if (Serial.available()) {
        String s = Serial.readStringUntil('\n');
        s.trim();
        s.toLowerCase();
        out = std::string(s.c_str());
        return true;
      }
      return false;
    };

    // Sender writes to Serial
    auto sender = [](const std::string& msg){ Serial.println(msg.c_str()); };

    controller = createCommandController(reader, sender);
  }

  if (Serial.available()) {
    std::string cmd = controller->receiveCommand();

    if (cmd == "red") {
      setColor(40, 0, 0);
      controller->sendCommand("ack: red");
    } else if (cmd == "green") {
      setColor(0, 40, 0);
      controller->sendCommand("ack: green");
    } else if (cmd == "blue") {
      setColor(0, 0, 40);
      controller->sendCommand("ack: blue");
    } else if (cmd == "white") {
      setColor(40, 40, 40);
      controller->sendCommand("ack: white");
    } else if (cmd == "off") {
      setColor(0, 0, 0);
      controller->sendCommand("ack: off");
    } else {
      Serial.println("Unknown command");
      controller->sendCommand("ack: unknown");
    }
  }
}
