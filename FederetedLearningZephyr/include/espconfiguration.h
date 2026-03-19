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
#define WIFI_SSID       "Alves"
#define WIFI_PASSWORD   "O4pN71nV"

// Funções de inicialização restantes
void WIFIConfiguration(void);
void GPIOConfiguration(void);

#endif