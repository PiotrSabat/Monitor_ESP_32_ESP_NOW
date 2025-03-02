#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <parameters.h>

// Definicje stosu i priorytetu tasków
#define TASK_STACK_SIZE 4096  
#define TASK_PRIORITY 1       

// Przyciski – maski (używane do późniejszej interpretacji stanów)
#define BUTTON_X         6
#define BUTTON_Y         2
#define BUTTON_A         5
#define BUTTON_B         1
#define BUTTON_SELECT    0
#define BUTTON_START     16
uint32_t button_mask = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                       (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);
uint32_t button_mask2 = button_mask;

// Adres rozgłoszeniowy – do wysyłania ESP-NOW
uint8_t broadcastAddress[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// Struktura do wysyłania prostych wiadomości tekstowych
typedef struct struct_message {
    char message[32];
} struct_message;
struct_message msg;

// Kolejka dla wiadomości wysyłanych przez ESP-NOW
QueueHandle_t espNowQueue = NULL;

// Globalne zmienne do przechowywania odebranych danych
struct_message_from_Pad receivedPadData;
struct_message_from_Platform_Mecanum receivedPlatformData;

// Mutex dla ochrony globalnych struktur danych (pad i platforma)
SemaphoreHandle_t dataMutex = NULL;

// Funkcja porównująca dwa adresy MAC
bool compareMAC(const uint8_t *mac1, const uint8_t *mac2) {
    return memcmp(mac1, mac2, 6) == 0;  
}

// ===== CALLBACK: Odbiór danych przez ESP-NOW =====
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    // Chronimy globalne zmienne mutexem, aby uniknąć wyścigu
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    if (compareMAC(mac, macModulXiao)) {
        memcpy(&receivedPadData, data, sizeof(receivedPadData));
        // Można dodać dodatkową diagnostykę, np. Serial.println("Dane z pada odebrane");
    } 
    else if (compareMAC(mac, macPlatformMecanum)) {
        memcpy(&receivedPlatformData, data, sizeof(receivedPlatformData));
        // Serial.println("Dane z platformy odebrane");
    } 
    else {
        Serial.println("❌ Nieznany nadawca!");
    }
    xSemaphoreGive(dataMutex);
}

// ===== TASK: Wysyłanie przez ESP-NOW =====
void espNowSendTask(void *parameter) {
    struct_message sendMsg;
    while (1) {
        // Oczekiwanie na dane w kolejce (blokuje task, gdy nie ma danych)
        if (xQueueReceive(espNowQueue, &sendMsg, portMAX_DELAY) == pdTRUE) {
            Serial.printf("🛰️ Wysyłanie: %s\n", sendMsg.message);
            esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&sendMsg, sizeof(sendMsg));
            if (result == ESP_OK) {
                Serial.println("✅ Wysłano do ESP-NOW!");
            } else {
                Serial.println("❌ Błąd wysyłania!");
            }
        }
    }
}

