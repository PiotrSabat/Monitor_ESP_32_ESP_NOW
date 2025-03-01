#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <parameters.h>

#define TASK_STACK_SIZE 4096  // Rozmiar stosu dla tasków
#define TASK_PRIORITY 1       // Priorytet tasków

#define BUTTON_X         6
#define BUTTON_Y         2
#define BUTTON_A         5
#define BUTTON_B         1
#define BUTTON_SELECT    0
#define BUTTON_START    16
uint32_t button_mask = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                       (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);

uint32_t button_mask2 = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                       (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);


uint8_t broadcastAddress[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; // MAC docelowego ESP

// Struktura wiadomości
typedef struct struct_message {
    char message[32];
} struct_message;

struct_message msg;

// Kolejka dla wiadomości ESP-NOW
QueueHandle_t espNowQueue;

// ======== CALLBACK: Odbiór danych przez ESP-NOW ==========

// Przechowywane dane
struct_message_from_Pad receivedPadData;
struct_message_from_Platform_Mecanum receivedPlatformData;

// MAC adresy nadawców (musisz uzupełnić właściwe)


// Funkcja porównująca dwa adresy MAC
bool compareMAC(const uint8_t *mac1, const uint8_t *mac2) {
    return memcmp(mac1, mac2, 6) == 0;  
}

void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    //Serial.printf("\nOtrzymano %d bajtów od: %02X:%02X:%02X:%02X:%02X:%02X\n",
    //              len, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (compareMAC(mac, macModulXiao)) {
        memcpy(&receivedPadData, data, sizeof(receivedPadData));
        //Serial.println("📥 Otrzymano dane z Pada");
    } 
    else if (compareMAC(mac, macPlatformMecanum)) {
        memcpy(&receivedPlatformData, data, sizeof(receivedPlatformData));
        //Serial.println("📥 Otrzymano dane z Platformy");
    } 
    else {
        Serial.println("❌ Nieznany nadawca!");
    }
}






// ======== TASK: Wysyłanie przez ESP-NOW ==========
void espNowSendTask(void *parameter) {
    struct_message sendMsg;

    while (1) {
        // Oczekiwanie na dane w kolejce (blokuje task, gdy nie ma danych)
        if (xQueueReceive(espNowQueue, &sendMsg, portMAX_DELAY) == pdTRUE) {
            Serial.printf("🛰️ Wysyłanie: %s\n", sendMsg.message);
            
            esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &sendMsg, sizeof(sendMsg));

            if (result == ESP_OK) {
                Serial.println("✅ Wysłano do ESP-NOW!");
            } else {
                Serial.println("❌ Błąd wysyłania!");
            }
        }
    }
}

