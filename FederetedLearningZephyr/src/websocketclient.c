#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/websocket.h>

#include "websocketclient.h"
#include "cJSON.h"
#include "JSONConverter.h"
#include "federatedlearning.h"

/* No Zephyr, registramos o módulo de log assim, sem precisar passar a TAG nos prints */
LOG_MODULE_REGISTER(WebSocketClient, LOG_LEVEL_INF);

#define WS_SERVER_IP "192.168.18.192"
#define WS_SERVER_PORT 8080
#define WS_SERVER_PATH "/ws"

void websocket_send_local_model(void)
{
    int sock;
    int ws_sock;
    struct sockaddr_in server_addr;
    int ret;

    LOG_INF("Iniciando conexão com %s:%d ...", WS_SERVER_IP, WS_SERVER_PORT);

    // 1. Configurar o endereço do servidor (Socket padrão)
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(WS_SERVER_PORT);
    zsock_inet_pton(AF_INET, WS_SERVER_IP, &server_addr.sin_addr);

    // 2. Criar e conectar o socket TCP base
    sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        LOG_ERR("Falha ao criar socket TCP: %d", errno);
        return;
    }

    ret = zsock_connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        LOG_ERR("Falha ao conectar via TCP: %d", errno);
        zsock_close(sock);
        return;
    }

    // 3. Promover o socket TCP para WebSocket (Handshake)
    struct websocket_request req = {
        .host = WS_SERVER_IP,
        .url = WS_SERVER_PATH,
    };

    ws_sock = websocket_connect(sock, &req, 5000, NULL); // 5 segundos de timeout
    if (ws_sock < 0) {
        LOG_ERR("Falha no handshake WebSocket: %d", ws_sock);
        zsock_close(sock);
        return;
    }

    LOG_INF("WEBSOCKET_EVENT_CONNECTED");

    // 4. Gerar o JSON da rede neural (sua lógica original mantida!)
    cJSON *root = federatedLearningToJSON(getFederatedLearningInstance());
    if (root == NULL) {
        LOG_ERR("Falha ao criar JSON do modelo.");
        websocket_disconnect(ws_sock);
        zsock_close(sock);
        return;
    }

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);   // Corrige o vazamento da árvore JSON

    // 5. Enviar os dados via WebSocket
    if (json_string != NULL) {
        LOG_INF("Enviando modelo local (%d bytes)...", strlen(json_string));
        
        // portMAX_DELAY vira K_FOREVER no Zephyr
        ret = websocket_send_msg(ws_sock, json_string, strlen(json_string),
                                 WEBSOCKET_OPCODE_DATA_TEXT, true, true, SYS_FOREVER_MS);
        
        if (ret < 0) {
            LOG_ERR("Falha ao enviar os dados: %d", ret);
        } else {
            LOG_INF("Dados enviados com sucesso!");
        }

        // IMPORTANTE: Use cJSON_free em vez de free() normal para respeitar o Zephyr Heap
        cJSON_free(json_string);
    }

    // 6. Encerrar a conexão
    websocket_disconnect(ws_sock);
    zsock_close(sock);
    
    LOG_INF("Websocket Stopped");
}