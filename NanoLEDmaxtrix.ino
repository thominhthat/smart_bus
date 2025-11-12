#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define MAX_DEVICES 4
#define MD_PIN_DATA 11
#define MD_PIN_CLK 13
#define MD_PIN_CS 10

MD_Parola matrix = MD_Parola(MD_MAX72XX::FC16_HW, MD_PIN_DATA, MD_PIN_CLK, MD_PIN_CS, MAX_DEVICES);

String inputString = "";
bool newData = false;

void setup() {
  Serial.begin(115200);
  matrix.begin();
  matrix.setIntensity(4);
  matrix.displayClear();

  Serial.println("Nano 2 ready - LED Matrix receiver");
}

void loop() {
  recvWithEndMarker();
  if (newData) {
    showOnMatrix(inputString);
    newData = false;
  }
}

void recvWithEndMarker() {
  while (Serial.available()) {
    char rc = Serial.read();
    if (rc == '\n') {
      newData = true;
      return;
    } else {
      inputString += rc;
    }
  }
}

void showOnMatrix(String msg) {
  Serial.print("Show: ");
  Serial.println(msg);
  matrix.displayClear();
  matrix.displayScroll(msg.c_str(), PA_CENTER, PA_SCROLL_LEFT, 50);
  unsigned long start = millis();
  while (millis() - start < 5000) matrix.displayAnimate();
  matrix.displayClear();
  inputString = "";
}
