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

extern struct k_sem wifi_connected_sem;

// Credenciais de Rede
#define WIFI_SSID       "Isabel"
#define WIFI_PASSWORD   "Mariana2218A"

// Configs http
#define SERVER_IP "192.168.1.195"
#define SERVER_PORT 8888


// Configs para o Websocket
#define WS_SERVER_IP SERVER_IP
#define WS_SERVER_PORT 8080
#define WS_SERVER_PATH "/"
//#define WEBSOCKET_SERVER "ws://"IP":8080/"




// Funções de inicialização restantes
void WIFIConfiguration(void);
void GPIOConfiguration(void);

#endif