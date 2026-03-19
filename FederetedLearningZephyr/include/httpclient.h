#ifndef _httpclient
#define _httpclient

#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h> 

#include "federatedlearning.h"
#include "JSONConverter.h"

#define MAX_RETRIES 3   
#define TIMEOUT_MS 1000 

// 1. Isolamos o IP e a Porta para criar a conexão TCP (Socket)
#define SERVER_IP "192.168.18.192"
#define SERVER_PORT 8888

// 2. Isolamos as Rotas (Paths) para o Cabeçalho HTTP
#define GET_GLOBAL_MODEL        "/api/getglobalmodel"
#define GET_GLOBAL_MODEL_STATUS "/api/checkglobalmodel"
#define GET_REGISTER_NODE       "/api/noderegister"
#define POST_GLOBAL_MODEL       "/api/postglobalmodel"

// 3. Assinaturas das funções (removendo os parâmetros do FreeRTOS)
FederatedLearning* getglobalmodel(void);
int getglobalmodelstatus(void);
void postglobalmodel(void);
void getregisternode(void);

// Alterado para void (sem o pvParameters do FreeRTOS)
void http_post_task(void);

#endif