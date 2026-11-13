#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <math.h>

// WIFI
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// SERVER HTTP
const char* server = "http://httpbin.org/post";

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// DHT22
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// PENGATURAN EKSPERIMEN
const int totalPesan = 100;
const unsigned long interval = 2000;
const unsigned long batasTimeout = 5000;

int berhasil = 0;
int gagal = 0;

unsigned long rttData[100];
int jumlahRTT = 0;

// Membuat payload tepat 500 byte
void buatPayload(char* payload, int seq,
                 float suhu, float hum) {

  char informasi[50];

  snprintf(informasi, sizeof(informasi),
           "SEQ:%03d|TEMP:%.1f|HUM:%.1f|",
           seq, suhu, hum);

  int panjangInfo = strlen(informasi);

  memcpy(payload, informasi, panjangInfo);

  // Mengisi sisa payload hingga 500 byte
  for (int i = panjangInfo; i < 500; i++) {
    payload[i] = 'A';
  }

  payload[500] = '\0';
}

// Menghitung statistik RTT
void hitungStatistik() {

  if (jumlahRTT == 0) {
    Serial.println("Tidak ada RTT valid.");
    return;
  }

  unsigned long dataUrut[100];

  for (int i = 0; i < jumlahRTT; i++) {
    dataUrut[i] = rttData[i];
  }

  // Sorting
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
    median = (dataUrut[jumlahRTT / 2 - 1] +
              dataUrut[jumlahRTT / 2]) / 2.0;
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

  // P95 nearest rank
  int indexP95 = (int)ceil(0.95 * jumlahRTT) - 1;
  double p95 = dataUrut[indexP95];

  double pdr = (double)berhasil / totalPesan * 100.0;

  Serial.println("\n======= HASIL S2 =======");
  Serial.print("Total pesan: ");
  Serial.println(totalPesan);

  Serial.print("Berhasil: ");
  Serial.println(berhasil);

  Serial.print("Gagal: ");
  Serial.println(gagal);

  Serial.print("PDR: ");
  Serial.print(pdr, 2);
  Serial.println("%");

  Serial.print("RTT Mean: ");
  Serial.print(mean, 2);
  Serial.println(" ms");

  Serial.print("RTT Median: ");
  Serial.print(median, 2);
  Serial.println(" ms");

  Serial.print("RTT Std Dev: ");
  Serial.print(stdDev, 2);
  Serial.println(" ms");

  Serial.print("RTT P95: ");
  Serial.print(p95, 2);
  Serial.println(" ms");

  Serial.println("========================");
}

void setup() {

  Serial.begin(115200);

  // LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  // Sensor
  dht.begin();

  lcd.setCursor(0, 0);
  lcd.print("Eksperimen S2");
  lcd.setCursor(0, 1);
  lcd.print("HTTP 500 Byte");

  Serial.println("================================");
  Serial.println("EKSPERIMEN S2 - HTTP POST");
  Serial.println("Payload : 500 byte");
  Serial.println("Pesan   : 100");
  Serial.println("Interval: 2 detik");
  Serial.println("================================");

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password, 6);

  lcd.clear();
  lcd.print("Connecting WiFi");

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

void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi terputus!");
    lcd.clear();
    lcd.print("WiFi Terputus");
    return;
  }

  for (int i = 1; i <= totalPesan; i++) {

    float suhu = dht.readTemperature();
    float hum = dht.readHumidity();

    if (isnan(suhu) || isnan(hum)) {
      Serial.println("DHT22 Error!");
      gagal++;
      delay(interval);
      continue;
    }

    // LCD
    lcd.setCursor(0, 0);
    lcd.print("Suhu: ");
    lcd.print(suhu, 1);
    lcd.print((char)223);
    lcd.print("C   ");

    lcd.setCursor(0, 1);
    lcd.print("Hum: ");
    lcd.print(hum, 1);
    lcd.print("%    ");

    // Payload 500 byte
    char payload[501];

    buatPayload(payload, i, suhu, hum);

    HTTPClient http;
    http.begin(server);
    http.addHeader("Content-Type", "text/plain");
    http.setTimeout(batasTimeout);

    unsigned long waktuMulai = millis();

    int responseCode = http.POST(
      (uint8_t*)payload, 500
    );

    unsigned long waktuSelesai = millis();

    unsigned long rtt = waktuSelesai - waktuMulai;

    String response = "";

    if (responseCode > 0) {
      response = http.getString();
    }

    char token[16];

    snprintf(token, sizeof(token), "SEQ:%03d", i);

    bool pesanValid =
      responseCode >= 200 &&
      responseCode < 300 &&
      response.indexOf(token) >= 0 &&
      rtt <= batasTimeout;

    Serial.print("SEQ: ");
    Serial.print(i);

    Serial.print(" | Payload: 500 byte");

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

    if (i < totalPesan) {
      delay(interval);
    }
  }

  hitungStatistik();

  lcd.clear();
  lcd.print("S2 Selesai");
  lcd.setCursor(0, 1);
  lcd.print("PDR: ");
  lcd.print((float)berhasil / totalPesan * 100, 1);
  lcd.print("%");

  while (true) {
    delay(1000);
  }
}