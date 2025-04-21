#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
//#include "mac_addresses_private.h"  // Contains the real MAC addresses
#include "mac_addresses.h"       // Contains anonymized MAC addresses
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

// Global variable to store incoming pad data
Message_from_Pad receivedPadData;

// ===== ESP‑NOW RECEIVE CALLBACK =====
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    // Only copy into our structure if the incoming packet size matches
    if (len == sizeof(receivedPadData)) {
        memcpy(&receivedPadData, incomingData, sizeof(receivedPadData));
        Serial.println("✅ Received data from pad!");
    }
}

// ===== WEBSOCKET STREAMING TASK =====
void handleWebSocketMessage(void *parameter) {
    const TickType_t delayTime = pdMS_TO_TICKS(20);  // 50 Hz update rate

    while (true) {
        if (ws.count() > 0) {
            DynamicJsonDocument json(512);

            // Fill JSON with pad data
            json["timestamp"]      = receivedPadData.timeStamp;
            json["totalMessages"]  = receivedPadData.messageSequenceNumber;
            json["L_X"]            = receivedPadData.L_Joystick_x_message;
            json["L_Y"]            = receivedPadData.L_Joystick_y_message;
            json["R_X"]            = receivedPadData.R_Joystick_x_message;
            json["R_Y"]            = receivedPadData.R_Joystick_y_message;

            // Left-pad button states (true = pressed)
            json["L_Button_A"]      = !(receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_A));
            json["L_Button_B"]      = !(receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_B));
            json["L_Button_X"]      = !(receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_X));
            json["L_Button_Y"]      = !(receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_Y));
            json["L_Button_SELECT"] = !(receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_SELECT));
            json["L_Button_START"]  = !(receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_START));

            // Right-pad button states
            json["R_Button_A"]      = !(receivedPadData.R_Joystick_buttons_message & (1UL << BUTTON_A));
            json["R_Button_B"]      = !(receivedPadData.R_Joystick_buttons_message & (1UL << BUTTON_B));
            json["R_Button_X"]      = !(receivedPadData.R_Joystick_buttons_message & (1UL << BUTTON_X));
            json["R_Button_Y"]      = !(receivedPadData.R_Joystick_buttons_message & (1UL << BUTTON_Y));
            json["R_Button_SELECT"] = !(receivedPadData.R_Joystick_buttons_message & (1UL << BUTTON_SELECT));
            json["R_Button_START"]  = !(receivedPadData.R_Joystick_buttons_message & (1UL << BUTTON_START));

            // Serialize JSON to a string
            String jsonString;
            serializeJson(json, jsonString);

            // Broadcast JSON string over all WebSocket clients
            ws.textAll(jsonString);
        }
        vTaskDelay(delayTime);
    }
}

