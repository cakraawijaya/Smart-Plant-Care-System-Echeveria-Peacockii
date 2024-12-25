// Blynk
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "YOUR_BLYNK_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "YOUR_BLYNK_TEMPLATE_NAME"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

// Pustaka yang digunakan
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <CTBot.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP_FC28.h>

// Koneksi
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define BOTtoken "YOUR_API_BOT_TOKEN"
#define KodeBot "ECHEVERIA2022"
#define SERIAL_DEBUG_BAUD 115200
BlynkTimer timer;

// Sensor
#define LDR_PIN 35 // Pin Antarmuka Sensor LDR
#define FC28_PIN 34 // Pin Antarmuka Sensor FC-28
FC28Sensor fc28(FC28_PIN); // Konstruktor FC28Sensor -> fc28
#define DHT_PIN 13 // Pin Antarmuka Sensor DHT
#define DHT_TYPE DHT22 // Tipe Sensor DHT -> DHT22
DHT dht(DHT_PIN, DHT_TYPE); // Konstruktor DHT -> dht

// Aktuator
#define RPOMPA1_PIN 2 // Pin Antarmuka Pompa Air 1
#define RPOMPA2_PIN 4 // Pin Antarmuka Pompa Air 2

// Layar
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Variabel untuk keperluan bot telegram
CTBot myBot; CTBotInlineKeyboard InlineKey, InlineOption, InlineKeyNULL;
#define InlineMenu1 "InlineMonitoringSuhuUdara"
#define InlineMenu2 "InlineMonitoringKelembabanUdara"
#define InlineMenu3 "InlineMonitoringKelembabanTanah"
#define InlineMenu4 "InlineMonitoringIntensitasCahaya"
#define InlineMenu5 "InlineControllingPenyiramanAir"
bool viewTombol;
String sendMsg, msg1, msg2; 

// Variabel untuk keperluan aktuator
bool relayON = HIGH; bool relayOFF = LOW; // Jika anda menggunakan NO pada relay maka yang terjadi adalah Active Low, sedangkan jika anda menggunakan NC pada relay maka yang terjadi adalah Active High
int manualPumpControl; // Variabel ini difungsikan untuk menampung perintah yaitu antara 1 (ON) dan 0 (OFF) yang di dapat dari Blynk

// Variabel untuk keperluan sensor
int adcLDR = 0, old_lux = 0, lux; const float R_FIXED = 10.0, calibrationValue = 1.2; float volt, resistance; String status_sinar, info_intensitas_cahaya; // LDR
float old_temp = 0, temp; int old_hum = 0, hum; String status_udara, info_suhu_udara, info_kelembaban_udara; // DHT
int old_moisture = 0, moisture; String status_tanah, info_kelembaban_tanah; // FC-28

// Kontrol air untuk menyiram tanaman echeveria
BLYNK_WRITE(V6) {
  manualPumpControl = param.asInt();
  if (manualPumpControl == 1) {
    Blynk.virtualWrite(V5, 1); // Indikator Pompa 2: menyala
    Serial.println("Status kontrol air : On");
    Serial.println("<------------------------------->\n");
    digitalWrite(RPOMPA2_PIN, relayON); // Pompa 2 menyala
  } 
  if (manualPumpControl == 0){
    Blynk.virtualWrite(V5, 0); // Indikator Pompa 2: mati
    Serial.println("Status kontrol air : Off");
    Serial.println("<------------------------------->\n");
    digitalWrite(RPOMPA2_PIN, relayOFF); // Pompa 2 mati
  }
}

