/*************************************************
   ESP32 Akıllı Bitki Sulama - Blynk + Simülasyon Modu
*************************************************/

#define BLYNK_TEMPLATE_ID "TMPL6BuE3z_Rh"
#define BLYNK_TEMPLATE_NAME "deneme"
#define BLYNK_AUTH_TOKEN "nFknPCNicuOJcr1oD-drInA2sC5tVqNE"

#define BLYNK_PRINT Serial 
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h> // Sinüs hesabı için

// --- SİMÜLASYON AYARI ---
// true  = Matematiksel simülasyon çalışır (Blynk'te veriler değişir)
// false = Gerçek fiziksel sensörlerden veri okur
#define SIMULATION_MODE true 

// --- WIFI BİLGİLERİ ---
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Wokwi-GUEST"; 
char pass[] = "";

// --- PINLER ---
#define SOIL_PIN   34
#define WATER_PIN  35
#define TEMP_PIN   4
#define PUMP_PIN   26

OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);
BlynkTimer timer;

// --- KALİBRASYON ---
int dryValue = 4095;
int wetValue = 0;
bool pumpState = false;

// --- SİMÜLASYON DEĞİŞKENLERİ ---
int simSoil = 45;   // Başlangıç nemi %45
int minSoilMoisture = 35; // Minimum nem seviyesi
float simWater = 100.0;  // Başlangıç su deposu %100
float simTemp = 24.0;   // Başlangıç sıcaklığı

/*************************************************/
// SİMÜLASYON MOTORU: Gerçek dünyayı taklit eder
void runSimulationEngine() {
  // 1. Sıcaklık: Sinüs dalgası (25°C merkezli, +-8 derece dalgalanma)
  simTemp = 25.0 + 8.0 * sin(millis() / 10000.0);

  // 2. Nem ve Su Seviyesi
  if (pumpState) {
    simSoil += 5.0;   // Pompa açıksa nem hızla artar
    simWater -= 1.5;  // Depodaki su azalır
  } else {
    simSoil -= 0.8;   // Pompa kapalıysa buharlaşma ile nem düşer
  }

  // Sınırlandırmalar
  if (simSoil > 100) simSoil = 100;
  if (simSoil < 0) simSoil = 0;
  if (simWater < 0) simWater = 0;
}

/*************************************************/
int readSoilPercent() {
  if (SIMULATION_MODE) return (int)simSoil;
  int raw = analogRead(SOIL_PIN);
  return map(raw, dryValue, wetValue, 0, 100);
}

int readWaterPercent() {
  if (SIMULATION_MODE) return (int)simWater;
  int raw = analogRead(WATER_PIN);
  return map(raw, 0, 4095, 0, 100);
}

float readTemperature() {
  if (SIMULATION_MODE) return simTemp;
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

/*************************************************/
void controlPump(int soil, int water) {
  if (water < 10) {
    digitalWrite(PUMP_PIN, LOW);
    pumpState = false;
    return;
  }

  if (soil < minSoilMoisture) {
    digitalWrite(PUMP_PIN, HIGH);
    pumpState = true;
  } else if (soil > 55) {
    digitalWrite(PUMP_PIN, LOW);
    pumpState = false;
  }
}

/*************************************************/
void sendSensorData() {
  // Eğer simülasyon açıksa önce değerleri güncelle
  if (SIMULATION_MODE) runSimulationEngine();

  int soil = readSoilPercent();
  int water = readWaterPercent();
  float temp = readTemperature();

  controlPump(soil, water);

  // Serial Monitor Çıktısı
  Serial.print("Nem: %"); Serial.print(soil);
  Serial.print(" | Su: %"); Serial.print(water);
  Serial.print(" | Sicaklik: "); Serial.print(temp);
  Serial.print(" | Pompa: "); Serial.println(pumpState ? "ACIK" : "KAPALI");
  Serial.print(" | Nem Alt Değeri: ");Serial.println(minSoilMoisture);
  // Blynk Dashboard Güncelleme
  Blynk.virtualWrite(V0, soil);
  Blynk.virtualWrite(V1, water);
  Blynk.virtualWrite(V2, temp);
  Blynk.virtualWrite(V3, pumpState);

  // Dinamik Renk Değişimi (Sıcaklık Grafiği için)
  if(temp > 30.0) {
    Blynk.setProperty(V2, "color", "#D3435C"); // Sıcaksa Kırmızı
  } else {
    Blynk.setProperty(V2, "color", "#23C48E"); // Normalse Yeşil
  }

  if(water < 10) {
    Blynk.logEvent("su_bitti_uyarisi", "Depo boşalıyor!");
  }
}

BLYNK_WRITE(V4) {
  int value = param.asInt(); // Get value as integer
  Serial.print("Data received from Dashboard: ");
  Serial.println(value);
  
  if (value > 0 && value <= 55) {
      minSoilMoisture = value;
      Serial.print("Minimum nem seviyesi güncellendi: ");
  } 
}

/*************************************************/
void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  analogReadResolution(12);
  sensors.begin();

  Blynk.begin(auth, ssid, pass);
  timer.setInterval(2000L, sendSensorData);
  
  Serial.println("Sistem Hazir!");
}

void loop() {
  Blynk.run();
  timer.run();
}