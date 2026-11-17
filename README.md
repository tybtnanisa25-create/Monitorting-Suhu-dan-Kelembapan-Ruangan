1. Monitoring Suhu dan Kelembapan Ruangan
   Repository ini merupakan dokumentasi proyek Internet of Things (IoT) untuk monitoring suhu dan kelembapan ruangan menggunakan ESP32 dan sensor DHT11.

  Proyek ini juga mencakup pengujian komunikasi data menggunakan protokol HTTP POST dan MQTT dengan variasi ukuran payload dan Quality of Service (QoS). Pengujian dilakukan untuk             mengetahui dan membandingkan performa komunikasi pada setiap skenario.

2. Tujuan Proyek
- Merancang sistem monitoring suhu dan kelembapan ruangan menggunakan ESP32 dan DHT11.
- Mengimplementasikan pengiriman data menggunakan HTTP POST dan MQTT.
- Melakukan pengujian komunikasi dengan variasi ukuran payload 20 byte dan 500 byte.
- Membandingkan performa komunikasi berdasarkan hasil pengujian.
- Mengolah dan menganalisis data hasil eksperimen.

3. Perangkat dan Teknologi
   Perangkat dan teknologi yang digunakan dalam proyek ini meliputi:

- ESP32 DevKit V1 sebagai mikrokontroler.
- Sensor DHT11 untuk membaca suhu dan kelembapan.
- Wokwi sebagai platform simulasi.
- Arduino C++ sebagai bahasa pemrograman.
- HTTP POST dan MQTT sebagai protokol komunikasi.
- Wireshark untuk analisis paket jaringan.
- Microsoft Excel untuk pengolahan dan visualisasi data.

4. Skenario Pengujian
   Pengujian komunikasi dilakukan dalam enam skenario:

(1) S1 – HTTP POST (20 Byte)
Pengiriman data menggunakan protokol HTTP POST dengan ukuran payload 20 byte.
(2) S2 – HTTP POST (500 Byte)
Pengiriman data menggunakan protokol HTTP POST dengan ukuran payload 500 byte.
(3) S3 – MQTT QoS 0 (20 Byte)
Pengiriman data menggunakan MQTT dengan QoS 0 dan ukuran payload 20 byte.
(4) S4 – MQTT QoS 0 (500 Byte)
Pengiriman data menggunakan MQTT dengan QoS 0 dan ukuran payload 500 byte.
(5) S5 – MQTT QoS 1 (20 Byte)
Pengiriman data menggunakan MQTT dengan QoS 1 dan ukuran payload 20 byte.
(6) S6 – MQTT QoS 1 (500 Byte)
Pengiriman data menggunakan MQTT dengan QoS 1 dan ukuran payload 500 byte.

  Setiap skenario menggunakan 100 pesan dengan interval pengiriman 2 detik.

5. Parameter Pengujian
   Parameter yang digunakan untuk menganalisis hasil eksperimen meliputi:

- Round Trip Time (RTT): waktu komunikasi berdasarkan mekanisme pengukuran pada setiap protokol.
- Packet Delivery Ratio (PDR): persentase pesan yang berhasil dikirim.
- Mean RTT: niai rata-rata RTT.
- Median RTT: nilai tengah dari pengukuran RTT.
- Standard Deviation: tingkat penyebaran nilai RTT.
- 95th Percentile (P95): nilai RTT pada persentil ke-95.
- RSSI: kekuatan sinyal Wi-Fi yang diterima ESP32.

6. Dokumentasi
   Seluruh data dan hasil eksperimen disimpan dalam repository ini, yang mencakup:

- Data mentah hasil pengujian dalam format CSV.
- Hasil pengolahan data menggunakan Microsoft Excel.
- Grafik perbandingan performa setiap skenario.
- Dokumentasi proses pengujian dan analisis paket menggunakan Wireshark.

7. Hasil dan Analisis

Hasil dari enam skenario pengujian digunakan untuk membandingkan performa HTTP POST dan MQTT berdasarkan variasi ukuran payload serta mekanisme QoS.
Analisis dilakukan dengan memperhatikan parameter RTT, PDR, dan RSSI untuk mengetahui karakteristik komunikasi dari masing-masing skenario.
