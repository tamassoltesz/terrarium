#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"
#include <WiFi.h>
#include "time.h"
#include "esp_system.h"

#define OLED_RESET 4
#define LIGHT 15
#define HEAT 17
#define RELAY_ON LOW
#define RELAY_OFF HIGH
#define MORNING 7
#define EVENING 21


Adafruit_SSD1306 display(OLED_RESET);

// DHT Sensor
uint8_t dhtPin = 16;

DHT dht(dhtPin, DHT11);

const char* ssid       = "yourssid";
const char* password   = "yourpassword";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600; 
const int   daylightOffset_sec = 3600;



const int wdtTimeout = 3000;  //time in ms to trigger the watchdog
hw_timer_t *timer = NULL;

void IRAM_ATTR resetModule() {
  ets_printf("reboot\n");
  esp_restart();
}

void setup() {

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  display.display();
  delay(2000);

  display.clearDisplay();

  pinMode(dhtPin, INPUT);
  dht.begin();

  pinMode(LIGHT, OUTPUT);
  pinMode(HEAT, OUTPUT);
  digitalWrite(LIGHT, RELAY_OFF);
  digitalWrite(HEAT, RELAY_OFF);

  Serial.begin(115200);
  Serial.printf("Connecting to %s ", ssid);

  connectWifi();
  
  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000, false); //set time in us
  timerAlarmEnable(timer);                      

}

void loop() {
  timerWrite(timer, 0); //reset timer (feed watchdog)
  
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  boolean isHeatActive = false;
  boolean isLightActive = false;
  boolean isWifiConnected = WiFi.status() == WL_CONNECTED;
  struct tm timeInfo;

  if(!isWifiConnected) {
    connectWifi();
  }

  getLocalTime(&timeInfo);

  int minTemperature = (timeInfo.tm_hour < MORNING || timeInfo.tm_hour >= EVENING) ? (20) : (25);
  int maxTemperature = (timeInfo.tm_hour >= MORNING && timeInfo.tm_hour < EVENING) ? (28) : (23);
  
  if (temperature <= minTemperature) {
    digitalWrite(HEAT, RELAY_ON);
    isHeatActive = true;
  }
  if (temperature >= maxTemperature) {
    digitalWrite(HEAT, RELAY_OFF);
    isHeatActive = false;
  }

  if(shouldBeThereLight(timeInfo)) {
    digitalWrite(LIGHT, RELAY_ON);
    isLightActive = true;
  } else {
    digitalWrite(LIGHT, RELAY_OFF);
    isLightActive = false;
  }

  writeToDisplay(temperature, humidity, isLightActive, isHeatActive, isWifiConnected, timeInfo, minTemperature, maxTemperature);
}

boolean shouldBeThereLight(struct tm timeInfo) {
  bool shouldBeLight = false;
  int hour = timeInfo.tm_hour;
  int minute = timeInfo.tm_min;
  
  if(hour >= MORNING && hour < EVENING) {
    shouldBeLight = true;
  }

  return shouldBeLight;
}

void writeToDisplay(float temperature, float humidity, boolean isLightActive, boolean isHeatActive, boolean isWifiConnected, struct tm timeInfo, int minTemperature, int maxTemperature) {
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  
  if(isWifiConnected) {
    display.print("W");
  } else {
    display.print("!");
  }

  display.print(":");
  
  if(isHeatActive) {
    display.print("H");
  } else {
    display.print("!");
  }

  display.print(":");
  
  if(isLightActive) {
    display.print("L");
  } else {
    display.print("!");
  }

  int hour = timeInfo.tm_hour;
  int minute = timeInfo.tm_min;

  char hourPrintable[12];
  char minutePrintable[12];
  sprintf(hourPrintable, "%2d", hour);
  sprintf(minutePrintable, "%2d", minute);

  char minTemperaturePrintable[12];
  char maxTemperaturePrintable[12];
  sprintf(minTemperaturePrintable, "%2d", minTemperature);
  sprintf(maxTemperaturePrintable, "%2d", maxTemperature);

  display.print(" [");
  display.print(minTemperaturePrintable);
  display.print("/");
  display.print(maxTemperaturePrintable);
  display.print("] ");

  display.print("[");
  display.print(hourPrintable);
  display.print(":");
  display.print(minutePrintable);
  display.print("]");

  display.println("\n");
  
  display.setTextSize(2);
  display.print(temperature); display.print(" C");
  display.println("");
  display.print(humidity); display.println(" %");
  display.display();
}

void printLocalTime() {
  struct tm timeinfo;
  struct tm timeofday;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(timeinfo.tm_hour);
  Serial.println(timeinfo.tm_min);
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void connectWifi() {
  int counter = 0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if(counter > 10) {
        break;
      }
      counter++;
  }
  Serial.println(" CONNECTED");
}
