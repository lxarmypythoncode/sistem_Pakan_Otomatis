#define BLYNK_TEMPLATE_ID "TMPL6jTWkqk-d"
#define BLYNK_TEMPLATE_NAME "pakan ikan"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Servo.h>
#include <RTClib.h>

// Konfigurasi LCD
LiquidCrystal_I2C lcd(0x27, 24, 4); // Alamat I2C LCD biasanya 0x27 atau 0x3F

// Konfigurasi sensor ultrasonik
const int trigPin = D3;  // GPIO5 = D1 di ESP8266
const int echoPin = D6;  // GPIO12 = D6 di ESP8266

// Konfigurasi servo
Servo servoMotor;
const int servoPin = D5;  // GPIO14 = D5 di ESP8266

// Konfigurasi relay
const int relayPin = D7;  // GPIO13 = D7 di ESP8266

// Blynk Auth Token
char auth[] = "L_0GoG4Y_xRI2cnhYn8csrAkh7yxMi8d";

// Nama jaringan WiFi dan kata sandi
char ssid[] = "Ente Bahlul";  // Menggunakan 'char' bukan 'const char'
char pass[] = "pokok sak seneng";

// Virtual pin untuk widget Blynk
#define VIRTUAL_FEED_BUTTON V1
#define VIRTUAL_DISTANCE_DISPLAY V2
#define VIRTUAL_FEED_LEVEL V3
#define VIRTUAL_RELAY_CONTROL V4 // Untuk kontrol relay
#define VIRTUAL_TIME_INPUT V5 // Untuk pengaturan waktu manual

// Variabel untuk mengukur jumlah pakan
const int maxDistance = 30; // Jarak maksimum dalam cm (sesuaikan dengan tinggi wadah)
int feedLevel;
float totalFeedWeight = 0; // Total berat pakan yang telah diberikan dalam kg
const float feedWeightPerTurn = 0.5; // Berat pakan per satu kali putaran servo dalam kg (disesuaikan)

// Konfigurasi RTC
RTC_DS3231 rtc;

// Timer Blynk
BlynkTimer timer;

void setup() {
  Serial.begin(115200);

  // Inisialisasi koneksi WiFi dan Blynk
  WiFi.begin(ssid, pass);
  Blynk.begin(auth, ssid, pass);

  // Inisialisasi servo
  servoMotor.attach(servoPin);
  servoMotor.write(0); // Pastikan wadah tertutup pada awal

  // Inisialisasi relay
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW); // Matikan relay pada awal

  // Inisialisasi sensor ultrasonik
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Inisialisasi RTC
  if (!rtc.begin()) {
    Serial.println("RTC tidak terdeteksi!");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC kehilangan daya, set waktu sekarang!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Set waktu RTC secara otomatis
  }

  // Inisialisasi LCD
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Sistem Pakan IOT");
  lcd.setCursor(0, 1);
  lcd.print("Inisialisasi...");

  // Pengukuran jarak setiap 1 detik dan kirim ke Blynk
  timer.setInterval(1000L, updateLCDAndBlynk);
}

void loop() {
  Blynk.run();
  timer.run();
}

// Fungsi untuk membaca jarak dari sensor ultrasonik
int readUltrasonicDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  int distance = duration * 0.034 / 2;

  return distance;
}

// Fungsi untuk menghitung level pakan berdasarkan jarak
void calculateFeedLevel() {
  int distance = readUltrasonicDistance();
  feedLevel = map(distance, 0, maxDistance, 100, 0); // Mengonversi jarak ke persentase
  if (feedLevel < 0) {
    feedLevel = 0;
  } else if (feedLevel > 100) {
    feedLevel = 100;
  }
}

// Fungsi untuk mengirim level pakan ke Blynk dan memperbarui LCD
void updateLCDAndBlynk() {
  calculateFeedLevel();
  Blynk.virtualWrite(VIRTUAL_FEED_LEVEL, feedLevel);

  // Update data di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sistem Pakan IOT");
  lcd.setCursor(0, 1);
  lcd.print("Level Pakan: ");
  lcd.print(feedLevel);
  lcd.print("%");

  DateTime now = rtc.now();
  lcd.setCursor(0, 2);
  lcd.print("Waktu: ");
  lcd.print(now.hour());
  lcd.print(":");
  lcd.print(now.minute());
  lcd.print(":");
  lcd.print(now.second());

  Serial.print("Feed level: ");
  Serial.print(feedLevel);
  Serial.println("%");
}

void checkFeedTime() {
  DateTime now = rtc.now();
  
  // Jika waktu pukul 7 pagi atau 5 sore
  if ((now.hour() == 7 && now.minute() == 0) || (now.hour() == 17 && now.minute() == 0)) {
    feedFish();
  }
}

// Fungsi untuk memberikan pakan (menggerakkan servo)
void feedFish() {
  // Contoh menggerakkan servo untuk membuka dan menutup pakan
  Serial.println("Memberi pakan...");
  servoMotor.write(-150);  // Servo ke posisi buka
  delay(5000);           // Tunggu selama 5 detik
  servoMotor.write(0);   // Servo ke posisi tutup
  delay(1000);           // Tunggu sejenak setelah menutup
  totalFeedWeight += feedWeightPerTurn; // Tambah berat pakan setelah sekali putar
  Serial.print("Total berat pakan yang diberikan: ");
  Serial.print(totalFeedWeight);
  Serial.println(" kg");
}

// Fungsi untuk mengendalikan relay dari Blynk
BLYNK_WRITE(VIRTUAL_RELAY_CONTROL) {
  int relayState = param.asInt();
  digitalWrite(relayPin, relayState); // Mengontrol relay ON/OFF

  if (relayState == 1) {
    Serial.println("Relay dihidupkan, servo siap digunakan.");
  } else {
    Serial.println("Relay dimatikan, servo tidak aktif.");
  }
}

// Fungsi untuk mengendalikan servo dari tombol Blynk
BLYNK_WRITE(VIRTUAL_FEED_BUTTON) {
  int buttonState = param.asInt();

  if (buttonState == 1 && digitalRead(relayPin) == HIGH) {
    while (totalFeedWeight < 10) {
      servoMotor.write(90); // Posisi buka
      delay(5000); // Tunggu 5 detik
      servoMotor.write(0); // Posisi tutup
      delay(5000); // Delay sebelum memutar lagi

      totalFeedWeight += feedWeightPerTurn; // Tambah berat pakan per putaran
      Serial.print("Total berat pakan: ");
      Serial.print(totalFeedWeight);
      Serial.println(" kg");

      // Jika berat mencapai 10 kg, hentikan
      if (totalFeedWeight >= 10) {
        Serial.println("Batas 10 kg tercapai, servo berhenti.");
        break;
      }
    }
  }
}

// Fungsi untuk mengatur waktu secara manual dari Blynk
BLYNK_WRITE(VIRTUAL_TIME_INPUT) {
  TimeInputParam t(param);

  if (t.hasStartTime()) {
    rtc.adjust(DateTime(2024, 1, 1, t.getStartHour(), t.getStartMinute(), 0));
    Serial.println("Waktu diatur secara manual.");
  } else {
    Serial.println("Waktu manual tidak tersedia.");
  }
}
