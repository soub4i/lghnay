#include "config.h"

// Library Includes
#include <Wire.h>
#include <TinyGsmClient.h>
#include <HTTPClient.h>
#include <WiFiManager.h>

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

bool connectToWiFi() {
  SerialMon.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  bool res = wm.autoConnect(AP_name, AP_password);
  if (!res) {
    Serial.println("Failed to connect");
    return false;
  } else {
    Serial.println("connected.");
    return true;
  }
}

bool sendSMSToServer(String phoneNumber, String message, String timestamp) {
  if (WiFi.status() != WL_CONNECTED) {
    SerialMon.println("WiFi not connected");
    return false;
  }
  
  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Cisab ") + apikey);
  
  String jsonPayload = "{\"sender\":\"" + phoneNumber + "\",\"sms\":\"" + message + "\",\"ts\":\"" + timestamp + "\"}";
  
  SerialMon.println("Posting to: " + String(serverURL));
  int httpResponseCode = http.POST(jsonPayload);
  
  if (httpResponseCode > 0) {
    SerialMon.print("HTTP Response: ");
    SerialMon.println(httpResponseCode);
    String response = http.getString();
    SerialMon.println("Response: " + response);
    http.end();
    return true;
  } else {
    SerialMon.print("Error code: ");
    SerialMon.println(httpResponseCode);
    http.end();
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  
  SerialMon.println("\n=== Starting System ===");
  SerialMon.print("Free heap: ");
  SerialMon.println(ESP.getFreeHeap());
  
  connectToWiFi();
  
  Wire.begin(I2C_SDA, I2C_SCL);
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));
  
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
  
  if (strlen(simPIN) && modem.getSimStatus() != 3) {
    modem.simUnlock(simPIN);
  }
  
  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
  } else {
    SerialMon.println(" success");
    int signalQuality = modem.getSignalQuality();
    SerialMon.print("Signal quality: ");
    SerialMon.println(signalQuality);
  }
  
  SerialAT.println("AT");
  delay(1000);
  
  SerialAT.println("AT+CMGF=1");
  delay(1000);
  
  SerialAT.println("AT+CNMI=2,2,0,0,0");
  delay(1000);
  
  SerialMon.println("\n=== System Ready ===");
  SerialMon.print("WiFi: ");
  SerialMon.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  SerialMon.print("Free heap: ");
  SerialMon.println(ESP.getFreeHeap());
  SerialMon.println("Waiting for SMS...\n");
}

String currentLine = "";

void loop() {
  while (SerialAT.available()) {
    char c = SerialAT.read();
    
    if (c == '\n') {
      currentLine.trim();
      
      if (currentLine.startsWith("+CMT:")) {
        SerialMon.println("\n=== NEW SMS ===");
        
        int fQuote = currentLine.indexOf('"');
        int sdQuote = currentLine.indexOf('"', fQuote + 1);
        int fifQuote = currentLine.indexOf('"', sdQuote + 1);
        int sxthQuote = currentLine.indexOf('"', fifQuote + 1);
        
        String sender = "";
        String timestamp = "";
        
        if (fQuote != -1 && sdQuote != -1) {
          sender = currentLine.substring(fQuote + 1, sdQuote);
        }
        
        if (fifQuote != -1 && sxthQuote != -1) {
          timestamp = currentLine.substring(fifQuote + 1, sxthQuote);
        }
        
        sender.trim();
        timestamp.trim();
        
        SerialMon.println("From: " + sender);
        SerialMon.println("Time: " + timestamp);
        
        delay(50);
        if (SerialAT.available()) {
          String message = SerialAT.readStringUntil('\n');
          message.trim();
          
          SerialMon.println("Message: " + message);
          
          bool success = sendSMSToServer(sender, message, timestamp);
          SerialMon.println(success ? "✓ Sent" : "✗ Failed");
          SerialMon.println("================\n");
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