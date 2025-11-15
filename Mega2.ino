/* 
  KẾ HOẠCH A - Arduino Mega2560 (cập nhật logic tuần tự & Serial log + TFT + LED qua Nano 2)
  - PN532 (UART, TX/RX)
  - 6 x HX711 (load cells)
  - TFT ST7735 (SPI) qua Arduino Nano 1
  - LED ma trận MAX7219 qua Arduino Nano 2
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

/* =================== PROTOTYPES =================== */
void logTFT(String msg);
void logTFT_Khoidong(String msg);
void sendLED(String msg);
void checkNFC_once();
void checkNFC_GV();
void checkNFC_HS();
void checkButton();
void handleButtonLogic();
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

// Buttons, Buzzer, Fan
#define BUTTON_PIN 28
#define BUZZER_PIN 27
#define FAN_PIN 26

// TFT via Nano 1
#define TFT_NANO Serial3

// LED via Nano 2 (SoftwareSerial)
SoftwareSerial ledNanoSerial(50, 48); // RX=50, TX=48
#define LED_NANO ledNanoSerial

/* =================== SYSTEM STATE =================== */
enum PhaseType {PHASE_NONE, PHASE_PICK, PHASE_DROP};
enum PhaseStep {STEP_B1, STEP_B1_5, STEP_B2};

PhaseType currentPhase = PHASE_NONE;
PhaseStep currentStep = STEP_B1;
PhaseStep previousStep = STEP_B1; // lưu lại B1 hoặc B1.5 trước khi vào B2
bool phaseStarted = false;
int teacherScanCount = 0;
bool tripEnded = false;

bool seatCheckActive = false;
unsigned long lastSeatCheckMillis = 0;
const unsigned long SEAT_CHECK_INTERVAL = 10000UL;

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
  {"3F35F4E5", "Nguyen Van A", "1A", "0862853461", false, false},
  {"FF64F6E5", "Le Thi B", "1A", "0862853461", false, false},
  {"3F8F05E6", "Tran Van C", "1B", "0862853461", false, false},
  {"82350CD5", "Pham Thi D", "1C", "0945721601", false, false},
  {"BF8B08E6", "Vo Van E", "1B", "0945721601", false, false},
  {"9F6706E6", "Do Thi F", "1C", "0945721601", false, false}
};

const String teacherPickID = "B15DFD03";
const String teacherDropID = "E996C601";
const String teacherbusPhone = "0946626711";

/* =================== NFC read buffer =================== */
String lastScannedID = "";
unsigned long lastScanMillis = 0;
const unsigned long NFC_DEBOUNCE_MS = 800;

/* =================== SETUP =================== */
void setup() {
  Serial.begin(115200);
  TFT_NANO.begin(115200);
  LED_NANO.begin(115200);
  logTFT("=== KHOI DONG HE THONG XE DUA DON HOC SINH ===");

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(FAN_PIN, LOW);

  mp3Serial.begin(9600);
  if (mp3.begin(mp3Serial)) { mp3.volume(30); logTFT_Khoidong("[OK] DFPlayer ket noi thanh cong"); }
  else logTFT_Khoidong("[ERR] Khong tim thay DFPlayer");

  SIM.begin(9600);
  logTFT_Khoidong("[OK] Khoi tao SIM800L xong");

  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) { logTFT_Khoidong("[ERR] PN532 khong ket noi"); while(1); }
  nfc.SAMConfig();
  logTFT_Khoidong("[OK] PN532 ket noi thanh cong");

  for (int i = 0; i < NUM_STUDENTS; i++) {
    loadCells[i].begin(HX711_DOUT[i], HX711_SCK[i]);
    loadCells[i].tare();
    loadCells[i].set_scale(scale[i]);
    delay(50);
  }
  logTFT_Khoidong("[OK] HX711 khoi tao hoan tat");

  logTFT("Giao vien quet the (pha don/tra) de bat dau");
}

/* =================== LOOP =================== */
void loop() {
  checkNFC_once();

  if (lastScannedID.length() > 0) {
    if (lastScannedID == teacherPickID || lastScannedID == teacherDropID) checkNFC_GV();
    else if (phaseStarted && (currentStep == STEP_B1 || currentStep == STEP_B1_5)) checkNFC_HS();
    lastScannedID = "";
  }

  checkButton();

  if (phaseStarted && currentStep == STEP_B2) {
    if (millis() - lastSeatCheckMillis >= SEAT_CHECK_INTERVAL) {
      lastSeatCheckMillis = millis();
      seatCheckPoint();
    }
  }

  delay(20);
}

/* =================== NFC read (debounced) =================== */
void checkNFC_once() {
  uint8_t uid[7];
  uint8_t uidLength;
  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) return;
  String id = "";
  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) id += "0";
    id += String(uid[i], HEX);
  }
  id.toUpperCase();
  if (millis() - lastScanMillis < NFC_DEBOUNCE_MS) return;
  lastScanMillis = millis();
  lastScannedID = id;
}

