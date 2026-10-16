#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/websocket.h>

#include "websocketclient.h"
#include <cJSON.h>
#include "JSONConverter.h"
#include "federatedlearning.h"
#include "httpclient.h"

/* No Zephyr, registramos o módulo de log assim, sem precisar passar a TAG nos prints */
LOG_MODULE_REGISTER(WebSocketClient, LOG_LEVEL_INF);

#define WS_SEND_FRAGMENT_SIZE 1024

// Retorna 0 se o modelo local foi enviado
int websocket_send_local_model(int round)
{
    int sock;
    int ws_sock;
    struct sockaddr_in server_addr;
    int ret;

    // 1. O buffer temporário exigido pela API do Zephyr para o Handshake
    static uint8_t ws_tmp_buf[1024];

    // 2. A lista de cabeçalhos opcionais (Obrigatório terminar com NULL)
    const char *extra_headers[] = {
        "Sec-WebSocket-Protocol: echo-protocol\r\n",
        NULL
    };

    LOG_INF("Iniciando conexão com %s:%d ...", WS_SERVER_IP, WS_SERVER_PORT);

    // Configurar o endereço do servidor (Socket padrão)
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(WS_SERVER_PORT);
    zsock_inet_pton(AF_INET, WS_SERVER_IP, &server_addr.sin_addr);

    // Criar e conectar o socket TCP base
    sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        LOG_ERR("Falha ao criar socket TCP: %d", errno);
        return -1;
    }

    ret = zsock_connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        LOG_ERR("Falha ao conectar via TCP: %d", errno);
        zsock_close(sock);
        return -1;
    }

    // 3. Estruturar a requisição do WebSocket conforme a documentação oficial
    struct websocket_request req;
    memset(&req, 0, sizeof(req)); // Zera a memória para evitar lixo de ponteiros
    
    req.host = WS_SERVER_IP;
    req.url = WS_SERVER_PATH;
    req.optional_headers = extra_headers; // Injeta a senha do protocolo
    req.tmp_buf = ws_tmp_buf;             // Passa o buffer de rascunho
    req.tmp_buf_len = sizeof(ws_tmp_buf); // Informa o tamanho do buffer

    // 4. Promover o socket TCP para WebSocket (Handshake)
    ws_sock = websocket_connect(sock, &req, 5000, NULL); // 5 segundos de timeout
    if (ws_sock < 0) {
        LOG_ERR("Falha no handshake WebSocket: %d", ws_sock);
        zsock_close(sock);
        return -1;
    }

    LOG_INF("WEBSOCKET_EVENT_CONNECTED");

    // 5. Gerar o texto do modelo. Servidor com formato plano: escrito direto num buffer (~5 KB para 260 pesos);
    // senão a árvore cJSON (~76 B por peso). A falha costuma ser falta de memória: registrar, antes ela era silenciosa.
    char *json_string = NULL;
    if (server_supports_flat()) {
        json_string = federatedLearningToFlatString(getFederatedLearningInstance(), round, NULL);
    } else {
        cJSON *root = federatedLearningToJSON(getFederatedLearningInstance());
        if (root != NULL) {
            // Rodada do modelo global usado no treino: o servidor rejeita modelos duplicados ou atrasados
            cJSON_AddNumberToObject(root, "round", round);
            json_string = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);   // Corrige o vazamento da árvore JSON
        }
    }
    if (json_string == NULL) {
        LOG_ERR("Falha ao serializar o modelo (sem memória?)");
        websocket_disconnect(ws_sock); // envia CLOSE e libera o contexto WebSocket
        zsock_close(sock);             // o socket TCP precisa ser fechado separadamente
        return -1;
    }

    // 6. Enviar os dados via WebSocket, em fragmentos: websocket_send_msg faz k_malloc de uma cópia
    // de cada fragmento para aplicar a máscara, e o heap do kernel (compartilhado com o Wi-Fi) não
    // garante um bloco contíguo do tamanho do modelo inteiro. O servidor junta os fragmentos.
    ret = -1;
    if (json_string != NULL) {
        size_t total = strlen(json_string);
        LOG_INF("Enviando modelo local (%zu bytes)...", total);

        size_t offset = 0;
        do {
            size_t chunk = MIN(total - offset, WS_SEND_FRAGMENT_SIZE);
            enum websocket_opcode opcode = (offset == 0) ? WEBSOCKET_OPCODE_DATA_TEXT
                                                         : WEBSOCKET_OPCODE_CONTINUE;
            ret = websocket_send_msg(ws_sock, (const uint8_t *)json_string + offset, chunk,
                                     opcode, true, offset + chunk == total, SYS_FOREVER_MS);
            offset += chunk;
        } while (ret >= 0 && offset < total);

        if (ret < 0) {
            LOG_ERR("Falha ao enviar os dados: %d", ret);
        } else {
            LOG_INF("Dados enviados com sucesso!");
        }

        // IMPORTANTE: Use cJSON_free em vez de free() normal para respeitar o Zephyr Heap
        cJSON_free(json_string);
    }

    // 7. Encerrar a conexão: websocket_disconnect NÃO fecha o socket TCP subjacente,
    // sem o zsock_close cada envio vaza um descritor e o handshake passa a falhar com -ENOSPC
    websocket_disconnect(ws_sock);
    zsock_close(sock);

    LOG_INF("Websocket Stopped");
    return ret < 0 ? -1 : 0;
}