#include <SoftwareSerial.h>
#include <DHT.h>
#include <EEPROM.h>

#define DHTPIN 3        // DHT22 connected to D3
#define DHTTYPE DHT22   // DHT22 sensor type
#define LDRPIN 2        // LDR sensor connected to D2

#define RELAY_FAN 4     // Fan relay
#define RELAY_HUMID 5   // Humidifier relay
#define RELAY_LIGHT 6   // Light relay

DHT dht(DHTPIN, DHTTYPE);
SoftwareSerial btSerial(10, 11);  // HC-05 Bluetooth (RX, TX)

float tempThreshold;
float humidThreshold;

bool fanState = false;
bool fanAuto = false;
bool humidifierState = false;
bool humidifierAuto = false;
bool lightState = false;
bool lightAuto = false;

String receivedCommand = ""; // Store received Bluetooth command

float tempReadings[10] = {0};
float humidReadings[10] = {0};
int readingIndex = 0;
unsigned long lastSaveTime = 0;
const unsigned long saveInterval = 30000; // 30 seconds

void setup() {
    Serial.begin(9600);
    btSerial.begin(9600);
    dht.begin();

    pinMode(LDRPIN, INPUT);
    pinMode(RELAY_FAN, OUTPUT);
    pinMode(RELAY_HUMID, OUTPUT);
    pinMode(RELAY_LIGHT, OUTPUT);

    digitalWrite(RELAY_FAN, HIGH);
    digitalWrite(RELAY_HUMID, HIGH);
    digitalWrite(RELAY_LIGHT, HIGH);

    Serial.println("Home Automation System Started!");

    // Read stored thresholds from EEPROM
    EEPROM.get(0, tempThreshold);
    EEPROM.get(sizeof(float), humidThreshold);

    // If EEPROM is uninitialized, set defaults
    if (isnan(tempThreshold) || tempThreshold < -40 || tempThreshold > 100) tempThreshold = 25.0;
    if (isnan(humidThreshold) || humidThreshold < 0 || humidThreshold > 100) humidThreshold = 95.0;

    Serial.print("Loaded Temp Threshold: ");
    Serial.println(tempThreshold);
    Serial.print("Loaded Humid Threshold: ");
    Serial.println(humidThreshold);
}

void loop() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int light = digitalRead(LDRPIN);

    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    Serial.print("Temperature: "); Serial.print(temperature); Serial.print("°C, ");
    Serial.print("Humidity: "); Serial.print(humidity); Serial.println("%");

    if (light == 0) {
        Serial.println("LDR reading: Bright detected");
    } else {
        Serial.println("LDR reading: Dark detected");
    }

    // Save readings every 30 seconds
    if (millis() - lastSaveTime >= saveInterval) {
        tempReadings[readingIndex] = temperature;
        humidReadings[readingIndex] = humidity;
        readingIndex = (readingIndex + 1) % 10; // Keep index within 10 readings

        EEPROM.put(10 + (readingIndex * sizeof(float)), temperature);
        EEPROM.put(50 + (readingIndex * sizeof(float)), humidity);

        lastSaveTime = millis();
    }

    // Fan automation
    if (fanAuto) {
        if (temperature > tempThreshold) {
            digitalWrite(RELAY_FAN, LOW);
            fanState = true;
        } else {
            digitalWrite(RELAY_FAN, HIGH);
            fanState = false;
        }
    }

    // Humidifier automation
    if (humidifierAuto) {
        if (humidity < humidThreshold) {
            digitalWrite(RELAY_HUMID, LOW);
            humidifierState = true;
        } else {
            digitalWrite(RELAY_HUMID, HIGH);
            humidifierState = false;
        }
    }

    // Light automation
    if (lightAuto) {
        if (light == 1) {
            digitalWrite(RELAY_LIGHT, LOW);
            lightState = true;
        } else {
            digitalWrite(RELAY_LIGHT, HIGH);
            lightState = false;
        }
    }

    // Read Bluetooth command
    while (btSerial.available()) {
        char c = btSerial.read();
        if (c == '\n') {
            processCommand(receivedCommand);
            receivedCommand = "";
        } else {
            receivedCommand += c;
        }
    }

    delay(2000);
}

void processCommand(String command) {
    Serial.print("Received Bluetooth Command: ");
    Serial.println(command);

    if (command.startsWith("SET_TEMP")) {
        tempThreshold = command.substring(8).toFloat();
        EEPROM.put(0, tempThreshold);  // Save to EEPROM
        Serial.print("Updated Temp Threshold: ");
        Serial.println(tempThreshold);
        btSerial.println("Temp threshold updated.");
    } else if (command.startsWith("SET_HUMID")) {
        humidThreshold = command.substring(9).toFloat();
        EEPROM.put(sizeof(float), humidThreshold);  // Save to EEPROM
        Serial.print("Updated Humid Threshold: ");
        Serial.println(humidThreshold);
        btSerial.println("Humid threshold updated.");
    } else if (command == "GET_AVG") {
        float avgTemp = 0, avgHumid = 0;
        int total_count =0;
        for (int i = 0; i < 10; i++) {
          if(tempReadings[i] && humidReadings[i]){
            avgTemp += tempReadings[i];
            avgHumid += humidReadings[i];
            total_count++;
          }
        }
        avgTemp /= total_count;
        avgHumid /= total_count;

        btSerial.print("AVG_TEMP:");
        btSerial.print(avgTemp);
        btSerial.print(", AVG_HUMID:");
        btSerial.println(avgHumid);
    } else {
        if (command == "1") {
            digitalWrite(RELAY_FAN, LOW);
            fanState = true;
            btSerial.println("Fan turned ON.");
        } else if (command == "2") {
            digitalWrite(RELAY_FAN, HIGH);
            fanState = false;
            fanAuto = false;
            btSerial.println("Fan turned OFF.");
        } else if (command == "3") {
            if (fanState) fanAuto = true;
            btSerial.println("Fan set to AUTO.");
        } else if (command == "4") {
            digitalWrite(RELAY_HUMID, LOW);
            humidifierState = true;
            btSerial.println("Humidifier turned ON.");
        } else if (command == "5") {
            digitalWrite(RELAY_HUMID, HIGH);
            humidifierState = false;
            humidifierAuto = false;
            btSerial.println("Humidifier turned OFF.");
        } else if (command == "6") {
            if (humidifierState) humidifierAuto = true;
            btSerial.println("Humidifier set to AUTO.");
        } else if (command == "7") {
            digitalWrite(RELAY_LIGHT, LOW);
            lightState = true;
            btSerial.println("Light turned ON.");
        } else if (command == "8") {
            digitalWrite(RELAY_LIGHT, HIGH);
            lightState = false;
            lightAuto = false;
            btSerial.println("Light turned OFF.");
        } else if (command == "9") {
            if (lightState) lightAuto = true;
            btSerial.println("Light set to AUTO.");
        }
    }
}
