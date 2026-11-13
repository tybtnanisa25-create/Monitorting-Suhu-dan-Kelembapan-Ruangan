#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <math.h>

// ================ WIFI ================
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ================ MQTT ================
const char* broker = "test.mosquitto.org";
const int mqttPort = 1883;

WiFiClient mqtt;

const char* clientId = "ESP32_S6_Telkom";
const char* topic = "telkom/s6/monitoring";

uint16_t packetId = 1;

// ================ SENSOR ================
#define DHTPIN 15
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================ PENGUJIAN S6 ================
const int TOTAL_PESAN = 100;
const int INTERVAL = 2000;
const unsigned long TIMEOUT = 5000;

const int PAYLOAD_SIZE = 500;

int berhasil = 0;
int gagal = 0;
int nomorPesan = 0;

float dataRTT[TOTAL_PESAN];

// ================ MQTT HELPER ================

// Membaca satu byte dari broker
int bacaByte(unsigned long timeout) {

  unsigned long mulai = millis();

  while (!mqtt.available()) {

    if (!mqtt.connected() ||
        millis() - mulai >= timeout) {
      return -1;
    }

    delay(1);
  }

  return mqtt.read();
}

// Membaca MQTT Remaining Length
int bacaRemainingLength() {

  int multiplier = 1;
  int value = 0;
  int encodedByte;

  do {

    encodedByte = bacaByte(3000);

    if (encodedByte < 0) return -1;

    value += (encodedByte & 127) * multiplier;
    multiplier *= 128;

  } while ((encodedByte & 128) != 0);

  return value;
}

// Menulis MQTT Remaining Length
int tulisRemainingLength(uint8_t* buffer, int value) {

  int jumlah = 0;

  do {

    uint8_t encodedByte = value % 128;
    value /= 128;

    if (value > 0) {
      encodedByte |= 128;
    }

    buffer[jumlah++] = encodedByte;

  } while (value > 0);

  return jumlah;
}

// ================ MQTT CONNECT ================

bool mqttConnect() {

  if (mqtt.connected()) return true;

  mqtt.stop();

  Serial.println("Menghubungkan MQTT...");

  if (!mqtt.connect(broker, mqttPort)) {

    Serial.println("Koneksi broker gagal");
    return false;
  }

  int idLength = strlen(clientId);

  int remainingLength = 10 + 2 + idLength;

  uint8_t packet[100];
  int pos = 0;

  // Fixed header
  packet[pos++] = 0x10;
  packet[pos++] = remainingLength;

  // Protocol Name: MQTT
  packet[pos++] = 0x00;
  packet[pos++] = 0x04;
  packet[pos++] = 'M';
  packet[pos++] = 'Q';
  packet[pos++] = 'T';
  packet[pos++] = 'T';

  // Protocol Level 4
  packet[pos++] = 0x04;

  // Clean Session
  packet[pos++] = 0x02;

  // Keep Alive 60 detik
  packet[pos++] = 0x00;
  packet[pos++] = 0x3C;

  // Client ID
  packet[pos++] = idLength >> 8;
  packet[pos++] = idLength & 0xFF;

  memcpy(packet + pos, clientId, idLength);
  pos += idLength;

  mqtt.write(packet, pos);

  // Membaca CONNACK
  int header = bacaByte(5000);

  if (header != 0x20) {

    Serial.println("Header CONNACK tidak valid");
    mqtt.stop();
    return false;
  }

  int length = bacaRemainingLength();

  if (length != 2) {

    Serial.println("Panjang CONNACK tidak valid");
    mqtt.stop();
    return false;
  }

  int ack1 = bacaByte(5000);
  int ack2 = bacaByte(5000);

  if (ack1 != 0 || ack2 != 0) {

    Serial.print("MQTT ditolak, return code: ");
    Serial.println(ack2);

    mqtt.stop();
    return false;
  }

  Serial.println("MQTT Connected!");

  return true;
}

// ================ MQTT PUBLISH QoS 1 ================

