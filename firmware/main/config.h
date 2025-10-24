#ifndef CONFIG_H
#define CONFIG_H

// ------------------------------------
// 1. USER CONFIGURATION (CHANGE THESE)
// ------------------------------------

// SIM card PIN (leave empty, if none)
const char simPIN[] = "0000";  // CHANGE This

// WiFi credentials
const char* ssid = "";      // CHANGE This
const char* password = "";  // CHANGE This

// API KEY that will be used to authenticate with the server. it should match the one configured on Cloudflare .
const char* apikey = ""; // CHANGE This

// Your server details
const char* serverURL = "https://<project_name>.<cloudflare_username>.workers.dev/set";  // CHANGE This


// ------------------------------------
// 2. LIBRARY/MODEM CONFIGURATION
// ------------------------------------

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER 1024


// ------------------------------------
// 3. HARDWARE PINS (TTGO T-Call)
// ------------------------------------

#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00


// ------------------------------------
// 4. SERIAL DEFINITIONS
// ------------------------------------

#define SerialMon Serial // Main debug serial port
#define SerialAT  Serial1 // Serial port for the GSM modem

#endif // CONFIG_H