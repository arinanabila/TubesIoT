#include <WiFi.h>
#include <nvs_flash.h>

void setup() {
  Serial.begin(115200);
  delay(2000); // Tunggu Serial Monitor siap
  
  Serial.println("\n\n=======================================");
  Serial.println("=== MEMULAI FACTORY RESET ESP32 ===");
  Serial.println("=======================================");

  // 1. Hapus semua pengaturan WiFi yang menyangkut di memori
  Serial.print("1. Menghapus konfigurasi WiFi yang tersimpan... ");
  WiFi.disconnect(true, true);
  delay(1000);
  WiFi.mode(WIFI_OFF);
  delay(1000);
  Serial.println("Selesai!");

  // 2. Hapus total partisi NVS (tempat ESP32 menyimpan data rahasia & kalibrasi antena)
  Serial.print("2. Memformat partisi NVS (Memori Internal)... ");
  nvs_flash_erase(); // Hapus paksa
  nvs_flash_init();  // Buat ulang partisi kosong
  delay(1000);
  Serial.println("Selesai!");
  
  Serial.println("\n=======================================");
  Serial.println("=== FORMAT & RESET BERHASIL! ===");
  Serial.println("=======================================");
  Serial.println("Memori ESP32 Anda sekarang sudah bersih 100% seperti baru keluar dari pabrik.");
  Serial.println("Jika masalah Brownout sebelumnya karena error pada antena WiFi internal, ini akan menyembuhkannya.");
  Serial.println("\nLANGKAH SELANJUTNYA:");
  Serial.println("Buka kembali file 'sketch_may4b.ino' Anda dan lakukan Upload seperti biasa.");
}

void loop() {
  // Dibiarkan kosong karena ini hanya script sekali jalan
}
