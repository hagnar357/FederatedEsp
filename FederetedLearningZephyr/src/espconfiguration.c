#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/dhcpv4.h>

#include <string.h>

#include <stdint.h>

#include "espconfiguration.h" // Mantenha para seus defines (WIFI_SSID, LED_PIN_ERROR, etc.)

LOG_MODULE_REGISTER(SystemConfig, LOG_LEVEL_INF);

K_SEM_DEFINE(wifi_connected_sem, 0, 1);

/* Estruturas para "ouvir" os eventos de rede (Substitui o esp_event_handler) */
static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

/* Handler para eventos de conexão e desconexão do Wi-Fi */
static void handle_wifi_events(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface)
{
    switch (mgmt_event) {
        case NET_EVENT_WIFI_CONNECT_RESULT:
            LOG_INF("WiFi conectado ...");
            net_dhcpv4_start(iface);
            break;
        case NET_EVENT_WIFI_DISCONNECT_RESULT:
            LOG_INF("WiFi perdeu a conexão ...");
            break;
    }
}

static void handle_ipv4_events(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        
        char ip_str[NET_IPV4_ADDR_LEN];
        
        // CORREÇÃO: Adicionamos o ".ipv4." antes do is_used e do address
        if (iface->config.ip.ipv4->unicast[0].ipv4.is_used) {
            
            net_addr_ntop(AF_INET, 
                          &iface->config.ip.ipv4->unicast[0].ipv4.address.in_addr, 
                          ip_str, sizeof(ip_str));
                          
            LOG_INF("Conectado! O Roteador nos deu o IP: %s", ip_str);
        } else {
            LOG_INF("Conectado, mas falha ao ler o texto do IP.");
        }
        
        // Libera a ficha (semáforo) para a função main() continuar
        k_sem_give(&wifi_connected_sem); 
    }
}

void WIFIConfiguration(void)
{
    LOG_INF("Iniciando configuração de rede...");

    // 1. Registra os callbacks de evento de rede
    net_mgmt_init_event_callback(&wifi_cb, handle_wifi_events, NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);

    net_mgmt_init_event_callback(&ipv4_cb, handle_ipv4_events, NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&ipv4_cb);

    // 2. Prepara a estrutura de conexão
    struct net_if *iface = net_if_get_default();
    struct wifi_connect_req_params wifi_params = {0};

    wifi_params.ssid = WIFI_SSID;
    wifi_params.ssid_length = strlen(WIFI_SSID);
    wifi_params.psk = WIFI_PASSWORD;
    wifi_params.psk_length = strlen(WIFI_PASSWORD);
    wifi_params.channel = WIFI_CHANNEL_ANY;
    wifi_params.security = WIFI_SECURITY_TYPE_PSK; // Assumindo WPA2

    // 3. Solicita a conexão
    LOG_INF("Conectando ao WiFi...");
    if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params, sizeof(struct wifi_connect_req_params))) {
        LOG_ERR("Erro ao solicitar conexão Wi-Fi.");
    }
}

void GPIOConfiguration(void)
{
    /* No ESP32, os pinos de 0 a 31 pertencem ao controlador "gpio0" */
    const struct device *gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));

    if (!device_is_ready(gpio0)) {
        LOG_ERR("Controlador GPIO não está pronto!");
        return;
    }

    // Configurar as saídas (LEDs) e já garantir que comecem desligadas (INACTIVE)
    gpio_pin_configure(gpio0, LED_PIN_ERROR, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio0, LED_PIN_WORKING, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio0, LED_PIN_SYNC, GPIO_OUTPUT_INACTIVE);

    // Configurar a entrada (Botão) com resistor interno de pull-up
    gpio_pin_configure(gpio0, BUTTON_PIN, GPIO_INPUT | GPIO_PULL_UP);

    LOG_INF("GPIOs configurados com sucesso.");
}
