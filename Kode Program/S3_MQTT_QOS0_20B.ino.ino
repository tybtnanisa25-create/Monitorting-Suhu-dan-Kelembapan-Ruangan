#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <math.h>

// ================= WIFI =================
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ================= MQTT =================
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

char clientId[40];
char topic[100];

// ================= SENSOR =================
#define DHTPIN 15
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= PENGUJIAN S3 =================
const int TOTAL_PESAN = 100;
const int INTERVAL = 2000;
const unsigned long TIMEOUT = 5000;

int nomorPesan = 0;
int berhasil = 0;
int gagal = 0;

bool menungguBalasan = false;
bool pesanDiterima = false;

unsigned long waktuKirim = 0;
unsigned long waktuMulai = 0;
unsigned long waktuPengirimanTerakhir = 0;

float dataRTT[TOTAL_PESAN];

// ================= CALLBACK MQTT =================
void callback(char* topicReceived, byte* payload, unsigned int length) {

  char pesan[100];

  if (length >= sizeof(pesan)) return;

  memcpy(pesan, payload, length);
  pesan[length] = '\0';

  // Pesan harus diawali dengan S
  if (pesan[0] != 'S') return;

  int seq = atoi(pesan + 1);

  if (menungguBalasan && seq == nomorPesan) {

    unsigned long rtt = millis() - waktuKirim;

    dataRTT[berhasil] = rtt;
    berhasil++;

    pesanDiterima = true;
    menungguBalasan = false;

    Serial.print("SEQ: ");
    Serial.print(seq);
    Serial.print(" | Payload: ");
    Serial.print(length);
    Serial.print(" byte | RTT: ");
    Serial.print(rtt);
    Serial.println(" ms | BERHASIL");
  }
}

// ================= KONEKSI WIFI =================
void koneksiWiFi() {

  Serial.print("Menghubungkan WiFi");

  WiFi.begin(ssid, password, 6);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// ================= KONEKSI MQTT =================
void koneksiMQTT() {

  while (!client.connected()) {

    Serial.print("Menghubungkan MQTT...");

    if (client.connect(clientId)) {

      Serial.println("Connected!");

      client.subscribe(topic, 0);

      Serial.print("Subscribe topic: ");
      Serial.println(topic);

    } else {

      Serial.print("Gagal, rc=");
      Serial.println(client.state());

      delay(2000);
    }
  }
}

// ================= MEMBUAT PAYLOAD 20 BYTE =================
void buatPayload(char* payload, int seq) {

  float suhu = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(suhu)) suhu = 0;
  if (isnan(hum)) hum = 0;

  snprintf(payload, 21, "S%03d|T%05.1f|H%05.1f",
           seq, suhu, hum);

  // Tambahkan underscore sampai tepat 20 byte
  int panjang = strlen(payload);

  while (panjang < 20) {
    payload[panjang] = '_';
    panjang++;
  }

  payload[20] = '\0';
}

// ================= STATISTIK =================
void tampilkanStatistik() {

  Serial.println("\n========== HASIL S3 ==========");

  Serial.print("Total pesan : ");
  Serial.println(TOTAL_PESAN);

  Serial.print("Berhasil    : ");
  Serial.println(berhasil);

  Serial.print("Gagal       : ");
  Serial.println(gagal);

  float pdr = (float)berhasil / TOTAL_PESAN * 100.0;

  Serial.print("PDR         : ");
  Serial.print(pdr, 2);
  Serial.println(" %");

  if (berhasil == 0) {
    Serial.println("Tidak ada data RTT yang berhasil.");
    return;
  }

  float data[TOTAL_PESAN];
  float jumlah = 0;

  for (int i = 0; i < berhasil; i++) {
    data[i] = dataRTT[i];
    jumlah += dataRTT[i];
  }

  float mean = jumlah / berhasil;

  // Urutkan data RTT
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
    median = (data[berhasil / 2 - 1] +
              data[berhasil / 2]) / 2.0;
  } else {
    median = data[berhasil / 2];
  }

  // Standar deviasi populasi
  float totalKuadrat = 0;

  for (int i = 0; i < berhasil; i++) {
    totalKuadrat += pow(dataRTT[i] - mean, 2);
  }

  float stdDev = sqrt(totalKuadrat / berhasil);

  // P95 metode nearest rank
  int indexP95 = (int)ceil(0.95 * berhasil) - 1;
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

  Serial.println("==============================");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("S3 SELESAI");
  lcd.setCursor(0, 1);
  lcd.print("PDR:");
  lcd.print(pdr, 1);
  lcd.print("%");
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  dht.begin();

  lcd.setCursor(0, 0);
  lcd.print("Eksperimen S3");

  Serial.println("\nEKSPERIMEN S3 - MQTT QoS 0");
  Serial.println("Payload : 20 byte");
  Serial.println("Pesan   : 100");
  Serial.println("Interval: 2 detik");

  koneksiWiFi();

  // Membuat client ID unik berdasarkan MAC ESP32
  String mac = WiFi.macAddress();
  mac.replace(":", "");

  snprintf(clientId, sizeof(clientId),
           "ESP32_S3_%s", mac.c_str());

  snprintf(topic, sizeof(topic),
           "telkom/s3/%s/echo", mac.c_str());

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(512);
  client.setKeepAlive(60);

  koneksiMQTT();

  Serial.println("\nMulai pengujian S3...\n");
}

// ================= LOOP =================
void loop() {

  if (nomorPesan >= TOTAL_PESAN) {
    if (nomorPesan == TOTAL_PESAN) {
      nomorPesan++;
      tampilkanStatistik();
    }
    return;
  }

  if (!client.connected()) {
    koneksiMQTT();
  }

  client.loop();

  if (millis() - waktuPengirimanTerakhir < INTERVAL &&
      nomorPesan > 0) {
    return;
  }

  nomorPesan++;

  char payload[21];
  buatPayload(payload, nomorPesan);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Suhu:");
  lcd.print(dht.readTemperature(), 1);

  lcd.setCursor(0, 1);
  lcd.print("Hum:");
  lcd.print(dht.readHumidity(), 1);

  pesanDiterima = false;
  menungguBalasan = true;

  waktuKirim = millis();
  waktuPengirimanTerakhir = waktuKirim;

  bool terkirim = client.publish(topic, payload, false);

  if (!terkirim) {
    menungguBalasan = false;
    gagal++;

    Serial.print("SEQ: ");
    Serial.print(nomorPesan);
    Serial.println(" | GAGAL PUBLISH");

    return;
  }

  // Menunggu pesan echo maksimal 5 detik
  while (!pesanDiterima &&
         millis() - waktuKirim < TIMEOUT) {

    client.loop();
    delay(1);
  }

  if (!pesanDiterima) {

    menungguBalasan = false;
    gagal++;

    Serial.print("SEQ: ");
    Serial.print(nomorPesan);
    Serial.println(" | TIMEOUT | GAGAL");
  }
}
