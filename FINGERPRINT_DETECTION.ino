#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <Preferences.h>
#include "BluetoothSerial.h"

/* ================= WIFI ================= */


const char* WIFI_SSID1 = "SHANKS BOT";
const char* WIFI_PASS1 = "shashank1234";

const char* WIFI_SSID2 = "vivo194";
const char* WIFI_PASS2 = "siddu12345";

const char* WIFI_SSID3 = "Aditya";
const char* WIFI_PASS3 = "Qwerty123";

const char* WIFI_SSID4 = "1968";
const char* WIFI_PASS4 = "hellohello";

const char* WIFI_SSID5 = "Corsit WLAN";
const char* WIFI_PASS5 = "corsit.sit";

/* ================= CLOUD API ================= */
String MONGO_API_URL = "https://biometric-corsit.onrender.com/log";

/* ================= NTP ================= */
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 19800;
const int DAYLIGHT_OFFSET_SEC = 0;

/* ================= LCD ================= */
LiquidCrystal_I2C lcd(0x27, 20, 4);

/* ================= FINGERPRINT ================= */
#define FP_RX 16
#define FP_TX 17
HardwareSerial FingerSerial(2);
Adafruit_Fingerprint finger(&FingerSerial);

/* ================= BUTTONS ================= */
#define ENROLL_BUTTON 4
#define DELETE_BUTTON 15

/* ================= OBJECTS ================= */
Preferences prefs;
BluetoothSerial BT;
WiFiClientSecure secureClient;

/* ================= VARIABLES ================= */
const String ADMIN_PASSWORD = "CORSIT";
int lastFingerID = -1;

/* =====================================================
   MAIN SCREEN
   ===================================================== */
void showMainScreen() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" CORSIT BIOMETRIC ");
  lcd.setCursor(0,1);
  lcd.print("--------------------");
  lcd.setCursor(0,2);
  lcd.print("BTN1(Y)=ENROLL");
  lcd.setCursor(0,3);
  lcd.print("BTN2(B)=DEL");
}

/* =====================================================
   TIME
   ===================================================== */
String getTimeStamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "TIME_NOT_SET";

  char buffer[30];
  strftime(buffer, sizeof(buffer),
           "%d-%m-%Y %H:%M:%S",
           &timeinfo);

  return String(buffer);
}

/* =====================================================
   NAME STORAGE
   ===================================================== */
void saveName(int id, String name) {
  prefs.begin("users", false);
  prefs.putString(String(id).c_str(), name);
  prefs.end();
}

String getName(int id) {
  prefs.begin("users", true);
  String name = prefs.getString(String(id).c_str(), "Unknown");
  prefs.end();
  return name;
}

/* =====================================================
   BLUETOOTH SAFE READ
   ===================================================== */
String readBluetooth() {
  unsigned long start = millis();
  while (!BT.available()) {
    if (millis() - start > 20000) return "";
  }
  String data = BT.readStringUntil('\n');
  data.trim();
  return data;
}

/* =====================================================
   CLOUD FUNCTION
   ===================================================== */
/* =====================================================
   CLOUD FUNCTION
   ===================================================== */
void sendToMongoCloud(String status,
                      String name,
                      int id,
                      String ts)
{
  if (WiFi.status() != WL_CONNECTED) return;

  secureClient.setInsecure();
  secureClient.stop();

  HTTPClient http;

  if (!http.begin(secureClient, MONGO_API_URL)) {
    Serial.println("HTTPS Begin Failed");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);

  char payload[200];

  snprintf(payload, sizeof(payload),
           "{\"status\":\"%s\",\"name\":\"%s\",\"id\":%d,\"timestamp\":\"%s\"}",
           status.c_str(),
           name.c_str(),
           id,
           ts.c_str());

  int httpCode = http.POST((uint8_t*)payload, strlen(payload));

  String response = "";

  if (httpCode > 0) {
    response = http.getString();
  }

  http.end();
  secureClient.stop();

  lcd.clear();

  if (response.indexOf("CHECK-IN SUCCESS") >= 0) {

    lcd.setCursor(0,0);
    lcd.print("CHECK-IN SUCCESS");

    lcd.setCursor(0,1);
    lcd.print("Name: ");
    lcd.print(name);

    lcd.setCursor(0,2);
    lcd.print("ID: ");
    lcd.print(id);
  }
  else if (response.indexOf("CHECK-OUT SUCCESS") >= 0) {

    lcd.setCursor(0,0);
    lcd.print("CHECK-OUT SUCCESS");

    lcd.setCursor(0,1);
    lcd.print("Name: ");
    lcd.print(name);

    lcd.setCursor(0,2);
    lcd.print("ID: ");
    lcd.print(id);
  }
  else if (response.indexOf("ALREADY CHECKED OUT") >= 0) {

    lcd.setCursor(0,0);
    lcd.print("ALREADY DONE");

    lcd.setCursor(0,1);
    lcd.print("Name: ");
    lcd.print(name);

    lcd.setCursor(0,2);
    lcd.print("ID: ");
    lcd.print(id);
  }
  else {

    lcd.setCursor(0,0);
    lcd.print("ACCESS DENIED");
  }

  delay(2500);
  showMainScreen();
  lastFingerID = -1;
}