// Method untuk menyambungkan ke Bot Telegram
void connectBot() {
  myBot.setTelegramToken(BOTtoken); // Mengatur token bot telegram
  myBot.wifiConnect(WIFI_SSID, WIFI_PASSWORD); // Mengatur konektivitas jaringan bot telegram
  myBot.setMaxConnectionRetries(5); // Bot telegram dapat menyambungkan ulang ke WiFi yang diatur sebanyak 5x sebelum layanan dihentikan
  Serial.println("\nMenghubungkan ke: echeveria_bot..."); // Cetak ke serial monitor

  if(myBot.testConnection()){ // Jika bot telegram tersambung ke jaringan maka cetak ke serial monitor :
    Serial.println("\n=========================================");
    Serial.println("Bot telegram berhasil tersambung ...[SUKSES]"); 
    Serial.println("=========================================\n");
  } else{ // Jika bot telegram tidak tersambung ke jaringan maka cetak ke serial monitor :
    Serial.print("Bot telegram gagal tersambung\nMenyambungkan kembali\n"); 
    while (!myBot.testConnection()){ // Selama bot telegram tidak tersambung ke jaringan maka cetak ke serial monitor :
      Serial.print("."); delay(500);
    } Serial.println();
  }
}

// Method untuk membaca sensor
void readSensor(){
  // Mengukur nilai temperatur udara
  temp = dht.readTemperature();

  // Cek perubahan nilai temperatur udara
  if(temp != old_temp){  
    Serial.println("Suhu Udara: "+String(temp)+"°C");
    lcd.clear(); lcd.backlight(); lcd.setCursor(1,0); lcd.print("Suhu Udara:"); 
    lcd.setCursor(1,1); lcd.print(""+String(temp, 2)+" "+String((char)223)+"C"); delay(1000);
    sendData(float(temp, 2),int(old_hum),int(old_moisture),int(old_lux));
    old_temp = temp; 
  }

  // Mengukur nilai kelembaban udara
  hum = dht.readHumidity(); 

  // Cek perubahan nilai kelembaban udara
  if(hum != old_hum){ 
    Serial.println("Kelembaban Udara: "+String(hum)+"%");  
    lcd.clear(); lcd.backlight(); lcd.setCursor(1,0); lcd.print("Kelem.Udara:"); 
    lcd.setCursor(1,1); lcd.print(""+String(hum)+" %"); delay(1000);
    sendData(float(old_temp, 2),int(hum),int(old_moisture),int(old_lux));
    old_hum = hum; 
  }

  // Mengukur nilai kelembaban tanah
  fc28.calibration(7); // 7 => agar pembacaan sensor fc28 mendekati benar (diisi bebas)
  moisture = fc28.getSoilMoisture(); 

  // Cek perubahan nilai kelembaban tanah
  if(moisture != old_moisture){ 
    Serial.println("Kelembaban Tanah: "+String(moisture)+"%");
    lcd.clear(); lcd.backlight(); lcd.setCursor(1,0); lcd.print("Kelem.Tanah:"); 
    lcd.setCursor(1,1); lcd.print(""+String(moisture)+" %"); delay(1000);
    sendData(float(old_temp, 2),int(old_hum),int(moisture),int(old_lux));
    old_moisture = moisture; 
  }
  
  // Mengukur nilai intensitas cahaya
  adcLDR = analogRead(LDR_PIN); // Baca ADC Sensor LDR
  volt = (adcLDR / 4095.0) * 5; // ESP bit=12 -> 4095, 5=Tegangan Referensi
  resistance = (5 * R_FIXED / volt) - R_FIXED; // Menghitung Resistansi Cahaya

  // Menghitung nilai lux
  lux = 500 / pow(resistance / 1000, calibrationValue);
  if(lux >= 100000){ lux = 100000; }
  if(lux < 0){ lux = 0; }

  // Cek perubahan nilai lux
  if(lux != old_lux){ 
    Serial.println("Intensitas Cahaya: "+String(lux)+"lux");
    lcd.clear(); lcd.backlight(); lcd.setCursor(1,0); lcd.print("Int.Cahaya:"); 
    lcd.setCursor(1,1); lcd.print(""+String(lux)+" lx"); delay(1000);
    sendData(float(old_temp, 2),int(old_hum),int(old_moisture),int(lux));
    old_lux = lux; 
  }

  autoPumpControl(); // Memanggil method autoPumpControl
}

