#ifndef _httpclient
#define _httpclient

#include "esp_http_client.h"
#include "federatedlearning.h"
#include "JSONConverter.h"
//#include "configs.c"

#define MAX_RETRIES 3   
#define TIMEOUT_MS 1000 
#define IP "192.168.18.192"

#define GET_GLOBAL_MODEL "http://"IP":8888/api/getglobalmodel"
#define GET_GLOBAL_MODEL_STATUS "http://"IP":8888/api/checkglobalmodel"
#define GET_REGISTER_NODE "http://"IP":8888/api/noderegister"
#define POST_GLOBAL_MODEL "http://"IP":8888/api/postglobalmodel"

FederatedLearning* getglobalmodel();
int getglobalmodelstatus();
void postglobalmodel();
void getregisternode();
void http_post_task(void *pvParameters);

#endif