#define BLYNK_TEMPLATE_ID "TMPL2-cFv8Pvu"
#define BLYNK_TEMPLATE_NAME "cap"
#define BLYNK_AUTH_TOKEN "QtbvvUp7IwuozA3skUg-aWIEJQZgfwWR"

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <time.h> // Include time library for NTP

#define soil_moisture 33
DHT dht(32, DHT22);

// Connecting Wi-Fi to My ESP.
#define WIFI_SSID "ESP22"
#define WIFI_PASSWORD "123456781"

// Private data for each person, you should replace it by your firebase's information.
#define API_KEY "AIzaSyDJm2PhN8QPqFsdniXowHpVLA3WRFRU5mw"  
#define PROJECT_ID "esp-cloud-3ee7c"                       
#define DATABASE_ID "(default)"                            
#define DATABASE_URL "https://esp-cloud-3ee7c-default-rtdb.firebaseio.com/" 

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastUpdateTime = 0;
unsigned long firebaseUpdateTime = 0;

bool signupOK = false;

// Blynk virtual pins
#define VIRTUAL_PIN_TEMP V0
#define VIRTUAL_PIN_HUMIDITY V1
#define VIRTUAL_PIN_SOIL V2
#define BUZZER_PIN 5 

// NTP Server and time zone offset for Egypt (UTC+2)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 2 * 3600;   // Egypt is UTC+2
const int daylightOffset_sec = 3600;  

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Wi-Fi connection check.

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Initialize time with NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Firebase configuration
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("signUp OK");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;  
  Firebase.begin(&config, &auth);  // Initialize Firebase without explicit sign-up
  Firebase.reconnectWiFi(true);

  // Connection to Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);

  dht.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(soil_moisture, INPUT);

}
// Timestamp check.
String getFormattedTime() {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo)) {
    Serial.println("Failed to obtain time");
    return "";
  }
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeInfo);
  return String(timeStringBuff);
}

void loop() {
  Blynk.run();

  // Read sensor values
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  int sensorValue = analogRead(soil_moisture);
  float percentage = map(sensorValue, 0, 4095, 0, 100);

  Serial.print("Temp: "); Serial.print(temp); Serial.print(" C ");
  Serial.print("Humidity: "); Serial.print(humidity); Serial.print(" % ");
  Serial.print("Soil Moisture: "); Serial.print(percentage); Serial.println(" %");

  // Severity logic
  int severityLevel = 0;
  if (temp >= 5 && temp <= 15) severityLevel++;
  if (humidity > 60) severityLevel++;
  if (percentage > 60) severityLevel++;

  // Buzzer alert based on severity
  if (severityLevel == 1) {
    tone(BUZZER_PIN, 1000); delay(500); noTone(BUZZER_PIN); delay(500);
  } else if (severityLevel == 2) {
    tone(BUZZER_PIN, 1000); delay(200); noTone(BUZZER_PIN); delay(200); tone(BUZZER_PIN, 1000); delay(200); noTone(BUZZER_PIN); delay(500);
  } else if (severityLevel == 3) {
    tone(BUZZER_PIN, 1000); delay(100); noTone(BUZZER_PIN); delay(100); tone(BUZZER_PIN, 1000); delay(100); noTone(BUZZER_PIN); delay(100); tone(BUZZER_PIN, 1000); delay(100); noTone(BUZZER_PIN); delay(500);
  }

  // Non-blocking Firebase and Blynk updates
  if (millis() - lastUpdateTime >= 2000) {
    lastUpdateTime = millis();
    // Update Blynk with new records.
    Blynk.virtualWrite(VIRTUAL_PIN_TEMP, temp);
    Blynk.virtualWrite(VIRTUAL_PIN_HUMIDITY, humidity);
    Blynk.virtualWrite(VIRTUAL_PIN_SOIL, percentage);
  }
  if (severityLevel == 1) {
    Blynk.logEvent("level_1", "One condition met for potential wheat fungus disease.");
  } else if (severityLevel == 2) {
    Blynk.logEvent("level_2", "Two conditions met for potential wheat fungus disease.");
  } else if (severityLevel == 3) {
    Blynk.logEvent("level_3", "All conditions met for high risk of wheat fungus disease.");
  }
  if (millis() - firebaseUpdateTime >= 5000 && signupOK) {
    firebaseUpdateTime = millis();

    // Batch Firebase update
    FirebaseJson json;
    json.set("Temperature", temp);
    json.set("Humidity", humidity);
    json.set("Moisture", percentage);

    if (Firebase.RTDB.updateNode(&fbdo, "test", &json)) {
      Serial.println("Data sent to Firebase.");
    } else {
      Serial.println("Failed to send data: " + fbdo.errorReason());
    }

    // Prepare Firestore document
    String timestamp = getFormattedTime();
    String collectionPath = "sensorData"; // Firestore collection name
    String documentPath = "reading_" + String(millis());
    FirebaseJson firestoreContent;
    firestoreContent.set("fields/temperature/doubleValue", temp);
    firestoreContent.set("fields/humidity/doubleValue", humidity);
    firestoreContent.set("fields/moisture/doubleValue", percentage);
    firestoreContent.set("fields/timestamp/stringValue", timestamp);

    if (Firebase.Firestore.createDocument(&fbdo, PROJECT_ID, DATABASE_ID, collectionPath.c_str(), documentPath.c_str(), firestoreContent.raw(), "")) {
      Serial.println("Data sent to Firestore.");
    } else {
      Serial.println("Failed to send data to Firestore: " + fbdo.errorReason());
    }
  }
  delay(2000);
}
