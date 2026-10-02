#ifndef _espconfiguration
#define _espconfiguration

/* * Todas as bibliotecas esp_* e driver/* foram removidas.
 * O Zephyr cuidará das dependências dentro dos arquivos .c
 */

// Pinos
#define LED_PIN_ERROR 12
#define LED_PIN_WORKING 14
#define LED_PIN_SYNC 27
#define BUTTON_PIN 26

#include <stdbool.h>

extern struct k_sem wifi_connected_sem;

// Credenciais de Rede
#define WIFI_SSID       "felipe"
#define WIFI_PASSWORD   "senha111"

// Configs http
#define SERVER_IP "10.190.120.184"
#define SERVER_PORT 8888


// Configs para o Websocket
#define WS_SERVER_IP "10.190.120.184"
#define WS_SERVER_PORT 8080
#define WS_SERVER_PATH "/"

// Training
#define DATA_PATH "/storage/dataset.csv"

//buffer de leitura dados
#define READ_BUFFER_SIZE 1024

// Funções de inicialização restantes
void WIFIConfiguration(void);
void GPIOConfiguration(void);
bool wifi_is_connected(void);
void wifi_wait_connected(void);

#endif