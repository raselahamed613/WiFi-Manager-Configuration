#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

WebServer server(80);

bool shouldConnectWifi = false;
String targetSSID = "";
String targetPassword = "";

// 1. Serving a visual web page for testing via Browser
void handleRoot() {
  String html = "<html><head><title>ESP32 WiFi Manager</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:Arial; margin:20px;} input,select{width:100%; padding:10px; margin:8px 0;} button{padding:10px; width:100%; background:#007bff; color:#fff; border:none;}</style>";
  html += "</head><body>";
  html += "<h2>ESP32 WiFi Configuration</h2>";
  html += "<form action='/config_browser' method='POST'>";
  
  html += "<label>Select Network:</label>";
  html += "<select name='ssid'>";
  
  // Scan networks dynamically for the HTML page
  int n = WiFi.scanNetworks();
  if (n == 0) {
    html += "<option value=''>No networks found</option>";
  } else {
    for (int i = 0; i < n; ++i) {
      html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + "dBm)</option>";
    }
  }
  html += "</select>";
  
  html += "<label>Password:</label><input type='password' name='password'>";
  html += "<button type='submit'>Connect ESP32</button>";
  html += "</form></body></html>";
  
  server.send(200, "text/html", html);
}

// Browser form handler
void handleBrowserConfig() {
  if (server.hasArg("ssid")) {
    targetSSID = server.arg("ssid");
    targetPassword = server.arg("password");
    server.send(200, "text/html", "<h3>Credentials Saved! ESP32 is connecting. You can close this tab.</h3>");
    shouldConnectWifi = true;
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// 2. API Endpoint for your APK: GET /api/scan
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

// 3. API Endpoint for your APK: POST /api/wifi
void handleApiWifi() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Missing Body\"}");
    return;
  }
  JsonDocument doc;
  deserializeJson(doc, server.arg("plain"));
  
  targetSSID = doc["ssid"].as<String>();
  targetPassword = doc["password"].as<String>();

  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Connecting...\"}");
  shouldConnectWifi = true;
}

void setup() {
  Serial.begin(115200);
  
  // Generate unique SSID
  String mac = WiFi.macAddress();
  mac.replace(":", ""); 
  String apName = "ESP32_" + mac.substring(6, 12);
  
  WiFi.softAP(apName.c_str()); 

  Serial.println("\n=== WiFi Manager Ready ===");
  Serial.print("SSID: "); Serial.println(apName);
  Serial.println("==========================");

  // Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/config_browser", HTTP_POST, handleBrowserConfig);
  server.on("/api/scan", HTTP_GET, handleApiScan);
  server.on("/api/wifi", HTTP_POST, handleApiWifi);

  server.begin();
}

void loop() {
  server.handleClient();
  
  if (shouldConnectWifi) {
    shouldConnectWifi = false;
    delay(1000);
    WiFi.softAPdisconnect(true);
    WiFi.begin(targetSSID.c_str(), targetPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[SUCCESS] Connected!");
      Serial.print("IP: "); Serial.println(WiFi.localIP());
    } else {
      Serial.println("\n[FAILED] Resetting...");
      delay(1000);
      ESP.restart();
    }
  }
}