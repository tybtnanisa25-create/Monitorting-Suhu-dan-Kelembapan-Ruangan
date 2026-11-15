
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

// ========== WIFI ==========
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ========== HTTP ==========
const char* serverUrl = "http://httpbin.org/post";

// ========== DHT22 ==========
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ========== LCD ==========
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ========== PARAMETER S2 ==========
const int TOTAL_PESAN = 100;
const int PAYLOAD_SIZE = 500;

const unsigned long INTERVAL = 2000;
const unsigned long TIMEOUT = 5000;

// ========== VARIABEL STATISTIK ==========
int berhasil = 0;
int gagal = 0;

long rttData[TOTAL_PESAN];
int rssiData[TOTAL_PESAN];

int rttCount = 0;
int rssiCount = 0;

unsigned long lastLCDUpdate = 0;

// ========== LCD SUHU DAN KELEMBAPAN ==========
void updateLCD() {

  if (millis() - lastLCDUpdate >= 2000) {

    lastLCDUpdate = millis();

    float suhu = dht.readTemperature();
    float hum = dht.readHumidity();

    lcd.clear();

    if (isnan(suhu) || isnan(hum)) {
      lcd.setCursor(0, 0);
      lcd.print("Sensor Error");
      return;
    }

    lcd.setCursor(0, 0);
    lcd.print("Suhu: ");
    lcd.print(suhu, 1);
    lcd.write((uint8_t)223);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("Lembap: ");
    lcd.print(hum, 1);
    lcd.print("%");
  }
}

// ========== KONEKSI WIFI ==========
void connectWiFi() {

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Menghubungkan WiFi");

  lcd.clear();
  lcd.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");
}

// ========== MEMBUAT PAYLOAD 500 BYTE ==========
String createPayload(int sequence) {

  String payload = "S2|SEQ:" + String(sequence) + "|";

  while (payload.length() < PAYLOAD_SIZE) {
    payload += "X";
  }

  return payload;
}

// ========== MENGIRIM HTTP POST ==========
void sendHTTP(int sequence) {

  // Pengukuran RSSI setiap percobaan
  int rssi = WiFi.RSSI();
  rssiData[rssiCount++] = rssi;

  String payload = createPayload(sequence);

  WiFiClient wifiClient;
  HTTPClient http;

  http.setTimeout(TIMEOUT);
  http.setReuse(false);

  unsigned long startTime = millis();

  if (!http.begin(wifiClient, serverUrl)) {

    gagal++;

    Serial.print("SEQ: ");
    Serial.print(sequence);
    Serial.println(" | HTTP BEGIN GAGAL");

    return;
  }

  http.addHeader("Content-Type", "text/plain");
  http.addHeader("Connection", "close");

  int httpCode = http.POST(
    (uint8_t*)payload.c_str(),
    payload.length()
  );

  unsigned long rtt = millis() - startTime;

  if (httpCode >= 200 && httpCode < 300) {

    berhasil++;

    // RTT hanya untuk pesan berhasil
    rttData[rttCount++] = rtt;

    Serial.print("SEQ: ");
    Serial.print(sequence);
    Serial.print(" | HTTP: ");
    Serial.print(httpCode);
    Serial.print(" | RTT: ");
    Serial.print(rtt);
    Serial.print(" ms | RSSI: ");
    Serial.print(rssi);
    Serial.println(" dBm | BERHASIL");

  } else {

    gagal++;

    Serial.print("SEQ: ");
    Serial.print(sequence);
    Serial.print(" | HTTP: ");
    Serial.print(httpCode);
    Serial.print(" | RTT: ");
    Serial.print(rtt);
    Serial.print(" ms | RSSI: ");
    Serial.print(rssi);
    Serial.println(" dBm | GAGAL");
  }

  http.end();
}

// ========== SORTING RTT ==========
void sortRTT(long data[], int n) {

  for (int i = 0; i < n - 1; i++) {

    for (int j = 0; j < n - i - 1; j++) {

      if (data[j] > data[j + 1]) {

        long temp = data[j];
        data[j] = data[j + 1];
        data[j + 1] = temp;
      }
    }
  }
}