bool publishQoS1(
  const char* payload,
  uint16_t id,
  unsigned long &rtt
) {

  int topicLength = strlen(topic);
  int payloadLength = strlen(payload);

  // Remaining Length:
  // Topic Length + Topic + Packet ID + Payload
  int remainingLength =
    2 + topicLength + 2 + payloadLength;

  uint8_t packet[600];
  int pos = 0;

  // Fixed header: PUBLISH QoS 1
  packet[pos++] = 0x32;

  // Remaining Length, termasuk encoding 2 byte jika diperlukan
  pos += tulisRemainingLength(
    packet + pos,
    remainingLength
  );

  // Topic length
  packet[pos++] = topicLength >> 8;
  packet[pos++] = topicLength & 0xFF;

  // Topic
  memcpy(packet + pos, topic, topicLength);
  pos += topicLength;

  // Packet Identifier
  packet[pos++] = id >> 8;
  packet[pos++] = id & 0xFF;

  // Payload 500 byte
  memcpy(packet + pos, payload, payloadLength);
  pos += payloadLength;

  unsigned long waktuMulai = millis();

  mqtt.write(packet, pos);

  // Menunggu PUBACK maksimal 5 detik
  while (millis() - waktuMulai < TIMEOUT) {

    if (!mqtt.connected()) return false;

    if (mqtt.available()) {

      int header = bacaByte(100);

      if (header < 0) return false;

      int length = bacaRemainingLength();

      if (length < 0) return false;

      if ((header & 0xF0) == 0x40 && length == 2) {

        int ackId1 = bacaByte(100);
        int ackId2 = bacaByte(100);

        if (ackId1 < 0 || ackId2 < 0) return false;

        uint16_t receivedId =
          ((uint16_t)ackId1 << 8) | ackId2;

        if (receivedId == id) {

          rtt = millis() - waktuMulai;

          return true;
        }
      }

      else {

        // Lewati paket MQTT lain
        for (int i = 0; i < length; i++) {

          if (bacaByte(100) < 0) break;
        }
      }
    }

    delay(1);
  }

  return false;
}

// ================ PAYLOAD 500 BYTE ================

void buatPayload(char* payload, int seq) {

  float suhu = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(suhu)) suhu = 0;
  if (isnan(hum)) hum = 0;

  snprintf(
    payload,
    PAYLOAD_SIZE + 1,
    "SEQ:%03d|TEMP:%.1f|HUM:%.1f|",
    seq,
    suhu,
    hum
  );

  int panjang = strlen(payload);

  // Mengisi sisa payload sampai 500 byte
  while (panjang < PAYLOAD_SIZE) {

    payload[panjang++] = 'A';
  }

  payload[PAYLOAD_SIZE] = '\0';
}

// ================ STATISTIK ================

