#include "../lib/cJSON.h"
#include "../lib/federatedlearning.h"
#include "../lib/JSONConverter.h"
#include "../lib/httphandlers.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

void handle_root_request(int client_socket){
    const char *response = "HTTP/1.1 404 Not Found\nContent-Type: text/plain\n\nFile Not Found";
    printf("File not found on socket: %d", client_socket);
    write(client_socket, response, strlen(response));
}

void handle_not_found_request(int client_socket) {
    const char *not_found_response = "HTTP/1.1 404 Not Found\nContent-Type: text/plain\n\n404 Not Found";
    printf("404 on socket: %d", client_socket);
    write(client_socket, not_found_response, strlen(not_found_response));
}

void handle_testget_request(int client_socket){
    cJSON *json_response = cJSON_CreateObject();
    cJSON_AddStringToObject(json_response, "message", "This is a JSON response for /api/testget.");
    
    char *response_str = cJSON_Print(json_response);
    cJSON_Delete(json_response);

    const char *header = "HTTP/1.1 200 OK\nContent-Type: application/json\n\n";
    write(client_socket, header, strlen(header));
    write(client_socket, response_str, strlen(response_str));
    
    free(response_str);
}

void handle_testpost_request(int client_socket, const char *request_body){
    cJSON *json = cJSON_Parse(request_body);
    if (json != NULL) {
        cJSON *key_value = cJSON_GetObjectItem(json, "key");
        if (cJSON_IsString(key_value) && key_value->valuestring != NULL) {
            printf("Value of 'key': %s\n", key_value->valuestring);
        }

        cJSON_Delete(json);
    }

    const char *response = "HTTP/1.1 200 OK\nContent-Type: text/plain\n\nPOST request to /api/testpost";
    write(client_socket, response, strlen(response));
}

// Sends a JSON body with a complete HTTP header. The socket is closed by handle_request.
static void send_json_response(int client_socket, const char *response_str) {
    char header[256];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n",
             strlen(response_str));

    write(client_socket, header, strlen(header));
    write(client_socket, response_str, strlen(response_str));
}

void handle_get_globalmodel(int client_socket) {
    FederatedLearningLock();
    FederatedLearning *FederatedLearningInstance = getFederatedLearningInstance();
    cJSON *json_response = FederatedLearningToJSON(FederatedLearningInstance);
    FederatedLearningUnlock();

    char *response_str = cJSON_Print(json_response);
    cJSON_Delete(json_response);

    if (response_str == NULL) {
        handle_not_found_request(client_socket);
        return;
    }

    send_json_response(client_socket, response_str);
    free(response_str);
}

void handle_post_globalmodel(int client_socket, const char *request_body) {
    
    printf("post chegou ao menos\n");
    
    // Parseie o corpo JSON usando cJSON

    if(request_body!=NULL){
        printf("TEST: %s\n",request_body);
    }

     cJSON *jsonModel = cJSON_Parse(request_body); 
    //  char* jsonString = cJSON_Print(jsonModel);
    //  printf("%s\n", jsonString);

    if (jsonModel != NULL) {

        //FederatedLearning *FederatedLearningInstance = JSONToFederatedLearning(jsonModel);
         
        // Libere a memória alocada
        
    }else{
    printf("CONTEUDO NULO\n");
    }

    cJSON_Delete(jsonModel);
    // Responder à solicitação
    const char *response = "HTTP/1.1 200 OK\nContent-Type: text/plain\n\nPOST request to /api/postglobalmodel";
    write(client_socket, response, strlen(response));
}


static ClientNode *find_client_node(FederatedLearning *instance, const char *ip_addr) {
    ClientNode *currentclientnode = instance->nodecontrol->firstclientnode;
    while (currentclientnode != NULL) {
        if (strcmp(currentclientnode->ip_id, ip_addr) == 0) {
            return currentclientnode;
        }
        currentclientnode = currentclientnode->nextclientnode;
    }
    return NULL;
}

void handle_get_checkmodelstatus(int client_socket,char *ip_addr){

    int status = 0;

    FederatedLearningLock();
    FederatedLearning *FederatedLearningInstance = getFederatedLearningInstance();
    NodeControl *nodecontrol = FederatedLearningInstance->nodecontrol;
    int round = nodecontrol->currentinteraction;

    ClientNode *currentclientnode = find_client_node(FederatedLearningInstance, ip_addr);

    //the node receives 1 only if all nodes are registered and it did not send the model of this round yet
    if(nodecontrol->clientnodesregistered == nodecontrol->clientnodes &&
       currentclientnode != NULL &&
       currentclientnode->interaction == nodecontrol->currentinteraction &&
       FederatedLearningInstance->globalmodelstatus){
        status = 1;
    }
    FederatedLearningUnlock();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "status", status);
    cJSON_AddNumberToObject(root, "round", round);
    char *response_str = cJSON_Print(root);
    cJSON_Delete(root);

    if (response_str == NULL) {
        handle_not_found_request(client_socket);
        return;
    }

    send_json_response(client_socket, response_str);
    free(response_str);
}



void handle_get_noderegister(int client_socket, char *ip_addr) {
    printf("Client IP: %s\n", ip_addr);

    const char *response_str;

    FederatedLearningLock();
    FederatedLearning *fedLearninginstance = getFederatedLearningInstance();
    NodeControl *nodecontrol = fedLearninginstance->nodecontrol;

    if (find_client_node(fedLearninginstance, ip_addr) != NULL) {
        //already registered nodes can register again (e.g. after a reboot)
        response_str = "{\"status\":\"registred\"}";
        printf("Node already Added\n");
    } else if (nodecontrol->clientnodesregistered >= nodecontrol->clientnodes) {
        response_str = "{\"status\":\"limit reached\"}";
        printf("Node limit reached\n");
    } else {
        ClientNode *clientnode = (ClientNode *)calloc(1, sizeof(ClientNode));
        if (clientnode == NULL) {
            response_str = "{\"status\":\"error\"}";
        } else {
            clientnode->interaction = nodecontrol->currentinteraction;
            snprintf(clientnode->ip_id, sizeof(clientnode->ip_id), "%s", ip_addr);
            clientnode->nextclientnode = NULL;
            clientnode->previousclientnode = nodecontrol->lastclientnode;

            if (nodecontrol->firstclientnode == NULL) {
                nodecontrol->firstclientnode = clientnode;
            } else {
                nodecontrol->lastclientnode->nextclientnode = clientnode;
            }
            nodecontrol->lastclientnode = clientnode;
            nodecontrol->clientnodesregistered++;

            response_str = "{\"status\":\"added\"}";
            printf("Client node Added\n");

            if (nodecontrol->clientnodesregistered == nodecontrol->clientnodes) {
                //all nodes registered: the first round starts now
                fedLearninginstance->globalmodelstatus = 1;
                nodecontrol->roundstarttime = time(NULL);
            }
        }
    }

    printf("client nodes registered %d client nodes %d\n",
           nodecontrol->clientnodesregistered,
           nodecontrol->clientnodes);
    printf("Global Model Status %d\n", fedLearninginstance->globalmodelstatus);
    FederatedLearningUnlock();

    send_json_response(client_socket, response_str);
}