// ========== HASIL STATISTIK ==========
void printStatistics() {

  Serial.println();
  Serial.println("========== HASIL S2 ==========");

  Serial.print("Total pesan       : ");
  Serial.println(TOTAL_PESAN);

  Serial.print("Berhasil          : ");
  Serial.println(berhasil);

  Serial.print("Gagal             : ");
  Serial.println(gagal);

  float pdr = (float)berhasil / TOTAL_PESAN * 100.0;

  Serial.print("PDR               : ");
  Serial.print(pdr, 2);
  Serial.println(" %");

  // ===== STATISTIK RTT =====
  if (rttCount > 0) {

    long sortedRTT[TOTAL_PESAN];
    long totalRTT = 0;

    for (int i = 0; i < rttCount; i++) {
      sortedRTT[i] = rttData[i];
      totalRTT += rttData[i];
    }

    float mean = (float)totalRTT / rttCount;

    sortRTT(sortedRTT, rttCount);

    float median;

    if (rttCount % 2 == 0) {
      median = (sortedRTT[rttCount / 2 - 1] +
                sortedRTT[rttCount / 2]) / 2.0;
    } else {
      median = sortedRTT[rttCount / 2];
    }

    float variance = 0;

    for (int i = 0; i < rttCount; i++) {
      variance += pow(rttData[i] - mean, 2);
    }

    float stdDev = sqrt(variance / rttCount);

    int p95Index = (int)ceil(0.95 * rttCount) - 1;

    Serial.print("RTT Mean          : ");
    Serial.print(mean, 2);
    Serial.println(" ms");

    Serial.print("RTT Median        : ");
    Serial.print(median, 2);
    Serial.println(" ms");

    Serial.print("RTT Std Dev       : ");
    Serial.print(stdDev, 2);
    Serial.println(" ms");

    Serial.print("RTT P95           : ");
    Serial.print(sortedRTT[p95Index]);
    Serial.println(" ms");

  } else {

    Serial.println("RTT Mean          : Tidak ada data");
    Serial.println("RTT Median        : Tidak ada data");
    Serial.println("RTT Std Dev       : Tidak ada data");
    Serial.println("RTT P95           : Tidak ada data");
  }

  // ===== STATISTIK RSSI =====
  Serial.println("--------------------------------");
  Serial.println("STATISTIK RSSI");

  Serial.print("Jumlah pengukuran : ");
  Serial.println(rssiCount);

  if (rssiCount > 0) {

    long totalRSSI = 0;

    int minRSSI = rssiData[0];
    int maxRSSI = rssiData[0];

    for (int i = 0; i < rssiCount; i++) {

      totalRSSI += rssiData[i];

      if (rssiData[i] < minRSSI) {
        minRSSI = rssiData[i];
      }

      if (rssiData[i] > maxRSSI) {
        maxRSSI = rssiData[i];
      }
    }

    float meanRSSI = (float)totalRSSI / rssiCount;

    Serial.print("RSSI Mean         : ");
    Serial.print(meanRSSI, 2);
    Serial.println(" dBm");

    Serial.print("RSSI Minimum      : ");
    Serial.print(minRSSI);
    Serial.println(" dBm");

    Serial.print("RSSI Maximum      : ");
    Serial.print(maxRSSI);
    Serial.println(" dBm");
  }

  Serial.println("================================");
}

// ========== SETUP ==========
void setup() {

  Serial.begin(115200);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  dht.begin();

  connectWiFi();

  Serial.println();
  Serial.println("EKSPERIMEN S2 - HTTP POST");
  Serial.println("Payload: 500 byte | Pesan: 100 | Interval: 2 detik");
}

// ========== LOOP ==========
void loop() {

  static int sequence = 0;
  static unsigned long nextSend = 0;
  static bool finished = false;

  updateLCD();

  if (finished) {
    delay(100);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (sequence < TOTAL_PESAN && millis() >= nextSend) {

    sequence++;

    unsigned long startInterval = millis();

    sendHTTP(sequence);

    nextSend = startInterval + INTERVAL;

    if (nextSend < millis()) {
      nextSend = millis();
    }
  }

  if (sequence >= TOTAL_PESAN) {

    finished = true;

    printStatistics();
  }

  delay(5);
}
