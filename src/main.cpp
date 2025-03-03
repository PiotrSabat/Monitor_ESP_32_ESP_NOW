#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "parameters.h"

// Definicje przycisków (używane do interpretacji stanów)
#define BUTTON_X         6
#define BUTTON_Y         2
#define BUTTON_A         5
#define BUTTON_B         1
#define BUTTON_SELECT    0
#define BUTTON_START     16
uint32_t button_mask = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                       (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);
uint32_t button_mask2 = button_mask;

// Globalne zmienne do przechowywania odebranych danych
Message_from_Pad receivedPadData;
Message_from_Platform_Mecanum receivedPlatformData;

// Globalny mutex chroniący dane
SemaphoreHandle_t dataMutex = NULL;

// Zmienne do śledzenia czasu ostatniego odebrania (heartbeat)
unsigned long lastReceivedPadTime = 0;
unsigned long lastReceivedPlatformTime = 0;
const unsigned long DATA_TIMEOUT = 2000; // Timeout 2000 ms (2 sekundy)

// Funkcja porównująca dwa adresy MAC
bool compareMAC(const uint8_t *mac1, const uint8_t *mac2) {
    return memcmp(mac1, mac2, 6) == 0;
}

// ===== CALLBACK: Odbiór danych przez ESP-NOW =====
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    if (compareMAC(mac, macModulXiao)) {
        memcpy(&receivedPadData, data, sizeof(receivedPadData));
        lastReceivedPadTime = millis();  // Aktualizacja timera
    } 
    else if (compareMAC(mac, macPlatformMecanum)) {
        memcpy(&receivedPlatformData, data, sizeof(receivedPlatformData));
        lastReceivedPlatformTime = millis();
    } 
    else {
        Serial.println("❌ Nieznany nadawca!");
    }
    xSemaphoreGive(dataMutex);
}

// ===== TASK: Wyświetlanie danych (displayTask) =====
// Wyświetla dane na terminalu, aktualizując tylko określone linie za pomocą sekwencji ANSI.
// Dodatkowo sprawdza heartbeat – jeśli od ostatniego odbioru minął DATA_TIMEOUT,
// wyświetla komunikat o braku danych.
void displayTask(void *parameter) {
    (void)parameter;
    const TickType_t updatePeriod = pdMS_TO_TICKS(100);  // 10 Hz
    TickType_t lastUpdate = xTaskGetTickCount();
    
    // Numery linii wyświetlania
    const uint8_t headerLine1 = 1;
    const uint8_t headerLine2 = 2;
    const uint8_t headerLine3 = 3;
    const uint8_t padSectionStart = 5;
    const uint8_t platformSectionStart = 15;
    
    // Lokalne kopie danych
    Message_from_Pad localPadData;
    Message_from_Platform_Mecanum localPlatformData;
     Serial.print("\033[2J\033[H"); // Czyści ekran i ustawia kursor na górze
    
    while (1) {
        // Kopiowanie danych chronionych mutexem
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        memcpy(&localPadData, &receivedPadData, sizeof(localPadData));
        memcpy(&localPlatformData, &receivedPlatformData, sizeof(localPlatformData));
        unsigned long padTime = lastReceivedPadTime;
        unsigned long platformTime = lastReceivedPlatformTime;
        xSemaphoreGive(dataMutex);
        
        Serial.print("\033[2J\033[H"); // Czyści ekran i ustawia kursor na górze
        // Sekcja Pada
        Serial.print("L_X = "); Serial.print(localPadData.L_Joystick_x_message);
        Serial.print("\tL_Y = \t"); Serial.print(localPadData.L_Joystick_y_message);
        Serial.print("\tR_X = \t"); Serial.print(localPadData.R_Joystick_x_message);
        Serial.print("\tR_Y = "); Serial.println(localPadData.R_Joystick_y_message);
        
        //przyciski
        Serial.print("L: ");
        if (! (localPadData.L_Joystick_buttons_message & (1UL << BUTTON_A))) {
            Serial.print("Button A pressed\t");
        }
        if (! (localPadData.L_Joystick_buttons_message & (1UL << BUTTON_B))) {
            Serial.print("Button B pressed\t");
        }
        if (! (localPadData.L_Joystick_buttons_message & (1UL << BUTTON_X))) {
            Serial.print("Button X pressed\t");
        }
        if (! (localPadData.L_Joystick_buttons_message & (1UL << BUTTON_Y))) {
            Serial.print("Button Y pressed\t");
        }
        if (! (localPadData.L_Joystick_buttons_message & (1UL << BUTTON_SELECT))) {
            Serial.print("Button SELECT pressed\t");
        }
        if (! (localPadData.L_Joystick_buttons_message & (1UL << BUTTON_START))) {
            Serial.print("Button START pressed\t");
        }
        Serial.println();
        Serial.print("R: \t\t");
        if (! (localPadData.R_Joystick_buttons_message & (1UL << BUTTON_A))) {
            Serial.print("Button A pressed\t");
        }
        if (! (localPadData.R_Joystick_buttons_message & (1UL << BUTTON_B))) {
            Serial.print("Button B pressed\t");
        }
        if (! (localPadData.R_Joystick_buttons_message & (1UL << BUTTON_X))) {
            Serial.print("Button X pressed\t");
        }
        if (! (localPadData.R_Joystick_buttons_message & (1UL << BUTTON_Y))) {
            Serial.print("Button Y pressed\t");
        }
        if (! (localPadData.R_Joystick_buttons_message & (1UL << BUTTON_SELECT))) {
            Serial.print("Button SELECT pressed\t");
        }   
        if (! (localPadData.R_Joystick_buttons_message & (1UL << BUTTON_START))) {
            Serial.print("Button START pressed\t");
        }

        
        
        
        vTaskDelayUntil(&lastUpdate, updatePeriod);
    }
}

void setup() {
    setCpuFrequencyMhz(80); // Zmiana częstotliwości CPU na 80 MHz
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Inicjalizacja ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ Błąd inicjalizacji ESP-NOW!");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);

    // Dodanie peerów – konfigurujemy Nadajniki (używamy MAC adresów z parameters.h)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macModulXiao, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Nie można dodać Pada!");
    }
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, macPlatformMecanum, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Nie można dodać Platformy!");
    }

    // Utworzenie mutexu dla danych
    dataMutex = xSemaphoreCreateMutex();
    if (dataMutex == NULL) {
        Serial.println("❌ Błąd tworzenia mutexu!");
        return;
    }

    // Tworzenie tasku wyświetlania danych
    xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, NULL, 0);

    Serial.println("\n✅ System ESP-NOW + FreeRTOS gotowy!");
     Serial.print("\033[2J\033[H"); // Czyści ekran i ustawia kursor na górze
}

void loop() {
    // Pusta pętla – operacje odbywają się w taskach FreeRTOS
}