// Method untuk mengendalikan pompa otomatis
void autoPumpControl(){
  // Jika suhu udara rendah / kelembaban tinggi / intensitas cahaya rendah / tanah basah, maka :
  if (temp >= 0 && temp < 16 || hum > 90 && hum <= 100 || lux >= 0 && lux < 200 || moisture >= 60 && moisture <= 100) {
    if (temp >= 0 && temp < 16) { 
      info_suhu_udara = "Suhu Udara: Rendah";                   // Dingin
      status_udara = "Status Kualitas Udara: Bahaya";           // Status Udara: Bahaya
    }
    if (hum > 90 && hum <= 100) {
      info_kelembaban_udara = "Kelembaban Udara: Tinggi";       // Basah
      status_udara = "Status Kualitas Udara: Bahaya";           // Status Udara: Bahaya
    }
    if (lux >= 0 && lux < 200) {
      info_intensitas_cahaya = "Intensitas Cahaya: Rendah";     // Gelap
      status_sinar = "Status Kualitas Sinar: Aman";             // Status Sinar: Aman
    }
    if (moisture >= 60 && moisture <= 100){
      info_kelembaban_tanah = "Kelembaban Tanah: Tinggi";       // Basah
      status_tanah = "Status Kualitas Tanah: Bahaya";           // Status Tanah: Bahaya
    }    
    Blynk.virtualWrite(V4, 0);                                  // Indikator Pompa 1: mati
    digitalWrite(RPOMPA1_PIN, relayOFF);                        // Pompa 1 mati
  }

  // Jika suhu udara sedang / kelembaban sedang / intensitas cahaya sedang / tanah lembab, maka :  
  if (temp >= 16 && temp <= 34 || hum >= 30 && hum <= 90 || lux >= 200 && lux < 500 || moisture > 40 && moisture < 60) { 
    if (temp >= 16 && temp <= 34) { 
      info_suhu_udara = "Suhu Udara: Normal";                   // Normal
      status_udara = "Status Kualitas Udara: Aman";             // Status Udara: Aman
    }
    if (hum >= 30 && hum <= 90) { 
      info_kelembaban_udara = "Kelembaban Udara: Normal";       // Lembab
      status_udara = "Status Kualitas Udara: Aman";             // Status Udara: Aman
    }
    if (lux >= 200 && lux < 500) {
      info_intensitas_cahaya = "Intensitas Cahaya: Normal";     // Remang-remang
      status_sinar = "Status Kualitas Sinar: Aman";             // Status Sinar: Aman
    }
    if (moisture > 40 && moisture < 60) { 
      info_kelembaban_tanah = "Kelembaban Tanah: Normal";       // Lembab
      status_tanah = "Status Kualitas Tanah: Aman";             // Status Tanah: Aman
    }
    Blynk.virtualWrite(V4, 0);                                  // Indikator Pompa 1: mati
    digitalWrite(RPOMPA1_PIN, relayOFF);                        // Pompa 1 mati
  }

  // Jika suhu udara tinggi / kelembaban rendah / intensitas cahaya tinggi / tanah kering, maka :
  if (temp > 34 && temp <= 80 || hum >= 0 && hum < 30 || lux >= 500 && lux <= 100000 || moisture >= 0 && moisture <= 40) { 
    if (temp > 34 && temp <= 80) {
      info_suhu_udara = "Suhu Udara: Tinggi";                   // Panas
      status_udara = "Status Kualitas Udara: Bahaya";           // Status Udara: Bahaya
    }
    if (hum >= 0 && hum < 30) {
      info_kelembaban_udara = "Kelembaban Udara: Rendah";       // Kering
      status_udara = "Status Kualitas Udara: Bahaya";           // Status Udara: Bahaya
    }
    if (lux >= 500 && lux <= 100000) {
      info_intensitas_cahaya = "Intensitas Cahaya: Tinggi";     // Cerah
      status_sinar = "Status Kualitas Sinar: Bahaya";           // Status Sinar: Bahaya
    }
    if (moisture >= 0 && moisture <= 40) {
      info_kelembaban_tanah = "Kelembaban Tanah: Rendah";       // Kering
      status_tanah = "Status Kualitas Tanah: Bahaya";           // Status Tanah: Bahaya
    }
    Blynk.virtualWrite(V4, 1);                                  // Indikator Pompa 1: menyala
    digitalWrite(RPOMPA1_PIN, relayON);                         // Pompa 1 menyala
  }
}

