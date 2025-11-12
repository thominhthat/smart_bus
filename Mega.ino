/* 
  KẾ HOẠCH A - Arduino Mega2560 (cập nhật logic tuần tự & Serial log + TFT)
  - PN532 (UART, TX/RX)
  - 6 x HX711 (load cells)
  - TFT ST7735 (SPI) qua Aruino Nano
  - MAX7219 LED matrix (MD_Parola)
  - DFPlayer Mini (SoftwareSerial)
  - SIM800L
  - 1 nút xác nhận
  - Buzzer
*/

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <PN532_HSU.h>
#include <PN532.h>
#include "HX711.h"
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>

/* =================== PROTOTYPES =================== */
void logTFT(String msg); // <-- Hàm mới
void displayMatrix(String msg);
void checkNFC();
void checkNFC_GV();
void checkNFC_HS();
void checkButton();
void checkButton_GV();
void handleButtonLogic();
void handleButtonLogic_GV();
void seatCheckPoint();
void sendSMS(const struct Student &s, String msg);
void callParent(const struct Student &s);
void callTeacher(String phone);
void buzz();
void checkTripEnd();
void sendSMSTeacher(String phone, String msg);
bool waitButtonHold(int pin);
void sendTFT(String msg);

/* =================== PIN MAP =================== */
// MAX7219
#define MAX_DEVICES 4
#define MD_PIN_DATA 51
#define MD_PIN_CLK 52
#define MD_PIN_CS 53
MD_Parola matrix = MD_Parola(MD_MAX72XX::FC16_HW, MD_PIN_DATA, MD_PIN_CLK, MD_PIN_CS, MAX_DEVICES);

// PN532 (UART HSU)
PN532_HSU pn532hsu(Serial1);
PN532 nfc(pn532hsu);

// HX711
#define NUM_STUDENTS 6
HX711 loadCells[NUM_STUDENTS];
const int HX711_DOUT[NUM_STUDENTS] = {22, 24, 29, 31, 33, 35};
const int HX711_SCK[NUM_STUDENTS]  = {23, 25, 30, 32, 34, 36};

// DFPlayer Mini
SoftwareSerial mp3Serial(10, 11);
DFRobotDFPlayerMini mp3;

// SIM800L
#define SIM Serial2

// Button, Buzzer, Fan
#define BUTTON_PIN 28
#define BUZZER_PIN 27
#define FAN_PIN 26

/* =================== SYSTEM STATE =================== */
bool teacherReady = false;
bool tripStarted = false;
bool tripEnded = false;
bool seatCheckActive = false;
int totalStudentsOnBoard = 0;
int buttonCount = 0;
float scale[6] = {412.44,593.64,-3417.94,-1168.62,-1357.17,-155.75};

/* =================== STUDENT DATA =================== */
struct Student {
  String id;
  String name;
  String grade;
  String parentPhone;
  bool onboard;
  bool seated;
};
Student students[NUM_STUDENTS] = {
  {"BF8B08E6", "Nguyen Van A", "1A", "0332081366", false, false},
  {"9F6706E6", "Le Thi B", "1A", "0332081366", false, false},
  {"E996C601", "Tran Van C", "1B", "0332081366", false, false},
  {"3F8F05E6", "Pham Thi D", "1C", "0332081366", false, false},
  {"3F35F4E5", "Vo Van E", "1B", "0332081366", false, false},
  {"FF64F6E5", "Do Thi F", "1C", "0332081366", false, false}
};

const String teacherID = "B15DFD03";
const String teacherbusPhone = "0332081366";