// ===== SETUP =====
void setup() {
    Serial.begin(115200);

    // Initialize Wi‑Fi in both AP and STA modes, start soft AP
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssid, password);
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    // Initialize ESP‑NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP‑NOW Init Failed");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);

    // Set up WebSocket event handler
    ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                  AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            Serial.println("WebSocket client connected");
        }
    });
    server.addHandler(&ws);

    // Serve the HTML UI at the root path
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>ESP32 WebSocket</title>
    <style>
        /* Simple responsive grid layout for data cards */
        body { font-family: Arial, sans-serif; background: #f4f4f9; color: #333; margin: 20px; }
        .container { display: grid; grid-template-columns: repeat(2,1fr); gap: 10px; max-width: 500px; margin: auto; }
        .card { background: #fff; border: 1px solid #ccc; border-radius: 8px; padding: 12px; box-shadow: 2px 2px 12px rgba(0,0,0,0.1); text-align: center; }
        h1 { text-align: center; color: #4CAF50; }
    </style>
</head>
<body>
    <h1>ESP32 WebSocket Data</h1>
    <div class="container">
        <!-- Data cards: values will be updated by JavaScript -->
        <div class="card" id="lx">L_X: --</div>
        <div class="card" id="ly">L_Y: --</div>
        <div class="card" id="rx">R_X: --</div>
        <div class="card" id="ry">R_Y: --</div>
        <div class="card" id="timestamp">Timestamp: --</div>
        <div class="card" id="button_L_a">Button L_A: --</div>
        <div class="card" id="button_L_b">Button L_B: --</div>
        <div class="card" id="button_L_x">Button L_X: --</div>
        <div class="card" id="button_L_y">Button L_Y: --</div>
        <div class="card" id="button_L_select">Button L_Select: --</div>
        <div class="card" id="button_L_start">Button L_Start: --</div>
        <div class="card" id="button_R_a">Button R_A: --</div>
        <div class="card" id="button_R_b">Button R_B: --</div>
        <div class="card" id="button_R_x">Button R_X: --</div>
        <div class="card" id="button_R_y">Button R_Y: --</div>
        <div class="card" id="button_R_select">Button R_Select: --</div>
        <div class="card" id="button_R_start">Button R_Start: --</div>
    </div>
    <script>
        // Open WebSocket to the same host on path /ws
        const ws = new WebSocket('ws://' + window.location.host + '/ws');
        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                document.getElementById('lx').innerText            = 'L_X: ' + data.L_X;
                document.getElementById('ly').innerText            = 'L_Y: ' + data.L_Y;
                document.getElementById('rx').innerText            = 'R_X: ' + data.R_X;
                document.getElementById('ry').innerText            = 'R_Y: ' + data.R_Y;
                document.getElementById('timestamp').innerText     = 'Timestamp: ' + data.timestamp;
                // Update button labels based on boolean pressed state
                document.getElementById('button_L_a').innerText     = 'Button L_A: ' + (data.L_Button_A ? 'Pressed' : 'Released');
                document.getElementById('button_L_b').innerText     = 'Button L_B: ' + (data.L_Button_B ? 'Pressed' : 'Released');
                document.getElementById('button_L_x').innerText     = 'Button L_X: ' + (data.L_Button_X ? 'Pressed' : 'Released');
                document.getElementById('button_L_y').innerText     = 'Button L_Y: ' + (data.L_Button_Y ? 'Pressed' : 'Released');
                document.getElementById('button_L_select').innerText = 'Button L_Select: ' + (data.L_Button_SELECT ? 'Pressed' : 'Released');
                document.getElementById('button_L_start').innerText  = 'Button L_Start: ' + (data.L_Button_START ? 'Pressed' : 'Released');

                document.getElementById('button_R_a').innerText     = 'Button R_A: ' + (data.R_Button_A ? 'Pressed' : 'Released');
                document.getElementById('button_R_b').innerText     = 'Button R_B: ' + (data.R_Button_B ? 'Pressed' : 'Released');
                document.getElementById('button_R_x').innerText     = 'Button R_X: ' + (data.R_Button_X ? 'Pressed' : 'Released');
                document.getElementById('button_R_y').innerText     = 'Button R_Y: ' + (data.R_Button_Y ? 'Pressed' : 'Released');
                document.getElementById('button_R_select').innerText = 'Button R_Select: ' + (data.R_Button_SELECT ? 'Pressed' : 'Released');
                document.getElementById('button_R_start').innerText  = 'Button R_Start: ' + (data.R_Button_START ? 'Pressed' : 'Released');
            } catch (error) {
                console.error('JSON Parsing Error:', error);
            }
        };
    </script>
</body>
</html>
)rawliteral");
    });

    server.begin();

    // Create a FreeRTOS task pinned to core 1 for handling WebSocket broadcasts
    xTaskCreatePinnedToCore(
        handleWebSocketMessage, // Task function
        "WebSocketTask",        // Task name
        4096,                   // Stack size
        NULL,                   // Parameter
        1,                      // Priority
        NULL,                   // Task handle
        1                       // Core number
    );

    Serial.println("✅ ESP‑NOW + WebSocket server started");
}

void loop() {
    // Periodically clean up disconnected WebSocket clients
    ws.cleanupClients();
}
