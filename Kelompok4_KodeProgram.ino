#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <DHT.h>

#define ledPin D0
#define buzzerPin D1 
#define flamePin D2
#define DHTPin D4
#define Pin_MQ2 A0

// Initialize DHT sensor
DHT dht(DHTPin, DHT11);

// WiFi credentials
const char* ssid = "UGM-Hotspot";
const char* password = "";

// Telegram BOT Token and Chat ID
String BOTtoken = "7334959792:AAFr31P__tcmKEkDRON4QBRJABI_t-mlMI4";
String chat_id = "1531663516";

#ifdef ESP8266
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

// Initialize Telegram BOT
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

unsigned long lastTimeCheck = 0;
const unsigned long botRequestDelay = 1000;

void setup() {
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(flamePin, INPUT);
  pinMode(Pin_MQ2, INPUT);
  digitalWrite(ledPin, LOW);
  digitalWrite(buzzerPin, LOW);
  dht.begin();

  // Connect to Wi-Fi
#ifdef ESP8266
  configTime(0, 0, "pool.ntp.org");
  client.setTrustAnchors(&cert);
#endif
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
#ifdef ESP32
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
#endif
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
  } else {
    Serial.println("Failed to connect to WiFi");
    return;
  }
}

int readMQ2() {
  unsigned int sensorValue = analogRead(Pin_MQ2);  // Read the analog value from sensor
  unsigned int outputValue = map(sensorValue, 0, 1023, 0, 255); // map the 10-bit data to 8-bit data

  return outputValue;             // Return analog gas value
}

// Mendeteksi gas
void checkAndNotifyGasLeak(int gasValue, float temperature) {
  if (gasValue > 100) {
    String message = "Gas bocor terdeteksi! Nilai PPM: " + String(gasValue) + " PPM";
    bot.sendMessage(chat_id, message, "");
    Serial.println(message);
    digitalWrite(buzzerPin, HIGH); // Turn on buzzer
    if (gasValue > 100 && temperature > 35) {
      String message = "Gas bocor dan suhu tinggi! Berpotensi kebakaran besar! Nilai PPM: " + String(gasValue) + " PPM";
      bot.sendMessage(chat_id, message, "");
      Serial.println(message);
      digitalWrite(ledPin, HIGH);  // Turn on warning LED
      digitalWrite(buzzerPin, HIGH); // Turn on buzzer
    }
  } else {
    digitalWrite(buzzerPin, LOW); // Turn off buzzer
  }
}

// Mendeteksi api
void checkAndNotifyFlame() {
  int flameValue = digitalRead(flamePin);
  if (flameValue == LOW) {  // Assuming LOW indicates flame detected
    String message = "Api terdeteksi!";
    bot.sendMessage(chat_id, message, "");
    Serial.println(message);
    digitalWrite(ledPin, HIGH);  // Turn on warning LED
    digitalWrite(buzzerPin, HIGH); // Turn on buzzer
  } 
}

// Mendeteksi suhu
void checkAndNotifyTemperature(float temperature, int gasValue) {
  if (temperature > 35) {
    String message = "Suhu tinggi!";
    bot.sendMessage(chat_id, message, "");
    Serial.println(message);
    digitalWrite(ledPin, HIGH);  // Turn on warning LED
    if (temperature > 35 && gasValue > 100) {
      String message = "Gas bocor dan suhu tinggi! Berpotensi kebakaran besar! Nilai PPM: " + String(gasValue) + " PPM";
      bot.sendMessage(chat_id, message, "");
      Serial.println(message);
      digitalWrite(ledPin, HIGH);  // Turn on warning LED
      digitalWrite(buzzerPin, HIGH); // Turn on buzzer
    }
  } else {
    digitalWrite(ledPin, LOW);   // Turn off warning LED
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    
    if (text == "/start") {
      String welcome = "Welcome to the \"Pendeteksi Kebakaran\" Bot.\n";
      welcome += "Use the following commands to interact with the bot.\n\n";
      welcome += "/suhu - Get the current temperature status\n";
      welcome += "/gas - Get the current gas sensor value (PPM)\n";
      bot.sendMessage(chat_id, welcome, "");
    }

    if (text == "/suhu") {
      float currentTemperatureC = dht.readTemperature();
      if (isnan(currentTemperatureC)) {
        bot.sendMessage(chat_id, "Failed to read from DHT sensor!", "");
      } else {
        String temperatureStatus = "Current temperature: " + String(currentTemperatureC) + "°C";
        temperatureStatus += " / " + String(currentTemperatureC * 9 / 5 + 32) + "°F";
        bot.sendMessage(chat_id, temperatureStatus, "");
      }
    }

    if (text == "/gas") {
      int gasValue = readMQ2();
      String ppmStatus = "Current gas sensor value: " + String(gasValue) + " PPM";
      bot.sendMessage(chat_id, ppmStatus, "");
    }
  }
}

void loop() {
  int gasValue = readMQ2();
  float currentTemperatureC = dht.readTemperature();
  
  Serial.print("Analog output: ");
  Serial.println(gasValue);

  if (isnan(currentTemperatureC)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    Serial.print("Temperature: ");
    Serial.print(currentTemperatureC);   // Print the temperature in Celsius
    Serial.print("°C");
    Serial.print("  ~  "); // Separator between Celsius and Fahrenheit
    Serial.print(currentTemperatureC * 9 / 5 + 32);   // Print the temperature in Fahrenheit
    Serial.println("°F");
  }

  checkAndNotifyTemperature(currentTemperatureC, gasValue);  // Check and notify temperature
  checkAndNotifyGasLeak(gasValue, currentTemperatureC);  // Check and notify gas leak
  checkAndNotifyFlame();  // Check and notify flame

  delay(1000);

  // Handle new Telegram messages
  if (millis() - lastTimeCheck > botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    handleNewMessages(numNewMessages);
    lastTimeCheck = millis();
  }
}
