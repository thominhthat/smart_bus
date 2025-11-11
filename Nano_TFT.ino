#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  8
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(9600); // phải cùng baud với Mega gửi qua Serial1
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_GREEN);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_BLACK);
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.startsWith("MSG|")) {
      String msg = line.substring(4);
      tft.fillScreen(ST77XX_GREEN);
      tft.setCursor(0, tft.height()/2 - 10);
      tft.println(msg);
    }
  }
}
