#include <FB_Const.h>
#include <FB_Error.h>
#include <FB_Network.h>
#include <FB_Utils.h>
#include <Firebase.h>
#include <FirebaseESP32.h>
#include <FirebaseFS.h>
#include <MB_File.h>

/*
 * ============================================
 * SMART ROOM SAFETY SYSTEM - ESP32
 * Kelompok 4 - Laboratorium IoT
 * Universitas Telkom 2026
 * 
 * Libraries yang perlu diinstall di Arduino IDE:
 *   1. DHT sensor library (by Adafruit)
 *   2. ESP32Servo
 *   3. Firebase ESP Client (by Mobizt)
 * ============================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <DHT.h>
#include <ESP32Servo.h>

// ============================================
// GANTI SESUAI DATA ANDA
// ============================================
#define WIFI_SSID       "ARINA"
#define WIFI_PASSWORD   "88888888"
#define API_KEY         "AIzaSyBgr3EQDCI_Q_4ZidXilpGU5RuANc02d1k"
#define DATABASE_URL    "https://smartroomiot-5ecfd-default-rtdb.asia-southeast1.firebasedatabase.app"

// ============================================
// KONFIGURASI PIN
// ============================================
#define DHTPIN       4
#define DHTTYPE      DHT22
#define FLAME_PIN    34
#define SERVO_PIN    18
#define RELAY_PIN    26
#define LED_PIN      14
#define BUZZER_PIN   27

// ============================================
// KONFIGURASI SISTEM
// ============================================
#define BATAS_SUHU   32.0   // Suhu °C di atas ini, kipas otomatis menyala
#define RELAY_ON     HIGH
#define RELAY_OFF    LOW
#define SERVO_TUTUP  0
#define SERVO_BUKA   45

// ============================================
// OBJECT & VARIABEL GLOBAL
// ============================================
DHT dht(DHTPIN, DHTTYPE);
Servo servoJendela;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

unsigned long waktuTerakhirApi  = 0;
unsigned long waktuBlink        = 0;
unsigned long waktuKirimData    = 0;
unsigned long waktuBacaKontrol  = 0;

bool ledState     = LOW;
int  posisiServo  = SERVO_TUTUP;

// Kontrol dari Web Dashboard
bool kipasDariWeb = false;
bool alarmDariWeb = false;

// ============================================
// FUNGSI GERAK SERVO HALUS
// ============================================
void gerakServoSmooth(int dari, int ke) {
  if (dari < ke) {
    for (int pos = dari; pos <= ke; pos++) {
      servoJendela.write(pos);
      delay(20);
    }
  } else {
    for (int pos = dari; pos >= ke; pos--) {
      servoJendela.write(pos);
      delay(20);
    }
  }
}

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(FLAME_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN,   OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN,  RELAY_OFF);
  digitalWrite(LED_PIN,    LOW);
  digitalWrite(BUZZER_PIN, LOW);

  servoJendela.setPeriodHertz(50);
  servoJendela.attach(SERVO_PIN, 500, 2400);
  servoJendela.write(SERVO_TUTUP);

  // --- KONEKSI WIFI ---
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Menghubungkan ke Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("WiFi Terhubung! IP: ");
  Serial.println(WiFi.localIP());

  // --- KONEKSI FIREBASE ---
  config.api_key      = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase: Sign Up Berhasil!");
    signupOK = true;
  } else {
    Serial.printf("Firebase Error: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("=== SMART ROOM SAFETY SYSTEM AKTIF ===");
}

// ============================================
// LOOP UTAMA
// ============================================
void loop() {
  // --- BACA SENSOR ---
  int   flameValue    = digitalRead(FLAME_PIN);
  bool  apiTerdeteksi = (flameValue == HIGH); // Jika sensor terbalik, ganti HIGH -> LOW

  float suhu      = dht.readTemperature();
  float kelembapan = dht.readHumidity();
  bool  suhuPanas  = (!isnan(suhu) && suhu >= BATAS_SUHU);

  // Serial Monitor
  Serial.println("-----------------------------");
  if (!isnan(suhu)) {
    Serial.printf("Suhu       : %.1f °C\n", suhu);
    Serial.printf("Kelembapan : %.1f %%\n", kelembapan);
  } else {
    Serial.println("Gagal membaca DHT22!");
  }
  Serial.printf("Flame Raw  : %d\n", flameValue);
  Serial.printf("Sensor Api : %s\n", apiTerdeteksi ? "API TERDETEKSI!" : "Aman");

  // --- BACA KONTROL DARI FIREBASE (Setiap 1 detik) ---
  if (millis() - waktuBacaKontrol > 1000) {
    if (Firebase.ready() && signupOK) {
      if (Firebase.RTDB.getBool(&fbdo, "/Kontrol/Kipas")) {
        kipasDariWeb = fbdo.boolData();
      }
      if (Firebase.RTDB.getBool(&fbdo, "/Kontrol/Alarm")) {
        alarmDariWeb = fbdo.boolData();
      }
    }
    waktuBacaKontrol = millis();
  }

  // --- LOGIKA KIPAS ---
  // Kipas menyala jika: suhu panas (otomatis) ATAU dinyalakan dari web
  bool kipasAktif = (suhuPanas || kipasDariWeb);
  if (kipasAktif) {
    digitalWrite(RELAY_PIN, RELAY_ON);
    Serial.println("Fan        : NYALA");
  } else {
    digitalWrite(RELAY_PIN, RELAY_OFF);
    Serial.println("Fan        : MATI");
  }

  // --- LOGIKA API / ALARM & SERVO ---
  // Alarm menyala jika: api terdeteksi (otomatis) ATAU dinyalakan dari web
  bool alarmAktif = (apiTerdeteksi || alarmDariWeb);
  if (alarmAktif) {
    waktuTerakhirApi = millis();

    // Buka jendela (servo)
    if (posisiServo != SERVO_BUKA) {
      gerakServoSmooth(posisiServo, SERVO_BUKA);
      posisiServo = SERVO_BUKA;
    }

    // Nyalakan buzzer
    digitalWrite(BUZZER_PIN, HIGH);

    // Kedipkan LED
    if (millis() - waktuBlink >= 300) {
      waktuBlink = millis();
      ledState   = !ledState;
      digitalWrite(LED_PIN, ledState);
    }

    Serial.println("Status     : DARURAT - ALARM ON, SERVO BUKA");

  } else {
    // Matikan semua peringatan
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN,    LOW);
    ledState = LOW;

    // Tunggu 3 detik setelah api hilang baru tutup jendela
    if (millis() - waktuTerakhirApi >= 3000) {
      if (posisiServo != SERVO_TUTUP) {
        gerakServoSmooth(posisiServo, SERVO_TUTUP);
        posisiServo = SERVO_TUTUP;
      }
      Serial.println("Status     : NORMAL - SERVO TUTUP");
    } else {
      Serial.println("Status     : AMAN - Tunggu 3 detik sebelum tutup servo");
    }
  }

  // --- KIRIM DATA KE FIREBASE (Setiap 2 detik) ---
  if (millis() - waktuKirimData > 2000) {
    if (Firebase.ready() && signupOK) {
      // Kirim data sensor
      if (!isnan(suhu))      Firebase.RTDB.setFloat(&fbdo,  "/Sensor/Suhu",       suhu);
      if (!isnan(kelembapan)) Firebase.RTDB.setFloat(&fbdo, "/Sensor/Kelembapan", kelembapan);
      Firebase.RTDB.setBool(&fbdo, "/Sensor/Api", apiTerdeteksi);

      // Kirim status aktuator (agar web dashboard bisa tampilkan status realtime)
      Firebase.RTDB.setBool(&fbdo, "/Status/Kipas", kipasAktif);
      Firebase.RTDB.setBool(&fbdo, "/Status/Alarm", alarmAktif);

      Serial.println("[Firebase] Data terkirim ke cloud.");
    }
    waktuKirimData = millis();
  }

  delay(10);
}
