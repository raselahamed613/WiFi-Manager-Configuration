// ==========================================
// 1. HARDWARE SEPARATION & LIBRARIES
// ==========================================
#if defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  #include <Preferences.h>
  WebServer server(80);
  Preferences preferences;
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  ESP8266WebServer server(80);
#endif

// Global application variables
bool shouldConnectWifi = false;
String targetSSID = "";
String targetPassword = "";
String apName = "";

// ==========================================
// 2. RAW STRING JSON BUILDER (GET /api/scan)
// ==========================================
void handleApiScan() {
  Serial.println("[SCAN] APK requested network scan...");

  #if defined(ESP32)
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    int n = WiFi.scanNetworks(false, false, false, 300);
  #elif defined(ESP8266)
    int n = WiFi.scanNetworks(false, false);
  #endif

  Serial.printf("[SCAN] Found %d networks.\n", n);

  // Manually construct the raw JSON array string
  String jsonResponse = "[";
  
  if (n > 0) {
    for (int i = 0; i < n; ++i) {
      jsonResponse += "{";
      jsonResponse += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
      jsonResponse += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
      
      #if defined(ESP32)
        String encryptType = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured";
      #elif defined(ESP8266)
        String encryptType = (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "Open" : "Secured";
      #endif
      
      jsonResponse += "\"encryption\":\"" + encryptType + "\"";
      jsonResponse += "}";
      
      if (i < n - 1) {
        jsonResponse += ","; // Add comma between array objects
      }
    }
  }
  jsonResponse += "]";

  WiFi.scanDelete(); // Free internal memory

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonResponse);
}

// ==========================================
// 3. RAW STRING JSON PARSER (POST /api/wifi)
// ==========================================
void handleApiWifi() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Missing Body\"}");
    return;
  }
  
  String body = server.arg("plain"); // Looks like: {"ssid":"Name","password":"Pass"}
  
  // Basic manual extraction using string slicing
  int ssidKeyIdx = body.indexOf("\"ssid\"");
  int passKeyIdx = body.indexOf("\"password\"");

  if (ssidKeyIdx == -1 || passKeyIdx == -1) {
    server.send(400, "application/json", "{\"error\":\"Invalid format. Keys missing.\"}");
    return;
  }

  // Extract SSID value string
  int ssidStart = body.indexOf(":", ssidKeyIdx) + 1;
  int ssidQuoteStart = body.indexOf("\"", ssidStart) + 1;
  int ssidQuoteEnd = body.indexOf("\"", ssidQuoteStart);
  targetSSID = body.substring(ssidQuoteStart, ssidQuoteEnd);

  // Extract Password value string
  int passStart = body.indexOf(":", passKeyIdx) + 1;
  int passQuoteStart = body.indexOf("\"", passStart) + 1;
  int passQuoteEnd = body.indexOf("\"", passQuoteStart);
  targetPassword = body.substring(passQuoteStart, passQuoteEnd);

  Serial.println("\n[RAW PARSER] Extracted Payload Successfully:");
  Serial.print("SSID Target: "); Serial.println(targetSSID);

  // Platform Storage Operations
  #if defined(ESP32)
    preferences.begin("wifi-config", false);
    preferences.putString("ssid", targetSSID);
    preferences.putString("password", targetPassword);
    preferences.end();
  #endif

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Credentials parsed raw. Connecting...\"}");
  
  shouldConnectWifi = true;
}

// ==========================================
// 4. CORE CONNECTION ROUTINE
// ==========================================
bool performWiFiConnection(String ssid, String pass) {
  Serial.printf("\nConnecting to network: %s \n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[SUCCESS] Connected to network!");
    Serial.print("IP Address: "); Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n[TIMEOUT] Failed to connect.");
    return false;
  }
}

// ==========================================
// 5. AP ACCESS NETWORK CONFIGURATOR
// ==========================================
void startConfigPortal() {
  Serial.println("\n[PORTAL] Resetting Wi-Fi subsystem state...");
  
  // 1. CRITICAL FIX: Break any stuck connection cycles and wipe the channel lock
  WiFi.disconnect(true, true); 
  delay(200); // Give the radio silicon a brief moment to clear memory registers

  // 2. Explicitly initialize dual access mode
  WiFi.mode(WIFI_AP_STA);
  delay(100);

  #if defined(ESP32)
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    apName = "ESP32_" + mac.substring(6, 12);
  #elif defined(ESP8266)
    String chipId = String(ESP.getChipId(), HEX);
    chipId.toUpperCase();
    apName = "ESP8266_" + chipId;
  #endif

  WiFi.softAP(apName.c_str());

  Serial.println("\n=== Raw String Configuration Portal ===");
  Serial.print("Device SSID: "); Serial.println(apName);
  Serial.print("API Endpoint: http://"); Serial.println(WiFi.softAPIP());
  Serial.println("=======================================");

  server.on("/api/scan", HTTP_GET, handleApiScan);
  server.on("/api/wifi", HTTP_POST, handleApiWifi);
  server.begin();
}

// ==========================================
// 6. SETUP METHOD
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(500);
  
  String savedSSID = "";
  String savedPassword = "";

  #if defined(ESP32)
    Serial.println("\nBooting System... [Platform: ESP32]");
    preferences.begin("wifi-config", true);
    savedSSID = preferences.getString("ssid", "");
    savedPassword = preferences.getString("password", "");
    preferences.end();
    
    if (savedSSID != "") {
      if (performWiFiConnection(savedSSID, savedPassword)) {
        WiFi.mode(WIFI_STA);
        return;
      }
    }
  #elif defined(ESP8266)
    Serial.println("\nBooting System... [Platform: ESP8266]");
    WiFi.mode(WIFI_STA);
    delay(100);
    WiFi.begin(); 

    Serial.print("Polling internal SDK flash configuration");
    int checks = 0;
    while (WiFi.status() != WL_CONNECTED && checks < 20) {
      delay(500);
      Serial.print(".");
      checks++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[SUCCESS] Linked automatically from flash memory variables.");
      return;
    }
  #endif

  startConfigPortal();
}

// ==========================================
// 7. MAIN ENGINE EXECUTION LOOP
// ==========================================
void loop() {
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    server.handleClient();
  }

  if (shouldConnectWifi) {
    shouldConnectWifi = false;
    delay(1000);

    server.stop();
    WiFi.softAPdisconnect(true);

    if (performWiFiConnection(targetSSID, targetPassword)) {
      WiFi.mode(WIFI_STA);
    } else {
      Serial.println("\n[RETRY] Connection dropped. Dropping back to configuration state...");
      startConfigPortal();
    }
  }
}