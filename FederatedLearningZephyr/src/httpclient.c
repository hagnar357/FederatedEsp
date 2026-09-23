#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "httpclient.h"
#include <cJSON.h>
#include "JSONConverter.h"
#include "federatedlearning.h"

LOG_MODULE_REGISTER(HTTP_CLIENT, LOG_LEVEL_INF);

// Variáveis globais para armazenar a resposta HTTP
static char *response_buffer = NULL;
static size_t response_buffer_length = 0;
static uint16_t response_status_code = 0;

static void clear_response_buffer(void)
{
    free(response_buffer);
    response_buffer = NULL;
    response_buffer_length = 0;
    response_status_code = 0;
}

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

static int http_response_cb(struct http_response *rsp,
                            enum http_final_call final_data,
                            void *user_data)
{
    if (rsp->body_frag_len > 0 && rsp->body_frag_start != NULL) {

        char *new_buf = realloc(response_buffer, response_buffer_length + rsp->body_frag_len + 1);

        if (new_buf == NULL) {
            LOG_ERR("Falha ao alocar memória para o buffer de resposta");
            return -ENOMEM; // aborta a requisição
        }

        response_buffer = new_buf;

        memcpy(response_buffer + response_buffer_length, rsp->body_frag_start, rsp->body_frag_len);
        response_buffer_length += rsp->body_frag_len;
        response_buffer[response_buffer_length] = '\0';
    }

    if (final_data == HTTP_DATA_FINAL) {
        response_status_code = rsp->http_status_code;
    }

    return 0;
}

// O motor principal de requisições HTTP do nosso Zephyr
// Retorna 0 somente se a resposta completa chegou com HTTP 200 (corpo em response_buffer)
static int perform_http_request(enum http_method method, const char *path, const char *payload)
{
    int sock;
    struct sockaddr_in server_addr;
    struct http_request req;
    int ret;

    // Buffer temporário que o Zephyr usa para ler os pacotes TCP da rede
    uint8_t internal_rx_buf[1024];

    // Configura o endereço do servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    zsock_inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    for (int retries = 0; retries < MAX_RETRIES; retries++) {

        if (retries > 0) {
            k_msleep(1000 * retries); // backoff entre tentativas
        }

        // Cada tentativa começa com o buffer vazio (evita juntar restos de uma tentativa falha)
        clear_response_buffer();

        int64_t start_time = k_uptime_get(); // Relógio interno do Zephyr

        sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0) {
            LOG_ERR("Falha ao criar socket TCP");
            continue;
        }

        ret = zsock_connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (ret < 0) {
            LOG_ERR("Falha ao conectar no servidor");
            zsock_close(sock);
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

        const char *headers[] = {"Content-Type: application/json\r\n", NULL};
        if (payload != NULL) {
            req.payload = payload;
            req.payload_len = strlen(payload);
            req.header_fields = headers;
        }

        ret = http_client_req(sock, &req, TIMEOUT_MS, NULL);

        zsock_close(sock); // Sempre feche o socket após usar

        if (ret >= 0 && response_status_code == 200) {
            int64_t elapsed_time = k_uptime_get() - start_time;
            LOG_INF("Requisição %s feita com sucesso. Latência: %lld ms", path, elapsed_time);
            return 0; // Sucesso
        }

        LOG_ERR("Erro na requisição %s: ret %d, HTTP %d (tentativa %d)", path, ret, response_status_code, retries + 1);
    }

    clear_response_buffer();
    LOG_ERR("Falha após %d tentativas", MAX_RETRIES);
    return -1; // Falha
}


////////////////////////////////////////////////// GETs //////////////////////////////////////////////////

// Retorna 1 se há modelo global disponível para este nó; round recebe a rodada atual do servidor
int getglobalmodelstatus(int *round) {
    int status = 0;

    if (perform_http_request(HTTP_GET, GET_GLOBAL_MODEL_STATUS, NULL) != 0) {
        return 0;
    }

    cJSON *req = cJSON_Parse(response_buffer);

    if (req != NULL) {
        cJSON *status_item = cJSON_GetObjectItem(req, "status");
        cJSON *round_item = cJSON_GetObjectItem(req, "round");
        if (cJSON_IsNumber(status_item) && cJSON_IsNumber(round_item)) {
            status = status_item->valueint;
            *round = round_item->valueint;
        } else {
            LOG_ERR("Erro ao pegar 'status'/'round'");
        }
        cJSON_Delete(req);
    } else {
        LOG_ERR("Falha no parse: %s", cJSON_GetErrorPtr());
    }

    clear_response_buffer();
    return status;
}

// Retorna 1 se o nó está registrado no servidor (novo ou já existente)
int getregisternode() {
    int registered = 0;

    if (perform_http_request(HTTP_GET, GET_REGISTER_NODE, NULL) != 0) {
        return 0;
    }

    cJSON *req = cJSON_Parse(response_buffer);
    if (req != NULL) {
        cJSON *status_item = cJSON_GetObjectItem(req, "status");
        if (cJSON_IsString(status_item)) {
            printf("Node Registrado: %s\n", status_item->valuestring);
            registered = strcmp(status_item->valuestring, "added") == 0 ||
                         strcmp(status_item->valuestring, "registred") == 0;
        }
        cJSON_Delete(req);
    }
    clear_response_buffer();
    return registered;
}

FederatedLearning *getglobalmodel() {
    if (perform_http_request(HTTP_GET, GET_GLOBAL_MODEL, NULL) != 0) {
        return NULL;
    }

    printf("Getting global model\n");
    FederatedLearning *FederatedLearningInstance = NULL;
    cJSON *req = cJSON_Parse(response_buffer);

    if (req != NULL) {
        FederatedLearningInstance = JSONToFederatedLearning(req);
        cJSON_Delete(req);
    } else {
        LOG_ERR("Falha no parse do modelo global");
    }
    clear_response_buffer();
    return FederatedLearningInstance;
}


////////////////////////////////////////////////// POST //////////////////////////////////////////////////

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
    clear_response_buffer();
}
