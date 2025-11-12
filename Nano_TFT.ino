#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/FreeSans9pt7b.h>

Adafruit_ST7735 tft = Adafruit_ST7735(10, 9, 8);

#define SCREEN_W 128
#define SCREEN_H 160

void drawWrappedTextTopLeft(String msg) {
  tft.fillScreen(ST77XX_GREEN);
  tft.setTextColor(ST77XX_BLACK);
  tft.setFont(&FreeSans9pt7b);

  int16_t x1, y1;
  uint16_t w, h;

  String line = "";
  String word = "";
  String lines[10];
  int lineCount = 0;
  int maxWidth = SCREEN_W - 6; // chừa lề nhỏ

  // --- Tách chuỗi thành các dòng nhỏ vừa màn hình ---
  for (uint16_t i = 0; i < msg.length(); i++) {
    char c = msg[i];
    word += c;

    if (c == ' ' || i == msg.length() - 1) {
      String testLine = line + word;
      tft.getTextBounds(testLine, 0, 0, &x1, &y1, &w, &h);
      if (w > maxWidth && line.length() > 0) {
        lines[lineCount++] = line;
        line = word;
      } else {
        line = testLine;
      }
      word = "";
    }
  }
  if (line.length() > 0) lines[lineCount++] = line;

  // --- Hiển thị từ trên xuống, trái sang ---
  int y = 16; // khoảng cách từ mép trên
  for (int i = 0; i < lineCount; i++) {
    tft.getTextBounds(lines[i], 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(3, y + h);
    tft.print(lines[i]);
    y += h + 4;
  }
}

void setup() {
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_GREEN);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(ST77XX_BLACK);
  tft.setCursor(3, 16);
  tft.println("Dang cho du lieu...");
}

void loop() {
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    drawWrappedTextTopLeft(msg);
    Serial.println(msg);
  }
}