// Method untuk mengatur visualisasi tombol bot telegram
void buttonBot() { 
  // Monitoring menu dalam bentuk inline button
  InlineKey.addButton("🌤️ Monitoring Temperature", InlineMenu1, CTBotKeyboardButtonQuery);
  InlineKey.addRow();
  InlineKey.addButton("🌦️ Monitoring Humidity", InlineMenu2, CTBotKeyboardButtonQuery);
  InlineKey.addRow();
  InlineKey.addButton("🌱 Monitoring Soil Moisture", InlineMenu3, CTBotKeyboardButtonQuery);
  InlineKey.addRow();
  InlineKey.addButton("☀️ Monitoring Light Intensity", InlineMenu4, CTBotKeyboardButtonQuery);
  InlineKey.addRow();
  InlineKey.addButton("🚰 Controlling Water Pump", InlineMenu5, CTBotKeyboardButtonQuery);
  
  // Menu kontrol dalam bentuk inline button
  InlineOption.addButton("✅ Pump: Turn ON", "ON", CTBotKeyboardButtonQuery);
  InlineOption.addButton("❌ Pump: Turn OFF", "OFF", CTBotKeyboardButtonQuery);
  
  // Tombol -> default : hidden
  viewTombol = false;
}

// Method untuk mengatur bot telegram
void botTelegram() {
  TBMessage msg; // Konstruktor TBMessage -> msg
  
  if(myBot.getNewMessage(msg)){  
    if(msg.text.equalsIgnoreCase("/start")){ // Start Bot
      msg1 = "🙋🏻‍♂️ Hai @" + msg.sender.username + " 👋👋\nSelamat datang di Layanan BOT ECHEVERIA.";
      msg2 = "\n\n🔐 Silahkan isi kode rahasia 👇👇\n.................................. *(13 Characters)";
      sendMsg = msg1 + msg2; myBot.sendMessage(msg.sender.id, sendMsg); // Mengirim pesan
    } 
    else if(msg.text.equalsIgnoreCase(KodeBot)){ // Menu Utama
      msg1 = "🔓 Kode yang anda masukkan benar";
      myBot.sendMessage(msg.sender.id, msg1);
      main_menu:
      msg2 = "\n--------------------------------------------------------------\n 📝 MENU UTAMA \n--------------------------------------------------------------\nSilahkan pilih menu dibawah ini 👇👇";
      viewTombol = true; myBot.sendMessage(msg.sender.id, msg2, InlineKey); // Mengirim pesan dan menampilkan tombol
    }
    else if(msg.messageType == CTBotMessageQuery){ // Respon Inline Button
      if(msg.callbackQueryData.equals(InlineMenu1)){ // Menampilkan data monitoring suhu udara beserta statusnya
        Serial.println("\n<------------------------------->");
        Serial.println("Deteksi Suhu Udara: " + String(temp, 2) + "°C");
        Serial.println(info_suhu_udara);
        Serial.println(status_udara);
        Serial.println("<------------------------------->\n");
        msg1 = "🙋🏻‍♂️ Hai @" + msg.sender.username + " 👋👋\nBerikut hasil monitoring suhu udara pada tanaman echeveria terkini:\n\n--------------------------------------------------------------\n 🌤️ MONITORING TEMPERATURE \n--------------------------------------------------------------\n";
        msg2 = "📲 Suhu udara tanaman: " + String(temp) + "°C\n✍️ " + String(status_udara) + "\n--------------------------------------------------------------"; 
        sendMsg = msg1 + msg2; myBot.sendMessage(msg.sender.id, sendMsg); // Mengirim pesan
      }
      else if(msg.callbackQueryData.equals(InlineMenu2)){ // Menampilkan data monitoring kelembaban udara beserta statusnya
        Serial.println("\n<------------------------------->");
        Serial.println("Deteksi Kelembaban Udara: " + String(hum) + "%");
        Serial.println(info_kelembaban_udara);
        Serial.println(status_udara);
        Serial.println("<------------------------------->\n");
        msg1 = "🙋🏻‍♂️ Hai @" + msg.sender.username + " 👋👋\nBerikut hasil monitoring kelembaban udara pada tanaman echeveria terkini:\n\n--------------------------------------------------------------\n 🌦️ MONITORING HUMIDITY \n--------------------------------------------------------------\n";
        msg2 = "📲 Kelembaban udara tanaman: " + String(hum) + "%\n✍️ " + String(status_udara) + "\n--------------------------------------------------------------"; 
        sendMsg = msg1 + msg2; myBot.sendMessage(msg.sender.id, sendMsg); // Mengirim pesan
      }
      else if(msg.callbackQueryData.equals(InlineMenu3)){ // Menampilkan data monitoring kelembaban tanah beserta statusnya
        Serial.println("\n<------------------------------->");
        Serial.println("Deteksi Kelembaban Tanah: " + String(moisture) + "%");
        Serial.println(info_kelembaban_tanah);
        Serial.println(status_tanah);
        Serial.println("<------------------------------->\n");
        msg1 = "🙋🏻‍♂️ Hai @" + msg.sender.username + " 👋👋\nBerikut hasil monitoring kelembaban tanah pada tanaman echeveria terkini:\n\n--------------------------------------------------------------\n 🌱 MONITORING SOIL MOISTURE \n--------------------------------------------------------------\n";
        msg2 = "📲 Kelembaban tanah tanaman: " + String(moisture) + "%\n✍️ " + String(status_tanah) + "\n--------------------------------------------------------------"; 
        sendMsg = msg1 + msg2; myBot.sendMessage(msg.sender.id, sendMsg); // Mengirim pesan
      }
      else if(msg.callbackQueryData.equals(InlineMenu4)){ // Menampilkan data monitoring intensitas cahaya beserta statusnya
        Serial.println("\n<------------------------------->");
        Serial.println("Deteksi Cahaya: " + String(lux) + "lx");
        Serial.println(info_intensitas_cahaya);
        Serial.println(status_sinar);
        Serial.println("<------------------------------->\n");
        msg1 = "🙋🏻‍♂️ Hai @" + msg.sender.username + " 👋👋\nBerikut hasil monitoring cahaya pada tanaman echeveria terkini:\n\n--------------------------------------------------------------\n ☀️ MONITORING LIGHT INTENSITY \n--------------------------------------------------------------\n";
        msg2 = "📲 Cahaya tanaman: " + String(lux) + "lx\n✍️ " + String(status_sinar) + "\n--------------------------------------------------------------"; 
        sendMsg = msg1 + msg2; myBot.sendMessage(msg.sender.id, sendMsg); // Mengirim pesan
      }
      else if(msg.callbackQueryData.equals(InlineMenu5)){ // Opsi controlling
        sendMsg = "🙋🏻‍♂️ Hai @" + msg.sender.username + " 👋👋\nPilihlah opsi controlling berikut:\n";
        myBot.sendMessage(msg.sender.id, sendMsg, InlineOption); // Mengirim pesan dan menampilkan tombol
      }
      else if(msg.callbackQueryData.equals("ON")){ // Memberikan perintah untuk menyalakan pompa 2
        Blynk.virtualWrite(V5, 1); // Indikator Pompa 2: menyala
        Serial.println("\n<------------------------------->");
        Serial.println("Status kontrol air: On");
        Serial.println("<------------------------------->");
        msg1 = "🙋🏻‍♂️ Hai @" + msg.sender.username + " 👋👋\nBerikut hasil controlling pompa air pada tanaman echeveria terkini:\n\n--------------------------------------------------------------\n 🚰 CONTROLLING WATER PUMP \n--------------------------------------------------------------\n";
        msg2 = "📲 Controlling water pump: ON\n--------------------------------------------------------------"; 
        sendMsg = msg1 + msg2; myBot.sendMessage(msg.sender.id, sendMsg); // Mengirim pesan
        digitalWrite(RPOMPA2_PIN, relayON); // Pompa 2 menyala
      }
      else if(msg.callbackQueryData.equals("OFF")){ // Memberikan perintah untuk mematikan pompa 2
        Blynk.virtualWrite(V5, 0); // Indikator Pompa 2: mati
        Serial.println("\n<------------------------------->");
        Serial.println("Status kontrol air: Off");
        Serial.println("<------------------------------->");
        msg1 = "🙋🏻‍♂️ Hai @" + msg.sender.username + " 👋👋\nBerikut hasil controlling pompa air pada tanaman echeveria terkini:\n\n--------------------------------------------------------------\n 🚰 CONTROLLING WATER PUMP \n--------------------------------------------------------------\n";
        msg2 = "📲 Controlling water pump: OFF\n--------------------------------------------------------------"; 
        sendMsg = msg1 + msg2; myBot.sendMessage(msg.sender.id, sendMsg); // Mengirim pesan
        digitalWrite(RPOMPA2_PIN, relayOFF); // Pompa 2 mati
      }
    }
    else{ // Jika perintah tidak sesuai maka :
      sendMsg = "🙋🏻‍♂️ Hai @" + msg.sender.username + " 👋👋\n\n❌ Gagal mengakses, coba lagi";
      myBot.sendMessage(msg.sender.id, sendMsg); // Mengirim pesan gagal
    } 
  }
}

