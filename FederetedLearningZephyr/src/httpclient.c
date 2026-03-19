#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <stdlib.h>
#include <string.h>

#include "httpclient.h"
#include "cJSON.h"
#include "JSONConverter.h"
#include "federatedlearning.h"

LOG_MODULE_REGISTER(HTTP_CLIENT, LOG_LEVEL_INF);

/* * ATENÇÃO: No Zephyr, precisamos separar o IP e a porta da rota HTTP.
 * Configure estes valores no seu httpclient.h ou aqui mesmo:
 */
#ifndef SERVER_IP
#define SERVER_IP "192.168.18.192"
#define SERVER_PORT 8888
#endif

// Timeout e Retentativas
#define TIMEOUT_MS 5000
#define MAX_RETRIES 3

// Variáveis globais para armazenar a resposta HTTP
static char *response_buffer = NULL;
static size_t response_buffer_length = 0;

// Funções utilitárias (Mantidas intactas!)
int is_utf8(const char *str) {
    while (*str) {
        if ((*str & 0x80) == 0) { str++; } 
        else if ((*str & 0xE0) == 0xC0) { if ((str[1] & 0xC0) != 0x80) return 0; str += 2; } 
        else if ((*str & 0xF0) == 0xE0) { if ((str[1] & 0xC0) != 0x80 || (str[2] & 0xC0) != 0x80) return 0; str += 3; } 
        else if ((*str & 0xF8) == 0xF0) { if ((str[1] & 0xC0) != 0x80 || (str[2] & 0xC0) != 0x80 || (str[3] & 0xC0) != 0x80) return 0; str += 4; } 
        else { return 0; }
    }
    return 1;
}

int is_complete_json(const char *json, size_t len) {
    int count_open_braces = 0;
    int count_close_braces = 0;
    for (size_t i = 0; i < len; ++i) {
        if (json[i] == '{') count_open_braces++;
        else if (json[i] == '}') count_close_braces++;
    }
    return (count_open_braces == count_close_braces && count_open_braces > 0);
}

// Callback interno do Zephyr para montar o JSON em pedaços
static void http_response_cb(struct http_response *rsp,
                             enum http_final_call final_data,
                             void *user_data)
{
    if (rsp->data_len > 0) {
        char *new_buf = realloc(response_buffer, response_buffer_length + rsp->data_len + 1);
        if (new_buf == NULL) {
            LOG_ERR("Falha ao alocar memória para o buffer de resposta");
            return;
        }
        response_buffer = new_buf;
        memcpy(response_buffer + response_buffer_length, rsp->recv_buf, rsp->data_len);
        response_buffer_length += rsp->data_len;
        response_buffer[response_buffer_length] = '\0'; // Garante o final da string
    }
}

// O motor principal de requisições HTTP do nosso Zephyr
static int perform_http_request(enum http_method method, const char *path, const char *payload)
{
    int sock;
    struct sockaddr_in server_addr;
    struct http_request req;
    int ret;
    int retries = 0;
    
    // Buffer temporário que o Zephyr usa para ler os pacotes TCP da rede
    uint8_t internal_rx_buf[1024];

    // Limpa o buffer de resposta global antes de cada requisição
    if (response_buffer != NULL) {
        free(response_buffer);
        response_buffer = NULL;
    }
    response_buffer_length = 0;

    // Configura o endereço do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    zsock_inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    while (retries < MAX_RETRIES) {
        int64_t start_time = k_uptime_get(); // Relógio interno do Zephyr

        sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0) {
            LOG_ERR("Falha ao criar socket TCP");
            retries++;
            k_msleep(1000);
            continue;
        }

        ret = zsock_connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (ret < 0) {
            LOG_ERR("Falha ao conectar no servidor");
            zsock_close(sock);
            retries++;
            k_msleep(1000);
            continue;
        }

        memset(&req, 0, sizeof(req));
        req.method = method;
        req.url = path;
        req.host = SERVER_IP;
        req.protocol = "HTTP/1.1";
        req.response = http_response_cb; // Associa nosso callback
        req.recv_buf = internal_rx_buf;
        req.recv_buf_len = sizeof(internal_rx_buf);

        if (payload != NULL) {
            req.payload = payload;
            req.payload_len = strlen(payload);
            const char *headers[] = {"Content-Type: application/json\r\n", NULL};
            req.header_fields = headers;
        }

        // A mágica acontece aqui!
        ret = http_client_req(sock, &req, TIMEOUT_MS, NULL);

        zsock_close(sock); // Sempre feche o socket após usar

        if (ret >= 0) {
            int64_t elapsed_time = k_uptime_get() - start_time;
            LOG_INF("Requisição feita com sucesso. Latência: %lld ms", elapsed_time);
            return 0; // Sucesso
        } else {
            LOG_ERR("Erro ao enviar a requisição: %d", ret);
        }

        int64_t current_time = k_uptime_get();
        if ((current_time - start_time) > TIMEOUT_MS) {
            LOG_WRN("Timeout alcançado, tentando novamente... (tentativa: %d)", retries + 1);
        }
        retries++;
    }

    LOG_ERR("Falha após %d tentativas", MAX_RETRIES);
    return -1; // Falha
}


