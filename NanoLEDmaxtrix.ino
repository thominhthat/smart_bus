#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define MAX_DEVICES 4
#define MD_PIN_DATA 11
#define MD_PIN_CLK 13
#define MD_PIN_CS 10

MD_Parola matrix = MD_Parola(MD_MAX72XX::FC16_HW, MD_PIN_DATA, MD_PIN_CLK, MD_PIN_CS, MAX_DEVICES);

void setup() {
  Serial.begin(115200);  // Nhận từ Mega
  matrix.begin();
  matrix.setIntensity(4);
  matrix.displayClear();
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "CLEAR") {
      matrix.displayClear();
    } else if (cmd.startsWith("SCROLL:")) {
      String msg = cmd.substring(7);
      matrix.displayClear();
      matrix.displayScroll(msg.c_str(), PA_CENTER, PA_SCROLL_LEFT, 50);
      unsigned long start = millis();
      while (millis() - start < 3000) matrix.displayAnimate();
      matrix.displayClear();
    }
  }
}
