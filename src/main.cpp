

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "parameters.h"

// ===== DEFINICJE PRZYCISKÓW =====
#define BUTTON_X         6
#define BUTTON_Y         2
#define BUTTON_A         5
#define BUTTON_B         1
#define BUTTON_SELECT    0
#define BUTTON_START     16

// ====== WiFi Credentials ======
const char* ssid = "ESP32-Network";
const char* password = "12345678";

// ====== Global Variables ======
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Zmienna globalna na odebrane dane
Message_from_Pad receivedPadData;

// ====== ESP-NOW Callback ======
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len == sizeof(receivedPadData)) {
        memcpy(&receivedPadData, incomingData, sizeof(receivedPadData));
        Serial.println("✅ Dane z pada odebrane!");
    }
}

// ====== WebSocket Handler ======
void handleWebSocketMessage(void *parameter) {
    const TickType_t delayTime = pdMS_TO_TICKS(20);  // 50Hz odświeżanie

    while (true) {
        if (ws.count() > 0) {
            DynamicJsonDocument json(512);

            // Dane z pada
            json["timestamp"] = receivedPadData.timestamp;
            json["L_X"] = receivedPadData.L_Joystick_x_message;
            json["L_Y"] = receivedPadData.L_Joystick_y_message;
            json["R_X"] = receivedPadData.R_Joystick_x_message;
            json["R_Y"] = receivedPadData.R_Joystick_y_message;

            // Stany przycisków
            json["L_Button_A"] = !(receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_A));
            json["L_Button_B"] = !(receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_B));
            json["L_Button_X"] = !(receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_X));
            json["L_Button_Y"] = !(receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_Y));
            json["L_Button_SELECT"] = !(receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_SELECT));
            json["L_Button_START"] = !(receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_START));

            // Serializacja JSON → tekst
            String jsonString;
            serializeJson(json, jsonString);

            // Wysłanie JSON przez WebSocket
            ws.textAll(jsonString);
        }
        vTaskDelay(delayTime);
    }
}

// ====== Setup ======
void setup() {
    Serial.begin(115200);

    // Inicjalizacja Wi-Fi
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssid, password);
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    // Inicjalizacja ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW Init Failed");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);

    // Inicjalizacja WebSocket
    ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                  AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            Serial.println("WebSocket client connected");
        }
    });
    server.addHandler(&ws);

    // Start serwera HTTP
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html",
            "<html><body>"
            "<h1>ESP32 WebSocket</h1>"
            "<div id='data'></div>"
            "<script>"
            "const ws = new WebSocket('ws://' + window.location.host + '/ws');"
            "ws.onmessage = e => document.getElementById('data').innerText = e.data;"
            "</script>"
            "</body></html>");
    });

    server.begin();

    // Utworzenie FreeRTOS Task do obsługi WebSocketów
    xTaskCreatePinnedToCore(
        handleWebSocketMessage, // Funkcja taska
        "WebSocketTask",        // Nazwa taska
        4096,                   // Rozmiar stosu
        NULL,                   // Parametr
        1,                      // Priorytet
        NULL,                   // Uchwyt taska
        1                       // Rdzeń
    );

    Serial.println("✅ ESP-NOW + WebSocket Server uruchomione");
}

void loop() {
    ws.cleanupClients(); // Czyszczenie połączeń WebSocket
}