/* =================== SETUP =================== */
void setup() {
  Serial.begin(115200);       // để debug qua USB
  Serial3.begin(115200);      // gửi dữ liệu TFT đến Nano
  logTFT("=== KHOI DONG HE THONG XE DUA DON HOC SINH ===");
  logTFT("=== Dang khoi dong... === ");

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(FAN_PIN, LOW);

  // LED Matrix
  matrix.begin();
  matrix.setIntensity(4);
  matrix.displayClear();

  // DFPlayer
  mp3Serial.begin(9600);
  bool dfOK = mp3.begin(mp3Serial);
  if (dfOK) { mp3.volume(20); logTFT("[OK] DFPlayer ket noi thanh cong"); }
  else logTFT("[ERR] Khong tim thay DFPlayer");

  // SIM800L
  SIM.begin(9600);
  logTFT("[OK] Khoi tao SIM800L xong");

  // PN532
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  bool pnOK = true;
  if (!versiondata) {
    logTFT("[ERR] PN532 khong ket noi");
    pnOK = false;
  } else {
    nfc.SAMConfig();
    logTFT("[OK] PN532 ket noi thanh cong");
  }

  // HX711
  bool hxOK = true;
  for (int i = 0; i < NUM_STUDENTS; i++) {
    loadCells[i].begin(HX711_DOUT[i], HX711_SCK[i]);
    loadCells[i].tare();
    loadCells[i].set_scale(scale[i]);
    delay(50);
  }
  logTFT("[OK] HX711 khoi tao hoan tat");

  // Tổng kiểm tra phần cứng
  if (dfOK && pnOK && hxOK) {
    logTFT("Tat ca thiet bi da san sang");
    delay(1500);
  } else {
    logTFT("Loi phan cung! Hay kiem tra lai ket noi phan cung!");
    while (1);
  }

  logTFT("Giao vien quet the va an nut de bat dau chuong trinh");
}

/* =================== LOOP =================== */
void loop() {

  if (teacherReady == false || tripStarted == false) {
    checkNFC_GV();
    checkButton_GV();
  } else {
    if (buttonCount == 2){
      checkNFC();
    }
    delay(2000);
    checkButton();
    checkTripEnd();

    // CHẠY LIÊN TỤC MỖI 10 GIÂY NẾU ĐANG Ở PHA KIỂM TRA GHẾ
    static unsigned long lastSeatCheck = 0;
    unsigned long currentMillis = millis();

    if (seatCheckActive && tripStarted) {
      if (currentMillis - lastSeatCheck >= 10000) {  // 10 giây
        lastSeatCheck = currentMillis;
        seatCheckPoint();
      }
    }
  }
}

/* =================== checkTripEnd =================== */
void checkTripEnd() {
  if (!teacherReady && buttonCount == 2 && tripStarted) { // Giáo viên đã xuống xe

    logTFT("[TRIP END] Giao vien da xuong xe => Xac nhan den diem cuoi (truong).");

    logTFT("[TRIP END] Bat PN532 de hoc sinh quet the xuong xe...");
    checkNFC_HS(); 

    logTFT("[TRIP END] Cho nhan nut ket thuc hanh trinh...");
    while (waitButtonHold(BUTTON_PIN) == LOW); 
    
    logTFT(">>> Nút đã nhấn xong");
    tripEnded = true;
    logTFT("[TRIP END] Nut duoc nhan -> Bat dau kiem tra toan bo he thong...");

    int onboardCount = 0;
    int occupiedSeats = 0;

    for (int i = 0; i < NUM_STUDENTS; i++) {
      if (students[i].onboard) onboardCount++;
      float w = loadCells[i].get_units(10);
      if (abs(w) >= 40) occupiedSeats++;
    }

    logTFT("[TRIP END] So HS con tren xe: " + String(onboardCount));
    logTFT("[TRIP END] So ghe co nguoi hoac do vat: " + String(occupiedSeats));

    // Trường hợp 1: còn học sinh
    if (onboardCount > 0) {
      logTFT("[TRIP END] CANH BAO: Con hoc sinh tren xe!");
      displayMatrix("Canh bao: HS bi bo quen!");

      for (int i = 0; i < NUM_STUDENTS; i++) {
        if (students[i].onboard) {
          String msg = "Canh bao: Hoc sinh " + students[i].name + " chua xuong xe!";
          logTFT("[TRIP END] " + msg);
          sendSMS(students[i], msg);
          callParent(students[i]);
        } 
      }

      logTFT("[TRIP END] Phat canh bao lien tuc den khi nhan nut...");
      while (digitalRead(BUTTON_PIN) == HIGH) {  
        mp3.play(2);                           
        matrix.displayScroll("Canh bao: HS bi bo quen!", PA_CENTER, PA_SCROLL_LEFT, 50);
        digitalWrite(FAN_PIN, HIGH);           

        unsigned long start = millis();
        while (millis() - start < 1000) {      
          matrix.displayAnimate();
          if (digitalRead(BUTTON_PIN) == LOW) break; 
          delay(10);
        }
      }
      logTFT("[TRIP END] Nut nhan -> ket thuc canh bao.");
      matrix.displayClear();
      digitalWrite(FAN_PIN, LOW);
    }

    // Trường hợp 2: không còn học sinh nhưng loadcell vẫn có người
    else if (onboardCount == 0 && occupiedSeats > 0) {
      logTFT("[TRIP END] CANH BAO: Ghe van co nguoi hoac do vat!");
      String msg = "Canh bao: Co nguoi hoac do vat bi bo quen tren xe!";
      displayMatrix("Canh bao: Do vat bi bo quen!");
      sendSMSTeacher(teacherbusPhone, msg);
      callTeacher(teacherbusPhone);

      logTFT("[TRIP END] Phat canh bao lien tuc den khi nhan nut...");
      while (digitalRead(BUTTON_PIN) == HIGH) {
        mp3.play(3);
        matrix.displayScroll("Canh bao: Do vat bi bo quen!", PA_CENTER, PA_SCROLL_LEFT, 50);
        matrix.displayAnimate();
        delay(5000);
        digitalWrite(FAN_PIN, HIGH);
        unsigned long start = millis();
        while (millis() - start < 1000) {      
          matrix.displayAnimate();
          if (digitalRead(BUTTON_PIN) == LOW) break; 
          delay(10);
        }
      }
      logTFT("[TRIP END] Nut nhan -> ket thuc canh bao.");
      matrix.displayClear();
      digitalWrite(FAN_PIN, LOW);
    }

    // Trường hợp 3: mọi thứ bình thường
    else {
      logTFT("[TRIP END] Ket thuc hanh trinh an toan.");
    }

    logTFT("[TRIP END] Hoan tat quy trinh ket thuc hanh trinh.");
    logTFT("============================================");

  } else {
    return;
  }
}