/* =====================================================
   ENROLL FUNCTION
   ===================================================== */
void enrollFingerprint() {

  BT.begin("ESP32_BIOMETRIC");
  delay(500);

  lcd.clear();
  lcd.print("Enter Password");
  BT.println("Enter Password:");

  String pass = readBluetooth();

  if (pass != ADMIN_PASSWORD) {
    lcd.clear();
    lcd.print("Wrong Password");
    delay(2000);
    BT.end();
    showMainScreen();
    return;
  }

  int id = 0;

  // Find first free ID
  for (int i = 1; i < 128; i++) {
    if (finger.loadModel(i) != FINGERPRINT_OK) {
      id = i;
      break;
    }
  }

  if (id == 0) {
    lcd.clear();
    lcd.print("No Free Slot");
    delay(2000);
    BT.end();
    showMainScreen();
    return;
  }

  lcd.clear();
  lcd.print("Place Finger");

  // Wait for finger
  while (finger.getImage() != FINGERPRINT_OK);

  // Convert image to template buffer 1
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Image Error");
    delay(2000);
    BT.end();
    showMainScreen();
    return;
  }

  // Store directly (Single scan enrollment)
  if (finger.storeModel(id) != FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Store Failed");
    delay(2000);
    BT.end();
    showMainScreen();
    return;
  }

  lcd.clear();
  lcd.print("Enter Name:");
  BT.println("Enter Name:");

  String name = readBluetooth();
  if (name == "") {
    BT.end();
    showMainScreen();
    return;
  }

  saveName(id, name);

  lcd.clear();
  lcd.print("Saved ID:");
  lcd.setCursor(0,1);
  lcd.print(id);
  lcd.setCursor(0,2);
  lcd.print(name);

  delay(2000);

  BT.end();
  showMainScreen();
}

/* =====================================================
   DELETE ALL
   ===================================================== */
void deleteAllFingerprints() {

  BT.begin("ESP32_BIOMETRIC");
  delay(500);

  lcd.clear();
  lcd.print("Enter Password");
  BT.println("Enter Password:");

  String pass = readBluetooth();

  if (pass != ADMIN_PASSWORD) {
    lcd.clear();
    lcd.print("Wrong Password");
    delay(2000);
    BT.end();
    showMainScreen();
    return;
  }

  finger.emptyDatabase();

  prefs.begin("users", false);
  prefs.clear();
  prefs.end();

  lcd.clear();
  lcd.print("ALL DELETED");
  delay(2000);

  BT.end();
  showMainScreen();
}
/* =====================================================
   DELETE SPECIFIC ID
   ===================================================== */
void deleteSpecificFingerprint() {

  BT.begin("ESP32_BIOMETRIC");
  delay(500);

  lcd.clear();
  lcd.print("Specific Delete");
  delay(1500);

  lcd.clear();
  lcd.print("Enter Password");
  BT.println("Enter Password:");

  String pass = readBluetooth();

  if (pass != ADMIN_PASSWORD) {
    lcd.clear();
    lcd.print("Wrong Password");
    delay(2000);
    BT.end();
    showMainScreen();
    return;
  }

  lcd.clear();
  lcd.print("Enter ID:");
  BT.println("Enter ID to delete:");

  String idStr = readBluetooth();
  int id = idStr.toInt();

  if (id <= 0 || id > 127) {
    lcd.clear();
    lcd.print("Invalid ID");
    delay(2000);
    BT.end();
    showMainScreen();
    return;
  }

  if (finger.deleteModel(id) == FINGERPRINT_OK) {

    prefs.begin("users", false);
    prefs.remove(String(id).c_str());
    prefs.end();

    lcd.clear();
    lcd.print("ID Deleted:");
    lcd.setCursor(0,1);
    lcd.print(id);

  } else {
    lcd.clear();
    lcd.print("Delete Failed");
  }

  delay(2000);
  BT.end();
  showMainScreen();
}

