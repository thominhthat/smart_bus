/* 
  Arduino Nano 2 - Nhận lệnh LED từ Mega
  - MAX7219 LED matrix
  - Nhận lệnh qua Serial từ Mega
  - Lệnh "SCROLL:..." để cuộn chữ
  - Lệnh "CLEAR" để xóa màn hình
*/

#include <SPI.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>

#define MAX_DEVICES 4
#define MD_PIN_DATA 11
#define MD_PIN_CLK 13
#define MD_PIN_CS 10

MD_Parola matrix = MD_Parola(MD_MAX72XX::FC16_HW, MD_PIN_DATA, MD_PIN_CLK, MD_PIN_CS, MAX_DEVICES);

String incoming = "";

void setup() {
  Serial.begin(115200); // Nhận dữ liệu từ Mega
  matrix.begin();
  matrix.setIntensity(4);
  matrix.displayClear();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      processCommand(incoming);
      incoming = "";
    } else {
      incoming += c;
    }
  }
}

void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd.startsWith("SCROLL:")) {
    String msg = cmd.substring(7);
    matrix.displayClear();
    matrix.displayScroll(msg.c_str(), PA_CENTER, PA_SCROLL_LEFT, 50);
    unsigned long start = millis();
    while (millis() - start < 3000) matrix.displayAnimate();
    matrix.displayClear();
  } else if (cmd == "CLEAR") {
    matrix.displayClear();
  }
}