/* =================== TEACHER NFC HANDLING =================== */
void checkNFC_GV() {
  String id = lastScannedID;
  buzz();

  if (!phaseStarted) {
    if (id == teacherPickID) { currentPhase = PHASE_PICK; phaseStarted = true; currentStep = STEP_B1; previousStep = STEP_B1; teacherScanCount=1; tripEnded=false; seatCheckActive=false; logTFT("Giao vien xac nhan PHA DON -> [B1] cho HS quet"); return; }
    if (id == teacherDropID) { currentPhase = PHASE_DROP; phaseStarted = true; currentStep = STEP_B1; previousStep = STEP_B1; teacherScanCount=1; tripEnded=false; seatCheckActive=false; logTFT("Giao vien xac nhan PHA TRA -> [B1] cho HS quet"); return; }
  }

  if (currentPhase == PHASE_PICK && currentStep == STEP_B1 && id == teacherDropID) {
    currentStep = STEP_B1_5;
    logTFT("Chuyen sang B1.5 (khong canh bao HS xuong truoc diem dung)");
    return;
  }

  if (currentPhase == PHASE_PICK && currentStep == STEP_B1_5 && id == teacherPickID) {
    currentStep = STEP_B1;
    logTFT("Chuyen ve B1");
    return;
  }

  if (currentStep == STEP_B1) {
    if ((currentPhase == PHASE_PICK && id == teacherPickID) || (currentPhase == PHASE_DROP && id == teacherDropID)) {
      teacherScanCount++;
      logTFT("Giao vien quet lan thu " + String(teacherScanCount));
      if (teacherScanCount >= 2) {
        checkTripEnd();
        currentPhase = PHASE_NONE; currentStep = STEP_B1; previousStep=STEP_B1; phaseStarted=false; teacherScanCount=0; seatCheckActive=false;
      }
      return;
    } else {
      logTFT("Giao vien quet the (khac pha hien tai) - bi bo qua");
      return;
    }
  } else {
    logTFT("Giao vien quet the khi o B2 - khong ket thuc hanh trinh");
    return;
  }
}

/* =================== STUDENT NFC HANDLING (co canh bao HS xuong som) =================== */
void checkNFC_HS() {
  String id = lastScannedID;
  buzz();
  for (int i = 0; i < NUM_STUDENTS; i++) {
    if (students[i].id == id) {
      bool before = students[i].onboard;
      students[i].onboard = !students[i].onboard;
      logTFT(students[i].name + (students[i].onboard ? " len xe" : " xuong xe"));
      
      // Cảnh báo HS xuống trước điểm dừng
      if (currentPhase == PHASE_PICK && currentStep == STEP_B1 && before && !students[i].onboard) {
        String txt = "HS " + students[i].name + " da xuong truoc diem dung!";
        logTFT("[ALERT] " + txt);

        // 1. Gửi SMS + gọi phụ huynh
        sendSMS(students[i], txt);
        callParent(students[i]);

        // 2. Phát cảnh báo liên tục đến khi nhấn nút
        logTFT("[ALERT] Phat canh bao lien tuc den khi nhan nut...");
        while (digitalRead(BUTTON_PIN) == HIGH) {
          mp3.play(4); // file cảnh báo HS xuống sớm
          sendLED("SCROLL:HS xuong truoc diem dung!");
          digitalWrite(FAN_PIN, HIGH);
          delay(5000);
        }

        // 3. Khi nút được nhấn -> dừng cảnh báo
        logTFT("[ALERT] Nut nhan -> ket thuc canh bao, tiep tuc hanh trinh");
        sendLED("CLEAR");
        digitalWrite(FAN_PIN, LOW);

        // 4. Chuyển sang B2 như bình thường
        handleButtonLogic();
      }
      
      return;
    }
  }
  logTFT("[NFC] The khong hop le: " + id);
}

/* =================== BUTTON HANDLING =================== */
void checkButton() {
  static bool lastState = HIGH;
  bool state = digitalRead(BUTTON_PIN);
  if (state == LOW && lastState == HIGH) { buzz(); handleButtonLogic(); delay(250); }
  lastState = state;
}

void handleButtonLogic() {
  if (!phaseStarted) { logTFT_Khoidong("Nut nhan nhung chua co giao vien xac nhan (bo qua)"); return; }

  if (currentStep == STEP_B1 || currentStep == STEP_B1_5) {
    previousStep = currentStep;  // lưu lại bước hiện tại trước khi vào B2
    currentStep = STEP_B2;
    seatCheckActive = true;
    lastSeatCheckMillis = millis();
    logTFT("[B2] Di chuyen - Kiem tra ghe lien tuc (NFC HS tam dung)");
  } else if (currentStep == STEP_B2) {
    currentStep = previousStep; // quay về đúng B1 hoặc B1.5
    seatCheckActive = false;
    logTFT((currentStep == STEP_B1 ? "[B1]" : "[B1.5]") + String(" Den diem tiep theo - Bat NFC HS"));
  }
}

