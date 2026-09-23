#include "../lib/websockethandlers.h"
#include "../lib/federatedlearning.h"
#include "../lib/JSONConverter.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void handle_clint_model_message(const char * message, int length,char *ip_addr){

    (void)length; // message is already NUL-terminated by the WebSocket server

    printf("Recebeu ws de %s\n", ip_addr);

    cJSON *json_model = cJSON_Parse(message);
    if (json_model == NULL) {
        printf("Model from %s rejected: invalid JSON\n", ip_addr);
        return;
    }

    // round of the global model used by the node in the training
    cJSON *round_item = cJSON_GetObjectItem(json_model, "round");
    int round = cJSON_IsNumber(round_item) ? round_item->valueint : -1;

    FederatedLearning *clientmodel = JSONToFederatedLearning(json_model);
    cJSON_Delete(json_model);

    if (clientmodel == NULL) {
        printf("Model from %s rejected: invalid model\n", ip_addr);
        return;
    }

    FederatedLearningLock();

    FederatedLearning *FederatedLearningInstance= getFederatedLearningInstance();
    ClientNode *currentclientnode = FederatedLearningInstance->nodecontrol->firstclientnode;

    while(currentclientnode!=NULL){
        if(strcmp(currentclientnode->ip_id,ip_addr)==0){
            break;
        }
        currentclientnode = currentclientnode->nextclientnode;
    }

    if (currentclientnode == NULL) {
        printf("Model from %s rejected: node not registered\n", ip_addr);
    } else {
        AggregationModel(clientmodel, currentclientnode, round);
    }

    FederatedLearningUnlock();

    freeFederatedLearningModel(clientmodel);
}