void tampilkanStatistik() {

  Serial.println("\n========== HASIL S6 ==========");

  Serial.print("Total pesan : ");
  Serial.println(TOTAL_PESAN);

  Serial.print("Berhasil    : ");
  Serial.println(berhasil);

  Serial.print("Gagal       : ");
  Serial.println(gagal);

  float pdr =
    (float)berhasil / TOTAL_PESAN * 100.0;

  Serial.print("PDR         : ");
  Serial.print(pdr, 2);
  Serial.println(" %");

  if (berhasil == 0) {

    Serial.println("Tidak ada data RTT.");
    return;
  }

  float data[TOTAL_PESAN];
  float jumlah = 0;

  for (int i = 0; i < berhasil; i++) {

    data[i] = dataRTT[i];
    jumlah += dataRTT[i];
  }

  // Mean
  float mean = jumlah / berhasil;

  // Mengurutkan data RTT
  for (int i = 0; i < berhasil - 1; i++) {

    for (int j = i + 1; j < berhasil; j++) {

      if (data[i] > data[j]) {

        float temp = data[i];
        data[i] = data[j];
        data[j] = temp;
      }
    }
  }

  // Median
  float median;

  if (berhasil % 2 == 0) {

    median =
      (data[berhasil / 2 - 1] +
       data[berhasil / 2]) / 2.0;

  } else {

    median = data[berhasil / 2];
  }

  // Standar deviasi populasi
  float totalKuadrat = 0;

  for (int i = 0; i < berhasil; i++) {

    totalKuadrat +=
      pow(dataRTT[i] - mean, 2);
  }

  float stdDev =
    sqrt(totalKuadrat / berhasil);

  // P95 nearest rank
  int indexP95 =
    (int)ceil(0.95 * berhasil) - 1;

  float p95 = data[indexP95];

  Serial.print("RTT Mean    : ");
  Serial.print(mean, 2);
  Serial.println(" ms");

  Serial.print("RTT Median  : ");
  Serial.print(median, 2);
  Serial.println(" ms");

  Serial.print("RTT Std Dev : ");
  Serial.print(stdDev, 2);
  Serial.println(" ms");

  Serial.print("RTT P95     : ");
  Serial.print(p95, 2);
  Serial.println(" ms");

  // Estimasi overhead paket MQTT
  int topicLength = strlen(topic);

  int remainingLength =
    2 + topicLength + 2 + PAYLOAD_SIZE;

  int remainingLengthBytes = 0;
  int temp = remainingLength;

  do {
    remainingLengthBytes++;
    temp /= 128;
  } while (temp > 0);

  int overheadBytes =
    1 + remainingLengthBytes + 2 + topicLength + 2;

  int totalBytes =
    PAYLOAD_SIZE + overheadBytes;

  float overheadPercent =
    (float)overheadBytes / totalBytes * 100.0;

  Serial.print("Overhead    : ");
  Serial.print(overheadBytes);
  Serial.println(" byte/pesan");

  Serial.print("Overhead %  : ");
  Serial.print(overheadPercent, 2);
  Serial.println(" %");

  Serial.println("==============================");

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("S6 SELESAI");

  lcd.setCursor(0, 1);
  lcd.print("PDR:");
  lcd.print(pdr, 1);
  lcd.print("%");
}

// ================ SETUP ================

void setup() {

  Serial.begin(115200);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  dht.begin();

  Serial.println("\nEKSPERIMEN S6 - MQTT QoS 1");
  Serial.println("Payload : 500 byte");
  Serial.println("Pesan   : 100");
  Serial.println("Interval: 2 detik");

  lcd.setCursor(0, 0);
  lcd.print("Eksperimen S6");

  // Koneksi WiFi
  WiFi.begin(ssid, password, 6);

  Serial.print("Menghubungkan WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.println(WiFi.localIP());

  // Koneksi MQTT
  if (!mqttConnect()) {

    Serial.println("Gagal memulai MQTT.");
    return;
  }

  Serial.println("\nMulai pengujian S6...\n");
}

// ================ LOOP ================

void loop() {

  if (nomorPesan >= TOTAL_PESAN) {

    if (nomorPesan == TOTAL_PESAN) {

      nomorPesan++;

      tampilkanStatistik();
    }

    return;
  }

  if (!mqtt.connected()) {

    if (!mqttConnect()) {

      delay(2000);
      return;
    }
  }

  nomorPesan++;

  char payload[PAYLOAD_SIZE + 1];

  buatPayload(payload, nomorPesan);

  // Tampilan LCD
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Suhu:");
  lcd.print(dht.readTemperature(), 1);

  lcd.setCursor(0, 1);
  lcd.print("Hum:");
  lcd.print(dht.readHumidity(), 1);

  unsigned long rtt = 0;

  uint16_t currentId = packetId++;

  if (packetId == 0) {
    packetId = 1;
  }

  bool sukses =
    publishQoS1(payload, currentId, rtt);

  if (sukses) {

    dataRTT[berhasil] = rtt;
    berhasil++;

    Serial.print("SEQ: ");
    Serial.print(nomorPesan);

    Serial.print(" | Payload: 500 byte");

    Serial.print(" | QoS: 1");

    Serial.print(" | RTT PUBACK: ");
    Serial.print(rtt);

    Serial.println(" ms | BERHASIL");

  } else {

    gagal++;

    Serial.print("SEQ: ");
    Serial.print(nomorPesan);

    Serial.println(" | TIMEOUT / GAGAL");
  }

  delay(INTERVAL);
}