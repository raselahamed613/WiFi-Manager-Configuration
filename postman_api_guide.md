# 📮 Postman API Test Guide — Water Pump Controller

> [!IMPORTANT]
> **Before testing:** Your computer/phone must be connected to the ESP's AP WiFi network named **`PumpCtrl_XXXXXX`** (no password). The base URL will always be `http://192.168.4.1`

---

## 🔧 Postman Setup (One-time)

1. Open Postman → **New Collection** → name it `ESP Pump Controller`
2. Set a **Collection Variable**:
   - Variable: `baseUrl`
   - Value: `http://192.168.4.1`
3. Use `{{baseUrl}}` in all request URLs below

---

## 📡 Endpoint 1 — GET /api/status

Check if the ESP is in AP mode or connected to WiFi.

| Field | Value |
|---|---|
| **Method** | `GET` |
| **URL** | `http://192.168.4.1/api/status` |
| **Headers** | *(none needed)* |
| **Body** | *(none)* |

**Expected Response (AP mode):**
```json
{
  "connected": false,
  "mode": "AP",
  "ip": "192.168.4.1",
  "apSSID": "PumpCtrl_A1B2C3",
  "rssi": 0
}
```

**Expected Response (after WiFi connect):**
```json
{
  "connected": true,
  "mode": "STA",
  "ip": "192.168.1.105",
  "apSSID": "PumpCtrl_A1B2C3",
  "rssi": -62
}
```

---

## 📡 Endpoint 2 — GET /api/scan

Scan for available WiFi networks near the ESP.

| Field | Value |
|---|---|
| **Method** | `GET` |
| **URL** | `http://192.168.4.1/api/scan` |
| **Headers** | *(none needed)* |
| **Body** | *(none)* |

> [!NOTE]
> This may take 3–5 seconds to respond while the ESP scans.

**Expected Response:**
```json
[
  { "ssid": "MyHomeWiFi",  "rssi": -48, "encryption": "Secured" },
  { "ssid": "NeighborNet", "rssi": -72, "encryption": "Secured" },
  { "ssid": "GuestOpen",   "rssi": -81, "encryption": "Open"    }
]
```

---

## 🚀 Endpoint 3 — POST /api/setup *(Most Important)*

Send **ALL configuration** at once — WiFi credentials + all device parameters. ESP saves everything to flash and connects to WiFi.

| Field | Value |
|---|---|
| **Method** | `POST` |
| **URL** | `http://192.168.4.1/api/setup` |
| **Header** | `Content-Type: application/json` |
| **Body Type** | `raw → JSON` |

**Request Body:**
```json
{
  "ssid":          "YourWiFiName",
  "password":      "YourWiFiPassword",
  "tankHeight":    100.0,
  "sensorGap":     5.0,
  "pumpOnLevel":   20.0,
  "pumpOffLevel":  90.0,
  "autoMode":      true,
  "timerValue":    30.0,
  "unitCostPrice": 8.0
}
```

**Postman Steps:**
1. Method → `POST`
2. URL → `http://192.168.4.1/api/setup`
3. Go to **Body** tab → select **raw** → choose **JSON** from dropdown
4. Paste the JSON above (edit your WiFi name/password)
5. Click **Send**

**Expected Response `200 OK`:**
```json
{
  "status": "success",
  "message": "All config saved! ESP connecting to WiFi..."
}
```

> [!WARNING]
> After this response, the ESP will **disconnect from AP** and connect to your router WiFi. Your Postman/computer will lose connection to `192.168.4.1`. This is normal! After ~5 seconds, check your router's DHCP list for the new ESP IP.

---

## ⚙️ Endpoint 4 — GET /api/config

Read the currently saved device parameters from flash.

| Field | Value |
|---|---|
| **Method** | `GET` |
| **URL** | `http://192.168.4.1/api/config` |
| **Headers** | *(none needed)* |
| **Body** | *(none)* |