// ===== TASK: Odbiór danych z Serial i wysyłanie do kolejki =====
void serialReceiveTask(void *parameter) {
    struct_message sendMsg;
    while (1) {
        if (Serial.available()) {
            String input = Serial.readStringUntil('\n');
            input.trim();
            if (input.length() > 0) {
                Serial.printf("🖥️ Otrzymano z komputera: %s\n", input.c_str());
                // Kopiowanie danych do struktury (zapewnienie zakończenia łańcucha)
                strncpy(sendMsg.message, input.c_str(), sizeof(sendMsg.message));
                sendMsg.message[sizeof(sendMsg.message) - 1] = '\0';
                // Dodanie wiadomości do kolejki
                xQueueSend(espNowQueue, &sendMsg, portMAX_DELAY);
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // Małe opóźnienie, aby nie blokować CPU
    }
}

// ===== TASK: Wyświetlanie danych z ESP-NOW =====
void displayTask(void *parameter) {
    const TickType_t updatePeriod = pdMS_TO_TICKS(100);  // 10 Hz
    TickType_t lastUpdate = xTaskGetTickCount();
    
    // Numery linii do wyświetlania danych
    const uint8_t headerLine1 = 1;
    const uint8_t headerLine2 = 2;
    const uint8_t headerLine3 = 3;
    const uint8_t padSectionStart = 5;      // Sekcja Pada
    const uint8_t platformSectionStart = 15; // Sekcja Platformy

    // Lokalne kopie danych, aby nie trzymać mutexu przez cały czas wyświetlania
    struct_message_from_Pad localPadData;
    struct_message_from_Platform_Mecanum localPlatformData;
    Serial.print("\033[2J\033[H"); // Czyści ekran i ustawia kursor na górze

    while (1) {
        // Skopiuj globalne dane do lokalnych zmiennych
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        memcpy(&localPadData, &receivedPadData, sizeof(localPadData));
        memcpy(&localPlatformData, &receivedPlatformData, sizeof(localPlatformData));
        xSemaphoreGive(dataMutex);

        // Aktualizacja nagłówka
        Serial.print("\033["); Serial.print(headerLine1); Serial.print(";1H\033[2K");
        Serial.println("==========================================");
        Serial.print("\033["); Serial.print(headerLine2); Serial.print(";1H\033[2K");
        Serial.println("               MONITOR ESP-NOW");
        Serial.print("\033["); Serial.print(headerLine3); Serial.print(";1H\033[2K");
        Serial.println("==========================================\n");

        // Sekcja Pada
        Serial.print("\033["); Serial.print(padSectionStart); Serial.print(";1H\033[2K");
        Serial.println(">>> DANE PADA <<<");
        Serial.print("\033["); Serial.print(padSectionStart + 1); Serial.print(";1H\033[2K");
        Serial.printf("SeqNum:\t\t%u\n", localPadData.seqNum);
        Serial.print("\033["); Serial.print(padSectionStart + 2); Serial.print(";1H\033[2K");
        Serial.printf("Lewy Joy:\tX: %d\tY: %d\n", localPadData.L_Joystick_x_message, localPadData.L_Joystick_y_message);
        Serial.print("\033["); Serial.print(padSectionStart + 3); Serial.print(";1H\033[2K");
        Serial.printf("Prawy Joy:\tX: %d\tY: %d\n", localPadData.R_Joystick_x_message, localPadData.R_Joystick_y_message);

        uint8_t line = padSectionStart + 5;
        if (!(localPadData.L_Joystick_buttons_message & (1UL << BUTTON_A))) {
            Serial.print("\033["); Serial.print(line++); Serial.print(";1H\033[2K");
            Serial.println("Button L_A pressed");
        }
        if (!(localPadData.L_Joystick_buttons_message & (1UL << BUTTON_B))) {
            Serial.print("\033["); Serial.print(line++); Serial.print(";1H\033[2K");
            Serial.println("Button L_B pressed");
        }
        if (!(localPadData.L_Joystick_buttons_message & (1UL << BUTTON_Y))) {
            Serial.print("\033["); Serial.print(line++); Serial.print(";1H\033[2K");
            Serial.println("Button L_Y pressed");
        }
        if (!(localPadData.L_Joystick_buttons_message & (1UL << BUTTON_X))) {
            Serial.print("\033["); Serial.print(line++); Serial.print(";1H\033[2K");
            Serial.println("Button L_X pressed");
        }
        if (!(localPadData.L_Joystick_buttons_message & (1UL << BUTTON_SELECT))) {
            Serial.print("\033["); Serial.print(line++); Serial.print(";1H\033[2K");
            Serial.println("Button L_SELECT pressed");
        }
        if (!(localPadData.L_Joystick_buttons_message & (1UL << BUTTON_START))) {
            Serial.print("\033["); Serial.print(line++); Serial.print(";1H\033[2K");
            Serial.println("Button L_START pressed");
        }
        if (!(localPadData.R_Joystick_buttons_message & (1UL << BUTTON_A))) {
            Serial.print("\033["); Serial.print(line++); Serial.print(";1H\033[2K");
            Serial.println("Button R_A pressed");
        }
        if (!(localPadData.R_Joystick_buttons_message & (1UL << BUTTON_B))) {
            Serial.print("\033["); Serial.print(line++); Serial.print(";1H\033[2K");
            Serial.println("Button R_B pressed");
        }
        if (!(localPadData.R_Joystick_buttons_message & (1UL << BUTTON_Y))) {
            Serial.print("\033["); Serial.print(line++); Serial.print(";1H\033[2K");
            Serial.println("Button R_Y pressed");
        }
        if (!(localPadData.R_Joystick_buttons_message & (1UL << BUTTON_X))) {
            Serial.print("\033["); Serial.print(line++); Serial.print(";1H\033[2K");
            Serial.println("Button R_X pressed");
        }
        if (!(localPadData.R_Joystick_buttons_message & (1UL << BUTTON_SELECT))) {
            Serial.print("\033["); Serial.print(line++); Serial.print(";1H\033[2K");
            Serial.println("Button R_SELECT pressed");
        }
        if (!(localPadData.R_Joystick_buttons_message & (1UL << BUTTON_START))) {
            Serial.print("\033["); Serial.print(line++); Serial.print(";1H\033[2K");
            Serial.println("Button R_START pressed");
        }
        Serial.print("\033["); Serial.print(line++); Serial.print(";1H\033[2K");
        Serial.println("------------------------------------------");

        // Sekcja Platformy
        Serial.print("\033["); Serial.print(platformSectionStart); Serial.print(";1H\033[2K");
        Serial.println(">>> DANE PLATFORMY <<<");
        Serial.print("\033["); Serial.print(platformSectionStart + 1); Serial.print(";1H\033[2K");
        Serial.printf("Czas:\t\t%u ms\n", localPlatformData.seqNum);
        Serial.print("\033["); Serial.print(platformSectionStart + 2); Serial.print(";1H\033[2K");
        Serial.printf("Predkosci:\tFL: %d\tFR: %d\tRL: %d\tRR: %d\n", 
                      localPlatformData.frontLeftSpeed, 
                      localPlatformData.frontRightSpeed, 
                      localPlatformData.rearLeftSpeed, 
                      localPlatformData.rearRightSpeed);
        Serial.print("\033["); Serial.print(platformSectionStart + 3); Serial.print(";1H\033[2K");
        Serial.printf("IMU:\t\tPitch: %.2f\tRoll: %.2f\tYaw: %.2f\n", 
                      localPlatformData.pitch, 
                      localPlatformData.roll, 
                      localPlatformData.yaw);
        Serial.print("\033["); Serial.print(platformSectionStart + 4); Serial.print(";1H\033[2K");
        Serial.printf("Bateria:\t%.2f V\n", localPlatformData.batteryVoltage);
        Serial.print("\033["); Serial.print(platformSectionStart + 6); Serial.print(";1H\033[2K");
        Serial.println("==========================================");

        vTaskDelayUntil(&lastUpdate, updatePeriod);
    }
}

void setup() {
    Serial.begin(115200);
    // Ustawienie trybu Wi-Fi – Station bez łączenia do sieci
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Inicjalizacja ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ Błąd inicjalizacji ESP-NOW!");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);

    // Dodajemy peer dla Pada (macModulXiao)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macModulXiao, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Nie można dodać Pada!");
    }

    // Dodajemy peer dla Platformy (macPlatformMecanum)
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, macPlatformMecanum, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Nie można dodać Platformy!");
    }

    // Utworzenie kolejki ESP-NOW
    espNowQueue = xQueueCreate(5, sizeof(struct_message)); 
    if (espNowQueue == NULL) {
        Serial.println("❌ Błąd tworzenia kolejki!");
        return;
    }

    // Utworzenie mutexu dla danych
    dataMutex = xSemaphoreCreateMutex();
    if (dataMutex == NULL) {
        Serial.println("❌ Błąd tworzenia mutexu dla danych!");
        return;
    }

    // Tworzenie tasków FreeRTOS (przypinamy do rdzeni, jak potrzeba)
    xTaskCreatePinnedToCore(serialReceiveTask, "SerialTask", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL, 0);
    xTaskCreatePinnedToCore(espNowSendTask, "EspNowTask", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL, 1);
    xTaskCreatePinnedToCore(displayTask, "DisplayTask", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL, 0);

    Serial.println("\n✅ System ESP-NOW + FreeRTOS gotowy!");
}

void loop() {
    // Pusta pętla – wszystkie operacje odbywają się w taskach FreeRTOS
}
