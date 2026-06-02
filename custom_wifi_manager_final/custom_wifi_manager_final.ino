#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h> // Library to read/write to ESP32 Flash Memory

WebServer server(80);
Preferences preferences; // Create an instance of Preferences

bool shouldConnectWifi = false;
String targetSSID = "";
String targetPassword = "";
String apName = "ESP32_Device";

// Function to handle Wi-Fi connection logic
bool connectToSavedWiFi(String ssid, String pass) {
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
    Serial.print("Local IP Address: "); Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n[FAILED] Connection timed out.");
    return false;
  }
}

// API Endpoint for your APK: GET /api/scan
void handleApiScan() {
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray networks = doc.to<JsonArray>();

  for (int i = 0; i < n; ++i) {
    JsonObject net = networks.add<JsonObject>();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
    net["encryption"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured";
  }

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  server.send(200, "application/json", jsonResponse);
}

// API Endpoint for your APK: POST /api/wifi
void handleApiWifi() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Missing Body\"}");
    return;
  }
  JsonDocument doc;
  deserializeJson(doc, server.arg("plain"));
  
  targetSSID = doc["ssid"].as<String>();
  targetPassword = doc["password"].as<String>();

  // --- CRITICAL FIX 2: Save to Flash Memory ---
  preferences.begin("wifi-config", false); // Open storage space in read/write mode
  preferences.putString("ssid", targetSSID);
  preferences.putString("password", targetPassword);
  preferences.end();
  Serial.println("\n[FLASH] New credentials saved to flash memory.");

  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Credentials saved. Connecting...\"}");
  shouldConnectWifi = true;
}

void startConfigPortal() {
  // --- CRITICAL FIX 1: Generate Dynamic SSID via MAC Address ---
  String mac = WiFi.macAddress();
  mac.replace(":", ""); 
  apName = "ESP32_" + mac.substring(6, 12); // Takes the last 6 characters of MAC

  WiFi.mode(WIFI_AP_STA); // Set dual mode: can broadcast AP and scan stations
  WiFi.softAP(apName.c_str()); 

  Serial.println("\n=== WiFi Manager Config Portal Active ===");
  Serial.print("SSID: "); Serial.println(apName);
  Serial.print("Portal URL: http://"); Serial.println(WiFi.softAPIP());
  Serial.println("=========================================");

  server.on("/api/scan", HTTP_GET, handleApiScan);
  server.on("/api/wifi", HTTP_POST, handleApiWifi);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  
  // Initialize Flash memory and read stored configurations
  preferences.begin("wifi-config", true); // Open storage space in read-only mode
  String storedSSID = preferences.getString("ssid", "");
  String storedPassword = preferences.getString("password", "");
  preferences.end();

  // If credentials exist in flash, try to auto-connect on startup
  if (storedSSID != "" && storedSSID != NULL) {
    WiFi.mode(WIFI_STA);
    if (connectToSavedWiFi(storedSSID, storedPassword)) {
      // Boot up process complete. Skip starting the manager entirely.
      Serial.println("System Running in Main Mode.");
      return; 
    }
  }

  // If no credentials exist or connection failed, fall back to the config portal
  startConfigPortal();
}

void loop() {
  // Only handle the server clients if the AP mode portal is running
  if (WiFi.getMode() & WIFI_AP) {
    server.handleClient();
  }
  
  if (shouldConnectWifi) {
    shouldConnectWifi = false;
    delay(1000);
    
    // Shut down the portal web server and disconnect AP
    server.stop();
    WiFi.softAPdisconnect(true);
    
    // Attempt to jump onto the newly configured network
    if (connectToSavedWiFi(targetSSID, targetPassword)) {
       WiFi.mode(WIFI_STA); // Switch completely to station client mode
    } else {
       Serial.println("\n[PORTAL] Retrying Config Portal due to bad credentials...");
       startConfigPortal(); // Re-open configuration portal if password was wrong
    }
  }
}