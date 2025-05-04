#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "mac_addresses_private.h"       // Contains device MAC addresses
#include "messages.h"

// ===== BUTTON DEFINITIONS =====
#define BUTTON_X      6
#define BUTTON_Y      2
#define BUTTON_A      5
#define BUTTON_B      1
#define BUTTON_SELECT 0
#define BUTTON_START  16

// ===== Wi‑Fi Credentials =====
const char* ssid     = "ESP32-Network";
const char* password = "12345678";

// ===== GLOBAL OBJECTS =====
AsyncWebServer server(80);
AsyncWebSocket  ws("/ws");

// Global variables for incoming data
Message_from_Pad receivedPadData;
Message_from_Platform_Mecanum receivedPlatformData;  // Telemetry from mecanum platform

// ===== ESP‑NOW RECEIVE CALLBACK =====
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len == sizeof(receivedPadData)) {
        memcpy(&receivedPadData, incomingData, sizeof(receivedPadData));
        //Serial.println("✅ Received data from pad!");
    }
    else if (len == sizeof(receivedPlatformData)) {
        memcpy(&receivedPlatformData, incomingData, sizeof(receivedPlatformData));
        //Serial.println("✅ Received data from platform!");
    }
}

// ===== WEBSOCKET STREAMING TASK =====
void handleWebSocketMessage(void *parameter) {
    const TickType_t delayTime = pdMS_TO_TICKS(40);  // 25 Hz update

    while (true) {
        if (ws.count() > 0) {
            DynamicJsonDocument json(1024);

            // --- Pad data ---
            json["timestamp"]     = receivedPadData.timeStamp;
            json["totalMessages"] = receivedPadData.messageSequenceNumber;
            json["L_X"]           = receivedPadData.L_Joystick_x_message;
            json["L_Y"]           = receivedPadData.L_Joystick_y_message;
            json["R_X"]           = receivedPadData.R_Joystick_x_message;
            json["R_Y"]           = receivedPadData.R_Joystick_y_message;
            // Buttons omitted for brevity (same as before)

            // --- Platform data ---
            json["P_timestamp"]     = receivedPlatformData.timestamp;
            json["P_totalMessages"] = receivedPlatformData.totalMessages;
            json["P_FL_RPM"]        = receivedPlatformData.frontLeftSpeedRPM;
            json["P_FR_RPM"]        = receivedPlatformData.frontRightSpeedRPM;
            json["P_RL_RPM"]        = receivedPlatformData.rearLeftSpeedRPM;
            json["P_RR_RPM"]        = receivedPlatformData.rearRightSpeedRPM;
            json["P_taskTime"]      = receivedPlatformData.taskTime;

            // Serialize JSON and broadcast
            String jsonString;
            serializeJson(json, jsonString);
            ws.textAll(jsonString);
        }
        vTaskDelay(delayTime);
    }
}

// ===== SETUP =====
void setup() {
    Serial.begin(115200);

    // Wi‑Fi AP+STA
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssid, password);
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    // ESP‑NOW init
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP‑NOW Init Failed");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);

    // WebSocket handler
    ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                  AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            Serial.println("WebSocket client connected");
        }
    });
    server.addHandler(&ws);

    // Serve HTML UI
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>ESP32 WebSocket</title>
    <style>
        body { font-family: Arial, sans-serif; background: #f4f4f9; color: #333; margin: 20px; }
        .container { display: grid; grid-template-columns: repeat(3,1fr); gap: 10px; max-width: 700px; margin: auto; }
        .card { background: #fff; border: 1px solid #ccc; border-radius: 8px; padding: 12px; box-shadow: 2px 2px 12px rgba(0,0,0,0.1); text-align: center; }
        h1 { text-align: center; color: #4CAF50; }
    </style>
</head>
<body>
    <h1>ESP32 WebSocket Data</h1>
    <div class="container">
        <!-- Pad data cards -->
        <div class="card" id="lx">L_X: --</div>
        <div class="card" id="ly">L_Y: --</div>
        <div class="card" id="timestamp">Timestamp: --</div>
        <!-- Platform data cards -->
        <div class="card" id="p_timestamp">P_Timestamp: --</div>
        <div class="card" id="p_totalMessages">P_TotalMsgs: --</div>
        <div class="card" id="p_fl_rpm">P_FL_RPM: --</div>
        <div class="card" id="p_fr_rpm">P_FR_RPM: --</div>
        <div class="card" id="p_rl_rpm">P_RL_RPM: --</div>
        <div class="card" id="p_rr_rpm">P_RR_RPM: --</div>
        <div class="card" id="p_taskTime">P_TaskTime (µs): --</div>
    </div>
    <script>
        const ws = new WebSocket('ws://' + window.location.host + '/ws');
        ws.onmessage = (event) => {
            const data = JSON.parse(event.data);
            document.getElementById('lx').innerText = 'L_X: ' + data.L_X;
            document.getElementById('ly').innerText = 'L_Y: ' + data.L_Y;
            document.getElementById('timestamp').innerText = 'Timestamp: ' + data.timestamp;
            // Platform updates
            document.getElementById('p_timestamp').innerText     = 'P_Timestamp: ' + data.P_timestamp;
            document.getElementById('p_totalMessages').innerText = 'P_TotalMsgs: ' + data.P_totalMessages;
            document.getElementById('p_fl_rpm').innerText        = 'P_FL_RPM: ' + data.P_FL_RPM.toFixed(1);
            document.getElementById('p_fr_rpm').innerText        = 'P_FR_RPM: ' + data.P_FR_RPM.toFixed(1);
            document.getElementById('p_rl_rpm').innerText        = 'P_RL_RPM: ' + data.P_RL_RPM.toFixed(1);
            document.getElementById('p_rr_rpm').innerText        = 'P_RR_RPM: ' + data.P_RR_RPM.toFixed(1);
            document.getElementById('p_taskTime').innerText     = 'P_TaskTime (µs): ' + data.P_taskTime.toFixed(1);
        };
    </script>
</body>
</html>
)rawliteral");
    });

    server.begin();

    // WebSocket broadcasting task
    xTaskCreatePinnedToCore(
        handleWebSocketMessage,
        "WebSocketTask",
        8192,
        NULL,
        1,
        NULL,
        1
    );

    Serial.println("✅ ESP‑NOW + WebSocket server started");
}

void loop() {
    ws.cleanupClients();
}