/* =================== NFC HANDLING =================== */
void checkNFC() {
  uint8_t uid[7];
  uint8_t uidLength;
  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) return;
  String id = "";
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) id += "0";
    id += String(uid[i], HEX);
  }
  id.toUpperCase();

  buzz();

  if (id == teacherID) {
    teacherReady = !teacherReady;
    logTFT(teacherReady ? "Giao vien len xe" : "Giao vien xuong xe");
    delay(3000);
    return;
  }

  for (int i = 0; i < NUM_STUDENTS; i++) {
    if (students[i].id == id) {
      bool before = students[i].onboard;
      students[i].onboard = !students[i].onboard;
      logTFT(students[i].name + (students[i].onboard ? " len xe" : " xuong xe"));
      delay(3000);

      if (before && !students[i].onboard && !tripEnded) {
        String txt = "HS " + students[i].name + " xuong truoc diem dung!";
        logTFT("[ALERT] " + txt);
        sendSMS(students[i], txt);
        callParent(students[i]);
      }
      return;
    }
  }
}

void checkNFC_GV() {
  uint8_t uid[7];
  uint8_t uidLength;
  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) return;
  String id = "";
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) id += "0";
    id += String(uid[i], HEX);
  }
  id.toUpperCase();

  buzz();
  if (id == teacherID) {
    teacherReady = !teacherReady;
    logTFT(teacherReady ? "Giao vien len xe" : "Giao vien xuong xe");
    delay(3000);
    return;
  } else {
    delay(3000);
    return;
  }
}

void checkNFC_HS() {
  uint8_t uid[7];
  uint8_t uidLength;
  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) return;
  String id = "";
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) id += "0";
    id += String(uid[i], HEX);
  }
  id.toUpperCase();

  buzz();
  for (int i = 0; i < NUM_STUDENTS; i++) {
    if (students[i].id == id) {
      bool before = students[i].onboard;
      students[i].onboard = !students[i].onboard;
      logTFT(students[i].name + (students[i].onboard ? " len xe" : " xuong xe"));
      delay(3000);
      return;
    }
  }
}

/* =================== BUTTON HANDLING =================== */
void checkButton() {
  static bool lastState = HIGH;
  bool state = digitalRead(BUTTON_PIN);
  if (state == LOW && lastState == HIGH) {
    buttonCount++;
    handleButtonLogic();
    delay(500);
  }
  lastState = state;
}

