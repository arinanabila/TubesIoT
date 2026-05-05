/*
 * ============================================
 * SMART ROOM SAFETY SYSTEM - ESP32
 * Kelompok 4 - Laboratorium IoT
 * Universitas Telkom 2026
 * 
 * Libraries yang dibutuhkan:
 *   1. DHT sensor library (by Adafruit)  <- install via Library Manager
 *   2. ESP32Servo                         <- install via Library Manager
 *   (TIDAK PERLU library Firebase apapun!)
 * ============================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ============================================
// GANTI SESUAI DATA ANDA
// ============================================
#define WIFI_SSID     "iPhone"
#define WIFI_PASSWORD "ayaaaaaa"

const char* FIREBASE_URL     = "https://smartroomiot-5ecfd-default-rtdb.asia-southeast1.firebasedatabase.app";
const char* FIREBASE_API_KEY = "AIzaSyBgr3EQDCI_Q_4ZidXilpGU5RuANc02d1k";

// ============================================
// KONFIGURASI PIN
// ============================================
#define DHTPIN      4
#define DHTTYPE     DHT22
#define FLAME_PIN   34
#define SERVO_PIN   18
#define RELAY_PIN   26
#define LED_PIN     14
#define BUZZER_PIN  27

#define BATAS_SUHU   32.0
#define RELAY_ON     HIGH
#define RELAY_OFF    LOW
#define SERVO_TUTUP  0
#define SERVO_BUKA   45

// ============================================
// OBJECT & VARIABEL GLOBAL
// ============================================
DHT dht(DHTPIN, DHTTYPE);
Servo servoJendela;

unsigned long waktuTerakhirApi = 0;
unsigned long waktuBlink       = 0;
unsigned long waktuKirimData   = 0;
unsigned long waktuBacaKontrol = 0;

bool ledState    = LOW;
int  posisiServo = SERVO_TUTUP;
bool kipasDariWeb = false;
bool alarmDariWeb = false;

// ============================================
// FUNGSI: Kirim data ke Firebase (PUT)
// ============================================
void firebaseSet(String path, String jsonValue) {
  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();
  String url = String(FIREBASE_URL) + path + ".json?auth=" + FIREBASE_API_KEY;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.PUT(jsonValue);
  if (httpCode > 0) {
    Serial.printf("[Firebase] SET %s = %s (HTTP %d)\n", path.c_str(), jsonValue.c_str(), httpCode);
  } else {
    Serial.printf("[Firebase] Error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

// ============================================
// FUNGSI: Baca data dari Firebase (GET)
// ============================================
String firebaseGet(String path) {
  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();
  String url = String(FIREBASE_URL) + path + ".json?auth=" + FIREBASE_API_KEY;
  http.begin(client, url);
  int httpCode = http.GET();
  String payload = "null";
  if (httpCode == 200) {
    payload = http.getString();
  }
  http.end();
  return payload;
}

// ============================================
// FUNGSI: Gerak servo halus
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
  delay(2000); // Tunggu serial siap
  Serial.begin(115200);
  Serial.println("\n=== SMART ROOM SAFETY SYSTEM BOOT ===");
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector
  dht.begin();

  pinMode(FLAME_PIN,  INPUT);
  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(LED_PIN,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN,  RELAY_OFF);
  digitalWrite(LED_PIN,    LOW);
  digitalWrite(BUZZER_PIN, LOW);

  servoJendela.setPeriodHertz(50);
  servoJendela.attach(SERVO_PIN, 500, 2400);
  servoJendela.write(SERVO_TUTUP);
  Serial.println("Hardware OK.");

  // Koneksi WiFi dengan timeout 15 detik
  Serial.print("Menghubungkan ke Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // Kurangi TX power agar tidak brownout
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiStart > 15000) {
      Serial.println("\nWiFi GAGAL! Cek SSID/Password.");
      Serial.println("Sistem berjalan TANPA Firebase.");
      break;
    }
    Serial.print(".");
    delay(300);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWiFi Terhubung! IP: ");
    Serial.println(WiFi.localIP());
  }

  // Inisialisasi kontrol di Firebase
  firebaseSet("/Kontrol/Kipas", "false");
  firebaseSet("/Kontrol/Alarm", "false");

  Serial.println("=== SMART ROOM SAFETY SYSTEM AKTIF ===");
}

// ============================================
// LOOP UTAMA
// ============================================
void loop() {
  // --- BACA SENSOR ---
  int  flameValue    = digitalRead(FLAME_PIN);
  bool apiTerdeteksi = (flameValue == HIGH); // Ganti HIGH -> LOW jika sensor terbalik

  float suhu       = dht.readTemperature();
  float kelembapan = dht.readHumidity();
  bool  suhuPanas  = (!isnan(suhu) && suhu >= BATAS_SUHU);

  Serial.println("-----------------------------");
  if (!isnan(suhu)) {
    Serial.printf("Suhu       : %.1f C\n", suhu);
    Serial.printf("Kelembapan : %.1f %%\n", kelembapan);
  } else {
    Serial.println("Gagal membaca DHT22!");
  }
  Serial.printf("Flame Raw  : %d\n", flameValue);
  Serial.printf("Sensor Api : %s\n", apiTerdeteksi ? "API TERDETEKSI!" : "Aman");

  // --- BACA KONTROL DARI FIREBASE (Setiap 2 detik) ---
  if (millis() - waktuBacaKontrol > 2000) {
    kipasDariWeb = (firebaseGet("/Kontrol/Kipas") == "true");
    alarmDariWeb = (firebaseGet("/Kontrol/Alarm") == "true");
    waktuBacaKontrol = millis();
  }

  // --- LOGIKA KIPAS ---
  bool kipasAktif = (suhuPanas || kipasDariWeb);
  digitalWrite(RELAY_PIN, kipasAktif ? RELAY_ON : RELAY_OFF);
  Serial.printf("Fan        : %s\n", kipasAktif ? "NYALA" : "MATI");

  // --- LOGIKA API / ALARM & SERVO ---
  bool alarmAktif = (apiTerdeteksi || alarmDariWeb);
  if (alarmAktif) {
    waktuTerakhirApi = millis();
    if (posisiServo != SERVO_BUKA) {
      gerakServoSmooth(posisiServo, SERVO_BUKA);
      posisiServo = SERVO_BUKA;
    }
    digitalWrite(BUZZER_PIN, HIGH);
    if (millis() - waktuBlink >= 300) {
      waktuBlink = millis();
      ledState   = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
    Serial.println("Status     : DARURAT - ALARM ON, SERVO BUKA");
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN,    LOW);
    ledState = LOW;
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

  // --- KIRIM DATA KE FIREBASE (Setiap 3 detik) ---
  if (millis() - waktuKirimData > 3000) {
    if (!isnan(suhu)) {
      firebaseSet("/Sensor/Suhu",       String(suhu, 1));
      firebaseSet("/Sensor/Kelembapan", String(kelembapan, 1));
    }
    firebaseSet("/Sensor/Api",   apiTerdeteksi ? "true" : "false");
    firebaseSet("/Status/Kipas", kipasAktif    ? "true" : "false");
    firebaseSet("/Status/Alarm", alarmAktif    ? "true" : "false");
    waktuKirimData = millis();
  }

  delay(10);
}