// Method untuk kirim data sensor ke Blynk melalui protokol TCP/IP
void sendData(float suhu_udara, int kelembaban_udara, int kelembaban_tanah, int cahaya) {
  Blynk.virtualWrite(V0, suhu_udara);         // Mengirimkan data sensor suhu udara ke Blynk
  Blynk.virtualWrite(V1, kelembaban_udara);   // Mengirimkan data sensor kelembaban udara ke Blynk
  Blynk.virtualWrite(V2, kelembaban_tanah);   // Mengirimkan data sensor kelembaban tanah ke Blynk
  Blynk.virtualWrite(V3, cahaya);             // Mengirimkan data sensor intensitas cahaya ke Blynk
}

// Method untuk memulai LCD
void lcdInit(){
  lcd.init(); // Memulai LCD
  lcd.clear(); lcd.backlight(); lcd.setCursor(1,0); lcd.print("Starting"); lcd.setCursor(1,1); lcd.print("Smart System..."); delay(2500); // Tampilan Pertama
  lcd.clear(); lcd.backlight(); lcd.setCursor(1,0); lcd.print("Welcome to"); lcd.setCursor(1,1); lcd.print("Echeveria BoT..."); delay(2500); // Tampilan Kedua
  lcd.clear(); lcd.backlight(); lcd.setCursor(1,0); lcd.print("Loading...."); delay(5000); // Tampilan Ketiga
}

// Method yang dijalankan sekali
void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD); // Baudrate untuk papan ESP
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD); // Memulai Blynk
  connectBot(); // Memanggil method connectBot
  buttonBot(); // Memanggil method buttonBot
  lcdInit(); // Memanggil method lcdInit
  fc28.begin(); // Memulai sensor fc-28
  dht.begin(); // Memulai sensor dht
  pinMode(RPOMPA1_PIN, OUTPUT); // Pompa 1 sebagai OUTPUT
  pinMode(RPOMPA2_PIN, OUTPUT); // Pompa 2 sebagai OUTPUT
  digitalWrite(RPOMPA1_PIN, relayOFF); // Default relay1: OFF
  digitalWrite(RPOMPA2_PIN, relayOFF); // Default relay2: OFF
  timer.setInterval(1000L, readSensor); // Interval pengiriman setiap 1 detik
}

// Method yang dijalankan berulang kali
void loop() {
  botTelegram(); // Memanggil method botTelegram
  Blynk.run(); // Menjalankan Blynk
  timer.run(); // Menjalankan Timer
}