/* =================== SEAT CHECK =================== */
void seatCheckPoint() {
  int seatedCount = 0;
  int studentCount = 0;
  for (int i = 0; i < NUM_STUDENTS; i++) {
    float w = loadCells[i].get_units(10);
    if (abs(w) >= 40) { students[i].seated = true; seatedCount++; }
    if (students[i].onboard) studentCount++;
  }
  logTFT("[CHECK] Ghe co nguoi: " + String(seatedCount) + " / HS tren xe: " + String(studentCount));
  if (studentCount > seatedCount) {
    mp3.play(1);
    logTFT("[ALERT] So HS > ghe ngoi => Canh bao!");
    sendLED("SCROLL:So HS > ghe ngoi!");
  } else { logTFT("[OK] So HS <= ghe hop le"); sendLED("CLEAR"); }
}

/* =================== CHECK TRIP END =================== */
void checkTripEnd() {
  logTFT("[TRIP END] Kiem tra toan bo he thong...");
  tripEnded = true;

  int onboardCount = 0;
  int occupiedSeats = 0;

  for (int i = 0; i < NUM_STUDENTS; i++) {
    if (students[i].onboard) onboardCount++;
    float w = loadCells[i].get_units(10);
    if (abs(w) >= 40) occupiedSeats++;
  }

  logTFT("[TRIP END] So HS con tren xe: " + String(onboardCount));
  logTFT("[TRIP END] So ghe co nguoi hoac do vat: " + String(occupiedSeats));

  if (onboardCount > 0) {
    logTFT("[TRIP END] CANH BAO: Con hoc sinh tren xe!");
    sendLED("SCROLL:Canh bao: HS bi bo quen!");
    digitalWrite(FAN_PIN, HIGH);
    for (int i = 0; i < NUM_STUDENTS; i++) if (students[i].onboard) { String msg = "Canh bao: Hoc sinh " + students[i].name + " chua xuong xe!"; logTFT("[TRIP END] " + msg); sendSMS(students[i], msg); callParent(students[i]); }

    logTFT("[TRIP END] Phat canh bao lien tuc den khi nhan nut...");
    while (digitalRead(BUTTON_PIN) == HIGH) { mp3.play(2); sendLED("SCROLL:Canh bao: HS bi bo quen!"); digitalWrite(FAN_PIN,HIGH); delay(5000); }
    logTFT("[TRIP END] Nut nhan -> ket thuc canh bao.");
    sendLED("CLEAR"); digitalWrite(FAN_PIN, LOW);
  } else if (onboardCount == 0 && occupiedSeats > 0) {
    logTFT("[TRIP END] CANH BAO: Ghe van co nguoi hoac do vat!");
    String msg = "Canh bao: Co nguoi hoac do vat bi bo quen tren xe!";
    sendLED("SCROLL:Canh bao: Do vat bi bo quen!");
    sendSMSTeacher(teacherbusPhone, msg);
    callTeacher(teacherbusPhone);

    logTFT("[TRIP END] Phat canh bao lien tuc den khi nhan nut...");
    while (digitalRead(BUTTON_PIN) == HIGH) { mp3.play(3); sendLED("SCROLL:Canh bao: Do vat bi bo quen!"); delay(7000); digitalWrite(FAN_PIN,HIGH); }
    logTFT("[TRIP END] Nut nhan -> ket thuc canh bao.");
    sendLED("CLEAR"); digitalWrite(FAN_PIN, LOW);
  } else {
    logTFT("[TRIP END] Ket thuc hanh trinh an toan.");
  }

  logTFT("[TRIP END] Hoan tat quy trinh ket thuc hanh trinh.");
  logTFT("============================================");

  for (int i = 0; i < NUM_STUDENTS; i++) { students[i].onboard=false; students[i].seated=false; }
}

/* =================== HELPER FUNCTIONS =================== */
void buzz() { digitalWrite(BUZZER_PIN, LOW); delay(100); digitalWrite(BUZZER_PIN, HIGH); }
void sendLED(String msg) { LED_NANO.println(msg); }
void sendTFT(String msg) { TFT_NANO.println(msg); }
void logTFT(String msg) { Serial.println(msg); sendTFT(msg); sendLED("SCROLL:" + msg); delay(250); }
void logTFT_Khoidong(String msg) { Serial.println(msg); sendTFT(msg); delay(250); }
void sendSMS(const Student &s, String msg) { SIM.println("AT+CMGF=1"); delay(200); SIM.println("AT+CMGS=\"" + s.parentPhone + "\""); delay(200); SIM.println(msg); SIM.write(26); delay(2000); }
void callParent(const Student &s) { SIM.println("ATD" + s.parentPhone + ";"); delay(3000); SIM.println("ATH"); delay(1000); }
void callTeacher(String phone) { SIM.println("ATD" + phone + ";"); delay(3000); SIM.println("ATH"); delay(1000); }
void sendSMSTeacher(String phone, String msg) { SIM.println("AT+CMGF=1"); delay(200); SIM.println("AT+CMGS=\"" + phone + "\""); delay(200); SIM.println(msg); SIM.write(26); delay(2000); }
bool waitButtonHold(int pin) { int state=digitalRead(pin); while(state==HIGH){state=digitalRead(pin);delay(50);} return state; }