/* =====================================================
   MATCH
   ===================================================== */
void matchContinuous() {

  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return;

  finger.image2Tz();
  p = finger.fingerFastSearch();

  if (p == FINGERPRINT_OK) {

    int id = finger.fingerID;

    if (id == lastFingerID) return;
    lastFingerID = id;

    String name = getName(id);
    String ts = getTimeStamp();

    sendToMongoCloud("AUTHORIZED", name, id, ts);
  }
  else {
    lastFingerID = -1;
    String ts = getTimeStamp();
    sendToMongoCloud("UNAUTHORIZED", "Unknown", -1, ts);
  }
}
void connectWiFi() {

  const char* ssids[] = {
    WIFI_SSID1,
    WIFI_SSID2,
    WIFI_SSID3,
    WIFI_SSID4,
    WIFI_SSID5
  };

  const char* passwords[] = {
    WIFI_PASS1,
    WIFI_PASS2,
    WIFI_PASS3,
    WIFI_PASS4,
    WIFI_PASS5
  };

  int totalNetworks = 5;

  WiFi.mode(WIFI_STA);

  for (int i = 0; i < totalNetworks; i++) {

    WiFi.disconnect(true);     // VERY IMPORTANT
    delay(1000);

    Serial.print("Trying: ");
    Serial.println(ssids[i]);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Connecting...");
    lcd.setCursor(0,1);
    lcd.print(ssids[i]);

    WiFi.begin(ssids[i], passwords[i]);

    int tries = 0;

    while (WiFi.status() != WL_CONNECTED && tries < 30) {
      delay(500);
      Serial.print(".");
      tries++;
    }

    if (WiFi.status() == WL_CONNECTED) {

      Serial.println();
      Serial.println("WiFi Connected!");
      Serial.println(WiFi.localIP());

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("WiFi Connected");
      lcd.setCursor(0,1);
      lcd.print(ssids[i]);

      delay(1500);
      return;
    }

    Serial.println("\nFailed...");
  }

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("No WiFi Found");

  Serial.println("No WiFi Found");
}

/* =====================================================
   SETUP
   ===================================================== */
void setup() {

  Serial.begin(9600);

  pinMode(ENROLL_BUTTON, INPUT_PULLUP);
  pinMode(DELETE_BUTTON, INPUT_PULLUP);

  Wire.begin(21,22);
  lcd.begin();
  lcd.backlight();

  connectWiFi();

  configTime(GMT_OFFSET_SEC,
             DAYLIGHT_OFFSET_SEC,
             NTP_SERVER);

  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) delay(1000);

  FingerSerial.begin(57600, SERIAL_8N1, FP_RX, FP_TX);
  finger.begin(57600);

  if (!finger.verifyPassword()) while(true);

  showMainScreen();
}


/* =====================================================
   LOOP
   ===================================================== */
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
  connectWiFi();
  }

  static unsigned long enrollPress = 0;
  static unsigned long deletePress = 0;

  // ENROLL BUTTON HOLD 10s
  if (digitalRead(ENROLL_BUTTON) == LOW) {
    if (enrollPress == 0) enrollPress = millis();
    if (millis() - enrollPress > 5000) {
      enrollFingerprint();
      enrollPress = 0;
    }
  } else {
    enrollPress = 0;
  }

  // DELETE BUTTON LOGIC
if (digitalRead(DELETE_BUTTON) == LOW) {

  if (deletePress == 0) deletePress = millis();

  unsigned long holdTime = millis() - deletePress;

  // At 10 seconds → Show option message
  if (holdTime > 10000 && holdTime < 20000) {

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Hold till 20 sec");

    lcd.setCursor(0,1);
    lcd.print("For ALL DELETE");

    lcd.setCursor(0,2);
    lcd.print("Release now for");

    lcd.setCursor(0,3);
    lcd.print("Specific Delete");
  }

  // At 20 seconds → Delete All
  if (holdTime >= 20000) {
    deleteAllFingerprints();
    deletePress = 0;
  }

} else {

  // If released between 10–20 seconds → Specific delete
  if (deletePress != 0) {

    unsigned long holdTime = millis() - deletePress;

    if (holdTime >= 10000 && holdTime < 20000) {
      deleteSpecificFingerprint();
    }
  }

  deletePress = 0;
}

  matchContinuous();
}