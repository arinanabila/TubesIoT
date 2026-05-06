
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
#define WIFI_SSID "iPhone"
#define WIFI_PASSWORD "ayaaaaaa"
#define API_KEY "AIzaSyBgr3EQDCI_Q_4ZidXilpGU5RuANc02d1k"
#define DATABASE_URL "smartroomiot-5ecfd-default-rtdb.asia-southeast1.firebasedatabase.app"

// ============================================
// KONFIGURASI PIN
// ============================================
#define DHTPIN 4
#define DHTTYPE DHT22
#define FLAME_PIN 34
#define SERVO_PIN 18
#define RELAY_PIN 26
#define LED_PIN 14
#define BUZZER_PIN 27

// ============================================
// KONFIGURASI SISTEM
// ============================================
#define BATAS_SUHU 32.0 // Suhu °C di atas ini, kipas otomatis menyala
#define RELAY_ON HIGH
#define RELAY_OFF LOW
#define SERVO_TUTUP 0
#define SERVO_BUKA 45

// ============================================
// OBJECT & VARIABEL GLOBAL
// ============================================
DHT dht(DHTPIN, DHTTYPE);
Servo servoJendela;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

unsigned long waktuTerakhirApi = 0;
unsigned long waktuBlink = 0;
unsigned long waktuKirimData = 0;
unsigned long waktuBacaKontrol = 0;

bool ledState = LOW;
int posisiServo = SERVO_TUTUP;

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

  // --- KONEKSI WIFI DULUAN (ANTI-BROWNOUT) ---
  Serial.println("\n=== SMART ROOM SAFETY SYSTEM BOOTING ===");
  Serial.print("Menghubungkan ke Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiStart > 15000) {
      Serial.println("\nWiFi GAGAL! Lanjut mode offline.");
      break;
    }
    Serial.print(".");
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("WiFi Terhubung! IP: ");
    Serial.println(WiFi.localIP());

    // --- KONEKSI FIREBASE ---
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    // Aktifkan mode publik (bypass authentication)
    config.signer.test_mode = true;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
  }

  // --- INISIALISASI HARDWARE SETELAH WIFI STABIL ---
  dht.begin();

  pinMode(FLAME_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, RELAY_OFF);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  servoJendela.setPeriodHertz(50);
  servoJendela.attach(SERVO_PIN, 500, 2400);
  servoJendela.write(SERVO_TUTUP);

  Serial.println("=== SMART ROOM SAFETY SYSTEM AKTIF ===");
}

// ============================================
// LOOP UTAMA
// ============================================
void loop() {
  // --- BACA SENSOR ---
  int flameValue = digitalRead(FLAME_PIN);
  bool apiTerdeteksi = (flameValue == HIGH); // Sensor ini membaca HIGH saat ada api

  float suhu = dht.readTemperature();
  float kelembapan = dht.readHumidity();
  bool suhuPanas = (!isnan(suhu) && suhu >= BATAS_SUHU);

  // --- BACA KONTROL DARI FIREBASE (Setiap 1 detik) ---
  if (millis() - waktuBacaKontrol > 1000) {
    if (Firebase.ready()) {
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
  } else {
    digitalWrite(RELAY_PIN, RELAY_OFF);
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
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  } else {
    // Matikan semua peringatan
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    ledState = LOW;

    // Tunggu 3 detik setelah api hilang baru tutup jendela
    if (millis() - waktuTerakhirApi >= 3000) {
      if (posisiServo != SERVO_TUTUP) {
        gerakServoSmooth(posisiServo, SERVO_TUTUP);
        posisiServo = SERVO_TUTUP;
      }
    }
  }

  // --- KIRIM DATA & PRINT KE SERIAL MONITOR (Setiap 2 detik) ---
  // JANGAN KELUARKAN KODE INI AGAR SERIAL MONITOR TIDAK BANJIR!
  if (millis() - waktuKirimData > 2000) {
    Serial.println("-----------------------------");
    if (!isnan(suhu)) {
      Serial.printf("Suhu       : %.1f °C\n", suhu);
      Serial.printf("Kelembapan : %.1f %%\n", kelembapan);
    } else {
      Serial.println("Gagal membaca DHT22!");
    }
    Serial.printf("Flame Raw  : %d\n", flameValue);
    Serial.printf("Sensor Api : %s\n", apiTerdeteksi ? "API TERDETEKSI!" : "Aman");
    Serial.printf("Kipas      : %s\n", kipasAktif ? "NYALA" : "MATI");
    Serial.printf("Alarm      : %s\n", alarmAktif ? "DARURAT" : "NORMAL");

    if (Firebase.ready()) {
      bool success = false;
      if (!isnan(suhu))
        success = Firebase.RTDB.setFloat(&fbdo, "/Sensor/Suhu", suhu);
      if (!isnan(kelembapan))
        Firebase.RTDB.setFloat(&fbdo, "/Sensor/Kelembapan", kelembapan);
      Firebase.RTDB.setBool(&fbdo, "/Sensor/Api", apiTerdeteksi);

      Firebase.RTDB.setBool(&fbdo, "/Status/Kipas", kipasAktif);
      Firebase.RTDB.setBool(&fbdo, "/Status/Alarm", alarmAktif);

      if (success) {
        Serial.println("[Firebase] Data BERHASIL terkirim ke cloud.");
      } else {
        Serial.print("[Firebase] GAGAL mengirim: ");
        Serial.println(fbdo.errorReason());
      }
    } else {
      Serial.println("[Firebase] Menunggu koneksi Firebase siap...");
    }
    waktuKirimData = millis();
  }

  delay(10);
}
