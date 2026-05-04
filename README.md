# 🏠 Smart Room Safety System — IoT Berbasis NodeMCU (ESP32)

**Kelompok 4 | Laboratorium Internet of Things | Fakultas Informatika | Universitas Telkom 2026**

| Anggota | NIM |
|---|---|
| Aliyah Rahma | 103032300126 |
| Galih Samudra Pinarindra | 103032300079 |
| Yuhen Aditya Gunawan | 103032530022 |
| Arina Rahmania Nabila | 103032300129 |

---

## 📋 Deskripsi Proyek

Sistem keamanan dan pemantauan ruangan pintar berbasis **Internet of Things (IoT)** menggunakan **ESP32** yang mampu:

- 🌡️ Monitoring suhu ruangan secara **real-time** menggunakan sensor DHT22
- 🔥 Mendeteksi keberadaan api menggunakan **Flame Sensor**
- 💨 Mengaktifkan **kipas (relay)** secara otomatis saat suhu melebihi 32°C
- 🔔 Mengaktifkan **buzzer** dan **membuka jendela (servo)** saat api terdeteksi
- 📊 Menampilkan semua data pada **Web Dashboard** secara real-time via Firebase

---

## 🗂️ Struktur Folder

```
TUBES/
├── frontend/                   # Web Dashboard (HTML, CSS, JS)
│   ├── index.html
│   ├── style.css
│   └── script.js
├── smart_room_firebase.ino     # Kode ESP32 (Arduino IDE) dengan Firebase
├── Template_Proposal_TUBES_NodeMCU.pdf
└── README.md
```

---

## ⚙️ Komponen Hardware

| Komponen | Keterangan |
|---|---|
| ESP32 Dev Module | Mikrokontroler utama + WiFi |
| Sensor DHT22 | Membaca suhu & kelembaban |
| Flame Sensor | Mendeteksi api |
| Fan 12V + Relay | Kipas pendingin ruangan |
| Motor Servo | Membuka/menutup jendela |
| Buzzer | Alarm peringatan kebakaran |
| Breadboard + Jumper | Rangkaian prototipe |

---

## 🔌 Konfigurasi Pin ESP32

| Pin ESP32 | Komponen |
|---|---|
| GPIO 4 | DHT22 (Data) |
| GPIO 34 | Flame Sensor |
| GPIO 18 | Servo Motor |
| GPIO 26 | Relay (Kipas) |
| GPIO 14 | LED Indikator |
| GPIO 27 | Buzzer |

---

## ☁️ Firebase Realtime Database Structure

```
smartroomiot/
├── Sensor/
│   ├── Suhu: 27.3
│   ├── Kelembapan: 55.2
│   └── Api: false
├── Status/
│   ├── Kipas: false
│   └── Alarm: false
└── Kontrol/
    ├── Kipas: false
    └── Alarm: false
```

---

## 🚀 Cara Menjalankan Web Dashboard

```bash
cd frontend
python -m http.server 8000
```
Buka browser: **http://localhost:8000**

---

## 🛠️ Library Arduino yang Digunakan

- `DHT sensor library` by Adafruit
- `ESP32Servo`
- `Firebase ESP Client` by Mobizt
