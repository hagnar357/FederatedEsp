#ifndef _httpclient
#define _httpclient

#include <zephyr/net/http/client.h>
#include <zephyr/net/socket.h> 

#include "espconfiguration.h"
#include "federatedlearning.h"
#include "JSONConverter.h"

#define MAX_RETRIES 3
#define TIMEOUT_MS 5000

// 2. Isolamos as Rotas (Paths) para o Cabeçalho HTTP
#define GET_GLOBAL_MODEL        "/api/getglobalmodel"
#define GET_GLOBAL_MODEL_STATUS "/api/checkglobalmodel"
#define GET_REGISTER_NODE       "/api/noderegister"
#define POST_GLOBAL_MODEL       "/api/postglobalmodel"

// 3. Assinaturas das funções (removendo os parâmetros do FreeRTOS)
FederatedLearning* getglobalmodel(void);
int getglobalmodelstatus(int *round);
void postglobalmodel(void);
int getregisternode(void);

// Alterado para void (sem o pvParameters do FreeRTOS)
void http_post_task(void);

#endif