**Expected Response:**
```json
{
  "tankHeight":    100.0,
  "sensorGap":     5.0,
  "pumpOnLevel":   20.0,
  "pumpOffLevel":  90.0,
  "autoMode":      true,
  "timerValue":    30.0,
  "unitCostPrice": 8.0
}
```

---

## ⚙️ Endpoint 5 — POST /api/config

Update **device parameters only** (does NOT change WiFi credentials).

| Field | Value |
|---|---|
| **Method** | `POST` |
| **URL** | `http://192.168.4.1/api/config` |
| **Header** | `Content-Type: application/json` |
| **Body Type** | `raw → JSON` |

**Request Body:**
```json
{
  "tankHeight":    120.0,
  "sensorGap":     8.0,
  "pumpOnLevel":   15.0,
  "pumpOffLevel":  85.0,
  "autoMode":      false,
  "timerValue":    45.0,
  "unitCostPrice": 9.50
}
```

**Expected Response `200 OK`:**
```json
{
  "status": "success",
  "message": "Device config updated!"
}
```

---

## 📦 Import Collection (One Click)

Save this as `PumpController.postman_collection.json` and **Import** into Postman:

```json
{
  "info": {
    "name": "ESP Pump Controller",
    "_postman_id": "esp-pump-ctrl-001",
    "schema": "https://schema.getpostman.com/json/collection/v2.1.0/collection.json"
  },
  "variable": [{ "key": "baseUrl", "value": "http://192.168.4.1" }],
  "item": [
    {
      "name": "1. GET Status",
      "request": { "method": "GET", "url": "{{baseUrl}}/api/status" }
    },
    {
      "name": "2. GET Scan WiFi",
      "request": { "method": "GET", "url": "{{baseUrl}}/api/scan" }
    },
    {
      "name": "3. POST Setup All",
      "request": {
        "method": "POST",
        "url": "{{baseUrl}}/api/setup",
        "header": [{ "key": "Content-Type", "value": "application/json" }],
        "body": {
          "mode": "raw",
          "raw": "{\n  \"ssid\": \"YourWiFiName\",\n  \"password\": \"YourWiFiPassword\",\n  \"tankHeight\": 100.0,\n  \"sensorGap\": 5.0,\n  \"pumpOnLevel\": 20.0,\n  \"pumpOffLevel\": 90.0,\n  \"autoMode\": true,\n  \"timerValue\": 30.0,\n  \"unitCostPrice\": 8.0\n}",
          "options": { "raw": { "language": "json" } }
        }
      }
    },
    {
      "name": "4. GET Device Config",
      "request": { "method": "GET", "url": "{{baseUrl}}/api/config" }
    },
    {
      "name": "5. POST Update Config",
      "request": {
        "method": "POST",
        "url": "{{baseUrl}}/api/config",
        "header": [{ "key": "Content-Type", "value": "application/json" }],
        "body": {
          "mode": "raw",
          "raw": "{\n  \"tankHeight\": 120.0,\n  \"sensorGap\": 8.0,\n  \"pumpOnLevel\": 15.0,\n  \"pumpOffLevel\": 85.0,\n  \"autoMode\": false,\n  \"timerValue\": 45.0,\n  \"unitCostPrice\": 9.50\n}",
          "options": { "raw": { "language": "json" } }
        }
      }
    }
  ]
}
```

---

## 🔄 Recommended Test Order

```
1. Flash firmware to ESP
2. Connect PC/phone to "PumpCtrl_XXXXXX" WiFi
3. GET  /api/status   → confirm AP mode
4. GET  /api/scan     → find your home WiFi SSID
5. POST /api/setup    → send all config (edit ssid/password first!)
6. Wait ~5 sec → ESP connects to home WiFi
7. Find new IP from router → update baseUrl
8. GET  /api/status   → confirm STA mode + new IP
9. GET  /api/config   → verify all params saved correctly
10. POST /api/config  → test updating device params only
```