// ======== TASK: Odbiór danych z Serial i wysyłanie do kolejki ==========
void serialReceiveTask(void *parameter) {
    struct_message sendMsg;

    while (1) {
        if (Serial.available()) {
            String input = Serial.readStringUntil('\n');
            input.trim();

            if (input.length() > 0) {
                Serial.printf("🖥️ Otrzymano z komputera: %s\n", input.c_str());

                // Kopiowanie danych do struktury
                strncpy(sendMsg.message, input.c_str(), sizeof(sendMsg.message));
                sendMsg.message[sizeof(sendMsg.message) - 1] = '\0';

                // Dodanie wiadomości do kolejki
                xQueueSend(espNowQueue, &sendMsg, portMAX_DELAY);
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // Małe opóźnienie, aby nie blokować CPU
    }
}

// ======== TASK: Wyświetlanie danych z ESP-NOW ==========
void displayTask(void *parameter) {
    while (1) {
        Serial.print("\033[2J\033[H"); // Czyści ekran i ustawia kursor na górze
        
        Serial.println("==========================================");
        Serial.println("               MONITOR ESP-NOW");
        Serial.println("==========================================\n");

        // Sekcja Pada
        Serial.println(">>> DANE PADA <<<");
        Serial.printf("SeqNum:\t\t%u\n", receivedPadData.seqNum);
        Serial.printf("Lewy Joy:\tX: %d\tY: %d\n", receivedPadData.L_Joystick_x_message, receivedPadData.L_Joystick_y_message);
        Serial.printf("Prawy Joy:\tX: %d\tY: %d\n", receivedPadData.R_Joystick_x_message, receivedPadData.R_Joystick_y_message);
        
    if (! (receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_A))) {
      Serial.println("Button L_A pressed");
    }
    if (! (receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_B))) {
      Serial.println("Button L_B pressed");
    }
    if (! (receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_Y))) {
      Serial.println("Button L_Y pressed");
    }
    if (! (receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_X))) {
      Serial.println("Button L_X pressed");
    }
    if (! (receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_SELECT))) {
      Serial.println("Button L_SELECT pressed");
    }
    if (! (receivedPadData.L_Joystick_buttons_message & (1UL << BUTTON_START))) {
      Serial.println("Button L_START pressed");
    }

    if (! (receivedPadData.R_Joystick_buttons_message & (1UL << BUTTON_A))) {
      Serial.println("Button R_A pressed");
    }
    if (! (receivedPadData.R_Joystick_buttons_message & (1UL << BUTTON_B))) {
      Serial.println("Button R_B pressed");
    }
    if (! (receivedPadData.R_Joystick_buttons_message & (1UL << BUTTON_Y))) {
      Serial.println("Button R_Y pressed");
    }
    if (! (receivedPadData.R_Joystick_buttons_message & (1UL << BUTTON_X))) {
      Serial.println("Button R_X pressed");
    }
    if (! (receivedPadData.R_Joystick_buttons_message & (1UL << BUTTON_SELECT))) {
      Serial.println("Button R_SELECT pressed");
    }
    if (! (receivedPadData.R_Joystick_buttons_message & (1UL << BUTTON_START))) {
      Serial.println("Button R_START pressed");
    }
Serial.println();


        Serial.println("\n------------------------------------------\n");

        // Sekcja Platformy
        Serial.println(">>> DANE PLATFORMY <<<");
        Serial.printf("Czas:\t\t%u ms\n", receivedPlatformData.seqNum);
        Serial.printf("Predkosci:\tFL: %d\tFR: %d\tRL: %d\tRR: %d\n", 
                      receivedPlatformData.frontLeftSpeed, 
                      receivedPlatformData.frontRightSpeed, 
                      receivedPlatformData.rearLeftSpeed, 
                      receivedPlatformData.rearRightSpeed);
        Serial.printf("IMU:\t\tPitch: %.2f\tRoll: %.2f\tYaw: %.2f\n", 
                      receivedPlatformData.pitch, 
                      receivedPlatformData.roll, 
                      receivedPlatformData.yaw);
        Serial.printf("Bateria:\t%.2f V\n", receivedPlatformData.batteryVoltage);

        Serial.println("\n==========================================");

        vTaskDelay(100 / portTICK_PERIOD_MS); // Odświeżanie 10 Hz
    }
}


void setup() {
    Serial.begin(115200);

    // Tryb Wi-Fi Station (ale bez połączenia do sieci)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Inicjalizacja ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ Błąd inicjalizacji ESP-NOW!");
        return;
    }

    // Rejestracja callbacka odbiorczego
    esp_now_register_recv_cb(OnDataRecv);

    // Konfiguracja odbiorcy
   // Dodanie Pada jako peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macModulXiao, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;  // WAŻNE! Określenie interfejsu
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Nie można dodać Pada!");
    }

// Dodanie Platformy jako peer
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t)); // Zerowanie struktury
    memcpy(peerInfo.peer_addr, macPlatformMecanum, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Nie można dodać Platformy!");
    }


    // Tworzenie kolejki ESP-NOW
    espNowQueue = xQueueCreate(5, sizeof(struct_message)); 

    if (espNowQueue == NULL) {
        Serial.println("❌ Błąd tworzenia kolejki!");
        return;
    }

    // Tworzenie tasków FreeRTOS
    xTaskCreatePinnedToCore(serialReceiveTask, "SerialTask", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL, 0);
    xTaskCreatePinnedToCore(espNowSendTask, "EspNowTask", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL, 1);
    xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, NULL, 0);


    Serial.println("\n✅ System ESP-NOW + FreeRTOS gotowy!");
}

void loop() {
    // Nic tutaj nie robimy, wszystko działa w taskach FreeRTOS
}
