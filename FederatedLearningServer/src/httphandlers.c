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
    const char *not_found_response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 13\r\nConnection: close\r\n\r\n404 Not Found";
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

static ClientNode *find_client_node(FederatedLearning *instance, const char *ip_addr);

// Phase TEACHER: the untrained teacher task, only for the teacher client
void handle_get_teachertask(int client_socket, char *ip_addr) {
    cJSON *json_response = NULL;

    FederatedLearningLock();
    FederatedLearning *instance = getFederatedLearningInstance();
    ClientNode *node = find_client_node(instance, ip_addr);
    if (instance->nodecontrol->phase == PHASE_TEACHER && node != NULL && node->isteacher) {
        json_response = TeacherTaskToJSON();
    }
    FederatedLearningUnlock();

    if (json_response == NULL) {
        printf("Teacher task refused for %s\n", ip_addr);
        handle_not_found_request(client_socket);
        return;
    }

    char *response_str = cJSON_Print(json_response);
    cJSON_Delete(json_response);
    if (response_str == NULL) {
        handle_not_found_request(client_socket);
        return;
    }
    send_json_response(client_socket, response_str);
    free(response_str);
}

// Phase FEDERATED: the trained teacher + distillation parameters (404 while there is no teacher)
void handle_get_teachermodel(int client_socket) {
    FederatedLearningLock();
    cJSON *json_response = TeacherModelToJSON();
    FederatedLearningUnlock();

    if (json_response == NULL) {
        handle_not_found_request(client_socket);
        return;
    }

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

    const char *task = "train";
    int phase = nodecontrol->phase;

    if(phase == PHASE_TEACHER){
        //only the teacher client works in phase 1, until it sends the teacher back
        task = "teacher";
        status = currentclientnode != NULL && currentclientnode->isteacher && !nodecontrol->teachertrained;
    }
    //the node receives 1 only if all nodes are registered and it did not send the model of this round yet
    else if(phase == PHASE_FEDERATED &&
       nodecontrol->clientnodesregistered == nodecontrol->clientnodes &&
       currentclientnode != NULL &&
       currentclientnode->interaction == nodecontrol->currentinteraction &&
       FederatedLearningInstance->globalmodelstatus){
        status = 1;
    }
    FederatedLearningUnlock();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "status", status);
    cJSON_AddNumberToObject(root, "round", round);
    cJSON_AddStringToObject(root, "phase", PhaseName(phase));
    cJSON_AddStringToObject(root, "task", task);
    char *response_str = cJSON_Print(root);
    cJSON_Delete(root);

    if (response_str == NULL) {
        handle_not_found_request(client_socket);
        return;
    }

    send_json_response(client_socket, response_str);
    free(response_str);
}



static ClientNode *find_teacher_node(FederatedLearning *instance) {
    ClientNode *currentclientnode = instance->nodecontrol->firstclientnode;
    while (currentclientnode != NULL) {
        if (currentclientnode->isteacher) {
            return currentclientnode;
        }
        currentclientnode = currentclientnode->nextclientnode;
    }
    return NULL;
}

// isteacher: the client registered with ?role=teacher and will train the teacher model in phase TEACHER
void handle_get_noderegister(int client_socket, char *ip_addr, int isteacher) {
    printf("Client IP: %s%s\n", ip_addr, isteacher ? " (teacher)" : "");

    const char *response_str;

    FederatedLearningLock();
    FederatedLearning *fedLearninginstance = getFederatedLearningInstance();
    NodeControl *nodecontrol = fedLearninginstance->nodecontrol;

    ClientNode *existing = find_client_node(fedLearninginstance, ip_addr);
    if (existing != NULL) {
        //already registered nodes can register again (e.g. after a reboot)
        if (isteacher) {
            existing->isteacher = 1;
        }
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
            clientnode->isteacher = isteacher;
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
                //all nodes registered: train the teacher first if a teacher client exists
                if (find_teacher_node(fedLearninginstance) != NULL) {
                    StartTeacherPhase();
                } else {
                    printf("No teacher client registered, starting federated training directly\n");
                    StartFederatedPhase();
                }
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
