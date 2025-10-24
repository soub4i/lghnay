#include "config.h"

// Library Includes
#include <Wire.h>
#include <TinyGsmClient.h>
#include <WiFi.h>
#include <HTTPClient.h>

TinyGsm modem(SerialAT);

bool setPowerBoostKeepOn(int en) {
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en) {
    Wire.write(0x37);
  } else {
    Wire.write(0x35);
  }
  return Wire.endTransmission() == 0;
}

// Connects the ESP32 to the configured WiFi network.
bool connectToWiFi() {
  SerialMon.print("Connecting to WiFi");
  // The variables 'ssid' and 'password' are defined in config.h
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    SerialMon.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    SerialMon.println("\nWiFi connected!");
    SerialMon.print("IP address: ");
    SerialMon.println(WiFi.localIP());
    return true;
  } else {
    SerialMon.println("\nWiFi connection failed!");
    return false;
  }
}

// Sends the received SMS data to the configured server endpoint via HTTP POST.
bool sendSMSToServer(String phoneNumber, String message, String timestamp) {
  if (WiFi.status() != WL_CONNECTED) {
    SerialMon.println("WiFi not connected, attempting to reconnect...");
    if (!connectToWiFi()) {
      return false;
    }
  }
  
  HTTPClient http;
  
  SerialMon.println("Posting to URL: " + String(serverURL));
  

  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Cisab ") + apikey); // 'apikey' is defined in config.h
  
  String jsonPayload = "{\"sender\":\"" + phoneNumber + "\",\"sms\":\"" + message + "\",\"ts\":\"" + timestamp + "\"}";

  int httpResponseCode = http.POST(jsonPayload);
  
  if (httpResponseCode > 0) {
    SerialMon.print("HTTP Response code: ");
    SerialMon.println(httpResponseCode);
    
    String response = http.getString();
    SerialMon.println("Response: " + response);
    SerialMon.println("=== Data Sent Successfully ===\n"); 
    
    http.end();
    return true;
  } else {
    SerialMon.print("Error sending POST (Code): ");
    SerialMon.println(httpResponseCode);
    http.end();
    return false;
  }
}


void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  connectToWiFi();
  
  // Initialize I2C for Power Management (IP5306)
  Wire.begin(I2C_SDA, I2C_SCL);
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));
  
  // Configure Modem
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  SerialMon.println("Initializing modem...");
  modem.restart();

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);

  // Unlock SIM
  if (strlen(simPIN) && modem.getSimStatus() != 3) {
    modem.simUnlock(simPIN);
  }
  
  // Wait for Network
  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
  } else {
    SerialMon.println(" success");
    int signalQuality = modem.getSignalQuality();
    SerialMon.print("Signal quality: ");
    SerialMon.println(signalQuality);
  }
  
  // Configure SMS settings on the modem
  SerialAT.println("AT");
  delay(1000);
  
  SerialAT.println("AT+CMGF=1"); //TEXT mode
  delay(1000);
  
  SerialAT.println("AT+CNMI=2,2,0,0,0");
  delay(1000);
  
  SerialMon.println("\n=== System Ready ===");
  SerialMon.println("WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected"));
  SerialMon.println("Waiting for SMS...\n");
}

String currentLine = "";

void loop() {
  // Check WiFi status periodically
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000) { 
    if (WiFi.status() != WL_CONNECTED) {
      SerialMon.println("WiFi disconnected, reconnecting...");
      connectToWiFi();
    }
    lastWiFiCheck = millis();
  }
  
  // Process incoming data from the modem
  while (SerialAT.available()) {
    char c = SerialAT.read();
    
    if (c == '\n') {
      currentLine.trim();
      
      // Look for the SMS notification prefix
      if (currentLine.startsWith("+CMT:")) {
        SerialMon.println("\n=== NEW SMS RECEIVED ===");
        
        // --- Parse sender and timestamp ---
        // The +CMT: string format is complex, using index of quotes to parse
        int fQuote = currentLine.indexOf('"');
        int sdQuote = currentLine.indexOf('"', fQuote + 1);
        int fifQuote = currentLine.indexOf('"', sdQuote + 1); // start of timestamp
        int sxthQuote = currentLine.indexOf('"', fifQuote + 1); // end of timestamp

        String sender = "";
        String timestamp = "";

        if (fQuote != -1 && sdQuote != -1) {
          sender = currentLine.substring(fQuote + 1, sdQuote);
        }

        if (fifQuote != -1 && sxthQuote != -1) {
          // Timestamp
          timestamp = currentLine.substring(fifQuote + 1, sxthQuote); 
        }

        sender.trim();
        timestamp.trim();

        SerialMon.println("From: " + sender);
        SerialMon.println("Timestamp: " + timestamp);

        delay(50); 
        if (SerialAT.available()) {
          String message = SerialAT.readStringUntil('\n');
          message.trim();

          SerialMon.println("Message: " + message);
          SerialMon.println("========================");

          // send SMS to server
          bool success = sendSMSToServer(sender, message, timestamp);
          if (success) {
            SerialMon.println("SMS forwarded to server successfully!");
          } else {
            SerialMon.println("Failed to forward SMS to server");
          }
        }
      }

      currentLine = "";
    } else if (c != '\r') {
      currentLine += c;
    }
  }
  
  while (Serial.available()) {
    SerialAT.write(Serial.read());
  }
  
  delay(10);
}