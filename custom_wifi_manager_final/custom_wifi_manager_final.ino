// ============================================================
//  Custom WiFi Manager — Compatible with ESP32 & ESP8266
//  Detects the target chip automatically via preprocessor guards.
// ================= Dev Branch ===========================================

// ---------- Platform-specific headers ----------
#ifdef ESP32
#include <Preferences.h> // ESP32 NVS flash storage
#include <WebServer.h>
#include <WiFi.h>
WebServer server(80);
Preferences preferences;

#elif defined(ESP8266)
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h> // ESP8266 filesystem-based storage
ESP8266WebServer server(80);

// ---- Tiny key=value helpers backed by LittleFS for ESP8266 ----
String readFile(const char *path) {
  File f = LittleFS.open(path, "r");
  if (!f)
    return "";
  String s = f.readString();
  f.close();
  return s;
}
void writeFile(const char *path, const String &data) {
  File f = LittleFS.open(path, "w");
  if (f) {
    f.print(data);
    f.close();
  }
}
#else
#error "Unsupported platform. Please use an ESP32 or ESP8266 board."
#endif

#include <ArduinoJson.h>

// ---------- Shared globals ----------
bool shouldConnectWifi = false;
String targetSSID = "";
String targetPassword = "";
String apName = "";

// ============================================================
//  Storage helpers — unified API across both platforms
// ============================================================
void storageSave(const String &ssid, const String &pass) {
#ifdef ESP32
  preferences.begin("wifi-config", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", pass);
  preferences.end();
#elif defined(ESP8266)
  writeFile("/wifi_ssid.txt", ssid);
  writeFile("/wifi_password.txt", pass);
#endif
  Serial.println("\n[FLASH] New credentials saved to flash memory.");
}

void storageLoad(String &ssid, String &pass) {
#ifdef ESP32
  preferences.begin("wifi-config", true);
  ssid = preferences.getString("ssid", "");
  pass = preferences.getString("password", "");
  preferences.end();
#elif defined(ESP8266)
  ssid = readFile("/wifi_ssid.txt");
  pass = readFile("/wifi_password.txt");
  ssid.trim();
  pass.trim();
#endif
}

// ============================================================
//  Wi-Fi connection helper
// ============================================================
bool connectToSavedWiFi(const String &ssid, const String &pass) {
  Serial.print("\nAttempting connection to saved Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid.c_str(), pass.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) { // 15-second timeout
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[SUCCESS] Connected to Wi-Fi!");
    Serial.print("Local IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n[FAILED] Connection timed out.");
    return false;
  }
}

// ============================================================
//  API: GET /api/scan — returns nearby networks as JSON
// ============================================================
void handleApiScan() {
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray networks = doc.to<JsonArray>();

  for (int i = 0; i < n; ++i) {
    JsonObject net = networks.add<JsonObject>();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);

    // Encryption type constant differs between platforms
#ifdef ESP32
    bool isOpen = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
#elif defined(ESP8266)
    bool isOpen = (WiFi.encryptionType(i) == ENC_TYPE_NONE);
#endif
    net["encryption"] = isOpen ? "Open" : "Secured";
  }

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  server.send(200, "application/json", jsonResponse);
}

// ============================================================
//  API: POST /api/wifi — receives credentials and saves them
// ============================================================
void handleApiWifi() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Missing Body\"}");
    return;
  }

  JsonDocument doc;
  deserializeJson(doc, server.arg("plain"));

  targetSSID = doc["ssid"].as<String>();
  targetPassword = doc["password"].as<String>();

  storageSave(targetSSID, targetPassword);

  server.send(200, "application/json",
              "{\"status\":\"success\",\"message\":\"Credentials saved. "
              "Connecting...\"}");
  shouldConnectWifi = true;
}

// ============================================================
//  Start configuration portal (AP mode)
// ============================================================
void startConfigPortal() {
  // Dynamic SSID using the last 6 hex digits of the MAC address
  String mac = WiFi.macAddress();
  mac.replace(":", "");

#ifdef ESP32
  apName = "ESP32_" + mac.substring(6, 12);
#elif defined(ESP8266)
  apName = "ESP8266_" + mac.substring(6, 12);
#endif

  WiFi.mode(WIFI_AP_STA); // Dual mode: AP + STA (allows scanning while in AP)
  WiFi.softAP(apName.c_str());

  Serial.println("\n=== WiFi Manager Config Portal Active ===");
  Serial.print("SSID: ");
  Serial.println(apName);
  Serial.print("Portal URL: http://");
  Serial.println(WiFi.softAPIP());
  Serial.println("=========================================");

  server.on("/api/scan", HTTP_GET, handleApiScan);
  server.on("/api/wifi", HTTP_POST, handleApiWifi);
  server.begin();
}

// ============================================================
//  Setup
// ============================================================
void setup() {
  Serial.begin(115200);

#ifdef ESP8266
  // Mount LittleFS; format on first use if the filesystem is blank
  if (!LittleFS.begin()) {
    Serial.println("[LittleFS] Formatting filesystem...");
    LittleFS.format();
    LittleFS.begin();
  }
#endif

  // Load stored credentials from flash
  String storedSSID, storedPassword;
  storageLoad(storedSSID, storedPassword);

  // Auto-connect if credentials exist
  if (storedSSID.length() > 0) {
    WiFi.mode(WIFI_STA);
    if (connectToSavedWiFi(storedSSID, storedPassword)) {
      Serial.println("System Running in Main Mode.");
      return; // Portal not needed — fall straight into loop()
    }
  }

  // No credentials or connection failed → open config portal
  startConfigPortal();
}

// ============================================================
//  Loop
// ============================================================
void loop() {
  // Serve portal clients while AP is active
  if (WiFi.getMode() & WIFI_AP) {
    server.handleClient();

#ifdef ESP8266
    yield(); // ESP8266 requires yield() to keep the watchdog fed
#endif
  }

  // Handle deferred connection request (must happen outside handleApiWifi
  // so the HTTP response is sent before we shut down the server)
  if (shouldConnectWifi) {
    shouldConnectWifi = false;
    delay(1000);

    server.stop();
    WiFi.softAPdisconnect(true);

    if (connectToSavedWiFi(targetSSID, targetPassword)) {
      WiFi.mode(WIFI_STA); // Switch fully to station mode
    } else {
      Serial.println(
          "\n[PORTAL] Retrying Config Portal due to bad credentials...");
      startConfigPortal(); // Re-open portal if password was wrong
    }
  }
}