////////////////////////////////////////////////// GETs //////////////////////////////////////////////////

int getglobalmodelstatus() {
    // ATENÇÃO: Substitua "/api/status" pela rota real que você usava no GET_GLOBAL_MODEL_STATUS
    perform_http_request(HTTP_GET, GET_GLOBAL_MODEL_STATUS, NULL);
    
    int status = 0;
    if (response_buffer != NULL) {
        cJSON *req = cJSON_Parse(response_buffer);
        if (req != NULL) {
            cJSON *status_item = cJSON_GetObjectItem(req, "status");
            if (status_item != NULL && cJSON_IsNumber(status_item)) {
                status = status_item->valueint;
            } else {
                LOG_ERR("Erro ao pegar 'status' ou não é um número.");
            }
            cJSON_Delete(req);
        } else {
            LOG_ERR("Falha no parse do JSON.");
        }
        free(response_buffer);
        response_buffer = NULL;
    }
    return status;
}

void getregisternode() {
    // ATENÇÃO: Substitua "/api/register" pela rota real
    perform_http_request(HTTP_GET, GET_REGISTER_NODE, NULL);
    printf("%c AAA", response_buffer);
    if (response_buffer != NULL) {
        cJSON *req = cJSON_Parse(response_buffer);
        if (req != NULL) {
            char *json_string = cJSON_Print(req);
            if (json_string != NULL) {
                printf("Node Registrado: %s\n", json_string);
                cJSON_free(json_string); // Usando cJSON_free para manter a integridade do heap
            }
            cJSON_Delete(req);
        }
        free(response_buffer);
        response_buffer = NULL;
    }
}

FederatedLearning *getglobalmodel() {
    // ATENÇÃO: Substitua "/api/model" pela rota real
    perform_http_request(HTTP_GET, GET_GLOBAL_MODEL, NULL);

    FederatedLearning *FederatedLearningInstance = NULL;
    if (response_buffer != NULL) {
        LOG_INF("Memória livre pré-Parse: %d", k_mem_slab_num_free_get(NULL)); // Log Zephyr
        cJSON *req = cJSON_Parse(response_buffer);
        
        if (req != NULL) {
            FederatedLearningInstance = JSONToFederatedLearning(req);
            cJSON_Delete(req);
        } else {
            LOG_ERR("Falha no parse do modelo global");
        }
        free(response_buffer);
        response_buffer = NULL;
    }
    return FederatedLearningInstance;
}


////////////////////////////////////////////////// POST //////////////////////////////////////////////////

// A sua antiga `http_post_task` agora é super limpa!
void http_post_task(void) {
    const char *post_data = "{\"key\":\"TESTEPOST\",\"value\":1}";
    
    LOG_INF("Iniciando POST de teste...");
    int ret = perform_http_request(HTTP_POST, "/api/testpost", post_data);
    
    if (ret == 0) {
        LOG_INF("POST de teste enviado com sucesso e resposta recebida!");
        if (response_buffer != NULL) {
            LOG_INF("Resposta do servidor: %s", response_buffer);
        }
    } else {
        LOG_ERR("Falha catastrófica no POST.");
    }
    
    // Thread encerra naturalmente no Zephyr ao chegar no fim da função
}