void handleButtonLogic() {
  if (buttonCount == 1 && teacherReady) {
    tripStarted = true;
    logTFT("[B1] Xac nhan bat dau hanh trinh don hoc sinh");
    seatCheckActive = false;
  }
  else if (buttonCount == 2 && tripStarted) {
    logTFT("[B2] Diem don - Bat PN532 de quet the");
    seatCheckActive = false;
  }
  else if (buttonCount == 3 && tripStarted) {
    logTFT("[B3] Ket thuc diem don - Kiem tra ghe sau 5s");
    delay(5000);
    seatCheckPoint();
    seatCheckActive = true;
    buttonCount = 1;
  }
}

void checkButton_GV() {
  static bool lastState = HIGH;
  bool state = digitalRead(BUTTON_PIN);
  if (state == LOW && lastState == HIGH) {
    if(teacherReady){
      buttonCount++;
      handleButtonLogic_GV();
      delay(500);
    }
  }
  lastState = state;
}

void handleButtonLogic_GV() {
  if (buttonCount == 1 && teacherReady) {
    tripStarted = true;
    buttonCount = 1;
    logTFT("[B1] Xac nhan bat dau hanh trinh don hoc sinh");
    delay(500);
  }
}

/* =================== SEAT CHECK =================== */
void seatCheckPoint() {
  int seatedCount = 0;
  int studentCount = 0;

  for (int i = 0; i < NUM_STUDENTS; i++) {
    float w = loadCells[i].get_units(10);
    if (abs(w) >= 40) {
      students[i].seated = true;
      seatedCount++;
    }
    if (students[i].onboard) studentCount++;
  }

  logTFT("[CHECK] Ghe co nguoi: " + String(seatedCount) + " / HS tren xe: " + String(studentCount));

  if (studentCount > seatedCount) {
    mp3.play(1);
    displayMatrix("Canh bao: ngoi dung cho!");
    logTFT("[ALERT] So HS > ghe ngoi => Canh bao!");
  } else {
    logTFT("[OK] So HS <= ghe hop le");
  }
}

/* =================== HELPER FUNCTIONS =================== */
void buzz() {
  digitalWrite(BUZZER_PIN, LOW);
  delay(100);
  digitalWrite(BUZZER_PIN, HIGH);
}

void displayMatrix(String msg) {
  matrix.displayClear();
  matrix.displayScroll(msg.c_str(), PA_CENTER, PA_SCROLL_LEFT, 50);
  unsigned long start = millis();
  while (millis() - start < 3000) matrix.displayAnimate();
  matrix.displayClear();
}

void sendSMS(const Student &s, String msg) {
  SIM.println("AT+CMGF=1");
  delay(200);
  SIM.print("AT+CMGS=\"");
  SIM.print(s.parentPhone);
  SIM.println("\"");
  delay(200);
  SIM.print(msg);
  SIM.write(26);
  delay(3000);
  logTFT("[SMS] " + msg + " -> " + s.parentPhone);
}

void callParent(const Student &s) {
  SIM.print("ATD");
  SIM.print(s.parentPhone);
  SIM.println(";");
  delay(10000);
  SIM.println("ATH");
  logTFT("[CALL] Goi dien -> " + s.parentPhone);
}

void callTeacher(String phone) {
  logTFT("[CALL TEACHER] Goi dien cho giao vien: " + phone);
  SIM.print("ATD");
  SIM.print(phone);
  SIM.println(";");
  delay(10000);
  SIM.println("ATH");
  logTFT("[CALL TEACHER] Cuoc goi ket thuc");
}

void sendSMSTeacher(String phone, String msg) {
  SIM.println("AT+CMGF=1");
  delay(200);
  SIM.print("AT+CMGS=\"");
  SIM.print(phone);
  SIM.println("\"");
  delay(200);
  SIM.print(msg);
  SIM.write(26);
  delay(3000);
  logTFT("[SMS] " + msg + " -> " + phone);
}

bool waitButtonHold(int pin) {
  if (digitalRead(pin) == LOW) {
    delay(500);
    if (digitalRead(pin) == LOW) {
      while (digitalRead(pin) == LOW) {
        delay(500);
      }
      return true;
    }
  }
  return false;
}

void sendTFT(String msg) {
  Serial3.println(msg); // Mega gửi lệnh TFT cho Nano
}

void logTFT(String msg) {
  Serial.println(msg);
  sendTFT(msg);
  delay(2500);
}
