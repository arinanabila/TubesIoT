#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <DHT.h>
#include <ESP32Servo.h>

// ============================================
// KONFIGURASI WIFI & FIREBASE
// ============================================
#define WIFI_SSID       "iPhone"
#define WIFI_PASSWORD   "ayaaaaaa"
#define API_KEY         "AIzaSyBgr3EQDCI_Q_4ZidXilpGU5RuANc02d1k"
#define DATABASE_URL    "https://smartroomiot-5ecfd-default-rtdb.asia-southeast1.firebasedatabase.app"

// ============================================
// KONFIGURASI PIN & HARDWARE
// ============================================
#define DHTPIN 4
#define DHTTYPE DHT22

#define FLAME_PIN 34
#define SERVO_PIN 18
#define RELAY_PIN 26
#define LED_PIN 14
#define BUZZER_PIN 27

#define BATAS_SUHU 32.0

#define RELAY_ON  HIGH
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

unsigned long waktuTerakhirApi   = 0;
unsigned long waktuBlink         = 0;
unsigned long waktuKirimData     = 0;
unsigned long waktuBacaKontrol   = 0;
unsigned long waktuCetakSerial   = 0;

bool ledState      = LOW;
int posisiServo    = SERVO_TUTUP;

// Variabel Kontrol Web
bool kipasDariWeb  = false;
bool alarmDariWeb  = false;

void gerakServoSmooth(int dari, int ke) {
  if (dari < ke) {
    for (int pos = dari; pos <= ke; pos++) {
      servoJendela.write(pos);
      delay(20);   // makin besar = makin pelan
    }
  } else {
    for (int pos = dari; pos >= ke; pos--) {
      servoJendela.write(pos);
      delay(20);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // 1. KONEKSI WIFI DULUAN (Bypass Brownout)
  Serial.println("\n=== SISTEM SMART ROOM BOOTING ===");
  Serial.print("Menghubungkan ke Wi-Fi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiStart > 15000) {
      Serial.println("\nWiFi GAGAL! Sistem berjalan tanpa internet.");
      break;
    }
    Serial.print(".");
    delay(300);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nWiFi Terhubung! IP: ");
    Serial.println(WiFi.localIP());
    
    // 2. INISIALISASI FIREBASE
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    
    if (Firebase.signUp(&config, &auth, "", "")) {
      Serial.println("Firebase: Berhasil terhubung!");
      signupOK = true;
    } else {
      Serial.printf("Firebase Error: %s\n", config.signer.signupError.message.c_str());
    }
    config.token_status_callback = tokenStatusCallback;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
  }

  // 3. INISIALISASI HARDWARE
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

  Serial.println("=== SISTEM SMART ROOM AKTIF ===");
}

void loop() {
  // ----------------------------------------------------
  // BACA SENSOR (REAL-TIME TANPA DELAY)
  // ----------------------------------------------------
  int flameValue = digitalRead(FLAME_PIN);
  bool apiTerdeteksi = (flameValue == LOW); // LOW karena Active-LOW sensor api

  float suhu = dht.readTemperature();
  float kelembapan = dht.readHumidity();
  bool suhuPanas = (!isnan(suhu) && suhu >= BATAS_SUHU);

  // ----------------------------------------------------
  // BACA KONTROL DARI WEB (Setiap 1 Detik)
  // ----------------------------------------------------
  if (millis() - waktuBacaKontrol > 1000) {
    if (Firebase.ready() && signupOK) {
      if (Firebase.RTDB.getBool(&fbdo, "/Kontrol/Kipas")) kipasDariWeb = fbdo.boolData();
      if (Firebase.RTDB.getBool(&fbdo, "/Kontrol/Alarm")) alarmDariWeb = fbdo.boolData();
    }
    waktuBacaKontrol = millis();
  }

  // ----------------------------------------------------
  // LOGIKA KIPAS (LOKAL + WEB)
  // ----------------------------------------------------
  bool kipasAktif = (suhuPanas || kipasDariWeb);
  digitalWrite(RELAY_PIN, kipasAktif ? RELAY_ON : RELAY_OFF);

  // ----------------------------------------------------
  // LOGIKA API / ALARM / SERVO (LOKAL + WEB)
  // ----------------------------------------------------
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
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    ledState = LOW;

    if (millis() - waktuTerakhirApi >= 3000) {
      if (posisiServo != SERVO_TUTUP) {
        gerakServoSmooth(posisiServo, SERVO_TUTUP);
        posisiServo = SERVO_TUTUP;
      }
    }
  }

  // ----------------------------------------------------
  // PRINT SERIAL MONITOR (Setiap 1 Detik)
  // ----------------------------------------------------
  if (millis() - waktuCetakSerial > 1000) {
    Serial.println("-----------------------------");
    if (isnan(suhu)) {
      Serial.println("Gagal membaca DHT22!");
    } else {
      Serial.printf("Suhu       : %.1f C\n", suhu);
      Serial.printf("Kelembapan : %.1f %%\n", kelembapan);
    }
    Serial.printf("Sensor Api : %s\n", apiTerdeteksi ? "API TERDETEKSI!" : "Aman");
    Serial.printf("Fan        : %s\n", kipasAktif ? "NYALA" : "MATI");
    Serial.printf("Status     : %s\n", alarmAktif ? "DARURAT API" : (millis() - waktuTerakhirApi < 3000 ? "TUNGGU AMAN" : "NORMAL"));
    
    waktuCetakSerial = millis();
  }

  // ----------------------------------------------------
  // KIRIM DATA KE FIREBASE (Setiap 2 Detik)
  // ----------------------------------------------------
  if (millis() - waktuKirimData > 2000) {
    if (Firebase.ready() && signupOK) {
      if (!isnan(suhu))      Firebase.RTDB.setFloat(&fbdo, "/Sensor/Suhu", suhu);
      if (!isnan(kelembapan)) Firebase.RTDB.setFloat(&fbdo, "/Sensor/Kelembapan", kelembapan);
      Firebase.RTDB.setBool(&fbdo, "/Sensor/Api", apiTerdeteksi);
      
      Firebase.RTDB.setBool(&fbdo, "/Status/Kipas", kipasAktif);
      Firebase.RTDB.setBool(&fbdo, "/Status/Alarm", alarmAktif);
    }
    waktuKirimData = millis();
  }

  delay(10); // Sedikit delay agar CPU tidak kepanasan
}