#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <math.h>

// =====================================
// 1. WIFI WOKWI
// =====================================
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// =====================================
// 2. SERVER HTTP
// =====================================
const char* server = "http://httpbin.org/post";

// =====================================
// 3. LCD I2C
// =====================================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================
// 4. SENSOR DHT22
// =====================================
#define DHTPIN 15
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

// =====================================
// 5. PENGATURAN EKSPERIMEN
// =====================================
const int totalPesan = 100;
const unsigned long interval = 2000;
const unsigned long batasTimeout = 5000;

int berhasil = 0;
int gagal = 0;

unsigned long rttData[100];
int jumlahRTT = 0;

// =====================================
// 6. FUNGSI STATISTIK RTT
// =====================================
void hitungStatistik() {

  if (jumlahRTT == 0) {
    Serial.println("Tidak ada data RTT valid.");
    return;
  }

  // Mengurutkan RTT
  unsigned long dataUrut[100];

  for (int i = 0; i < jumlahRTT; i++) {
    dataUrut[i] = rttData[i];
  }

  for (int i = 0; i < jumlahRTT - 1; i++) {
    for (int j = 0; j < jumlahRTT - i - 1; j++) {
      if (dataUrut[j] > dataUrut[j + 1]) {
        unsigned long temp = dataUrut[j];
        dataUrut[j] = dataUrut[j + 1];
        dataUrut[j + 1] = temp;
      }
    }
  }

  // Mean
  double total = 0;

  for (int i = 0; i < jumlahRTT; i++) {
    total += rttData[i];
  }

  double mean = total / jumlahRTT;

  // Median
  double median;

  if (jumlahRTT % 2 == 0) {
    median = (
      dataUrut[jumlahRTT / 2 - 1] +
      dataUrut[jumlahRTT / 2]
    ) / 2.0;
  } else {
    median = dataUrut[jumlahRTT / 2];
  }

  // Standar deviasi populasi
  double jumlahKuadrat = 0;

  for (int i = 0; i < jumlahRTT; i++) {
    double selisih = rttData[i] - mean;
    jumlahKuadrat += selisih * selisih;
  }

  double stdDev = sqrt(jumlahKuadrat / jumlahRTT);

  // P95 (nearest rank)
  int indexP95 = (int)ceil(0.95 * jumlahRTT) - 1;
  double p95 = dataUrut[indexP95];

  // PDR
  double pdr = (double)berhasil / totalPesan * 100.0;

  // Menampilkan hasil
  Serial.println();
  Serial.println("========== HASIL S1 ==========");

  Serial.print("Total pesan       : ");
  Serial.println(totalPesan);

  Serial.print("Berhasil          : ");
  Serial.println(berhasil);

  Serial.print("Gagal              : ");
  Serial.println(gagal);

  Serial.print("PDR                : ");
  Serial.print(pdr, 2);
  Serial.println(" %");

  Serial.print("RTT Mean           : ");
  Serial.print(mean, 2);
  Serial.println(" ms");

  Serial.print("RTT Median         : ");
  Serial.print(median, 2);
  Serial.println(" ms");

  Serial.print("RTT Std Dev        : ");
  Serial.print(stdDev, 2);
  Serial.println(" ms");

  Serial.print("RTT P95             : ");
  Serial.print(p95, 2);
  Serial.println(" ms");

  Serial.println("==============================");
}

// =====================================
// 7. SETUP
// =====================================
void setup() {

  Serial.begin(115200);

  // Inisialisasi LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  // Inisialisasi sensor
  dht.begin();

  lcd.setCursor(0, 0);
  lcd.print("IoT Experiment");
  lcd.setCursor(0, 1);
  lcd.print("S1 HTTP POST");

  Serial.println("================================");
  Serial.println("EKSPERIMEN S1 - HTTP POST");
  Serial.println("Payload : 20 byte");
  Serial.println("Pesan   : 100");
  Serial.println("Interval: 2 detik");
  Serial.println("================================");

  // Koneksi WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password, 6);

  lcd.clear();
  lcd.print("Connecting WiFi");

  Serial.print("Menghubungkan WiFi");

  unsigned long mulaiWiFi = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - mulaiWiFi < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi gagal terhubung!");

    lcd.clear();
    lcd.print("WiFi Gagal!");
    return;
  }

  Serial.println("WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  lcd.clear();
  lcd.print("WiFi Connected");
  delay(1500);
}

// =====================================
// 8. LOOP EKSPERIMEN S1
// =====================================
void loop() {

  // Pastikan WiFi terhubung
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi terputus. Eksperimen dihentikan.");

    lcd.clear();
    lcd.print("WiFi Terputus");
    return;
  }

  for (int i = 1; i <= totalPesan; i++) {

    // Membaca sensor
    float suhu = dht.readTemperature();
    float hum = dht.readHumidity();

    if (isnan(suhu) || isnan(hum)) {
      Serial.println("Pembacaan DHT22 gagal.");
      gagal++;
      delay(interval);
      continue;
    }

    // Menampilkan data pada LCD
    lcd.setCursor(0, 0);
    lcd.print("Suhu: ");
    lcd.print(suhu, 1);
    lcd.print((char)223);
    lcd.print("C   ");

    lcd.setCursor(0, 1);
    lcd.print("Hum: ");
    lcd.print(hum, 1);
    lcd.print("%    ");

    // Membuat payload tepat 20 byte
    char token[32];

    snprintf(
      token,
      sizeof(token),
      "S%04dT%03dH%03d",
      i,
      (int)round(suhu),
      (int)round(hum)
    );

    String payload = token;

    while (payload.length() < 20) {
      payload += " ";
    }

    // Memulai HTTP POST
    HTTPClient http;

    http.begin(server);
    http.addHeader("Content-Type", "text/plain");
    http.setTimeout(batasTimeout);

    unsigned long waktuMulai = millis();

    int responseCode = http.POST(
      (uint8_t*)payload.c_str(),
      payload.length()
    );

    unsigned long waktuSelesai = millis();

    unsigned long rtt = waktuSelesai - waktuMulai;

    String response = "";

    if (responseCode > 0) {
      response = http.getString();
    }

    // Memeriksa respons dan sequence number
    bool pesanValid =
      responseCode >= 200 &&
      responseCode < 300 &&
      response.indexOf(token) >= 0 &&
      rtt <= batasTimeout;

    Serial.print("SEQ: ");
    Serial.print(i);

    Serial.print(" | Payload: ");
    Serial.print(payload.length());
    Serial.print(" byte");

    Serial.print(" | HTTP: ");
    Serial.print(responseCode);

    Serial.print(" | RTT: ");
    Serial.print(rtt);
    Serial.print(" ms");

    if (pesanValid) {

      berhasil++;

      rttData[jumlahRTT] = rtt;
      jumlahRTT++;

      Serial.println(" | BERHASIL");

    } else {

      gagal++;

      Serial.println(" | GAGAL / HILANG");
    }

    http.end();

    // Jeda antar pesan
    if (i < totalPesan) {
      delay(interval);
    }
  }

  // Menghitung statistik setelah 100 pesan
  hitungStatistik();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("S1 Selesai");
  lcd.setCursor(0, 1);
  lcd.print("PDR: ");
  lcd.print((float)berhasil / totalPesan * 100, 1);
  lcd.print("%");

  // Mencegah pengujian diulang
  while (true) {
    delay(1000);
  }
}
