#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <cJSON.h>
#include "httpclient.h"
#include "JSONConverter.h"
#include "knowledgedistillation.h"

#define MAX_RETRIES 3
#define TIMEOUT_S 5

static char server_ip[INET_ADDRSTRLEN] = "127.0.0.1";
static int server_port = 8888;
static char bind_ip[INET_ADDRSTRLEN] = "";

void httpclient_configure(const char *ip, int port, const char *local_ip) {
    snprintf(server_ip, sizeof(server_ip), "%s", ip);
    server_port = port;
    snprintf(bind_ip, sizeof(bind_ip), "%s", local_ip != NULL ? local_ip : "");
}

// One HTTP/1.1 GET. Returns the status code (or -1), body malloc'ed in *body (caller frees).
static int http_get(const char *path, char **body) {
    *body = NULL;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct timeval timeout = { .tv_sec = TIMEOUT_S, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    if (bind_ip[0] != '\0') {
        struct sockaddr_in local = { .sin_family = AF_INET, .sin_port = 0 };
        inet_pton(AF_INET, bind_ip, &local.sin_addr);
        if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
            printf("<err> HTTP: bind to %s failed: %s\n", bind_ip, strerror(errno));
        }
    }

    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(server_port) };
    inet_pton(AF_INET, server_ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("<err> HTTP: connect to %s:%d failed: %s\n", server_ip, server_port, strerror(errno));
        close(sock);
        return -1;
    }

    char request[512];
    int len = snprintf(request, sizeof(request),
                       "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, server_ip);
    if (write(sock, request, len) != len) {
        close(sock);
        return -1;
    }

    // the server answers with Connection: close, so read until EOF
    size_t capacity = 8192, length = 0;
    char *response = malloc(capacity);
    while (response != NULL) {
        if (length + 1024 > capacity) {
            capacity *= 2;
            char *bigger = realloc(response, capacity);
            if (bigger == NULL) { free(response); response = NULL; break; }
            response = bigger;
        }
        ssize_t n = read(sock, response + length, capacity - length - 1);
        if (n <= 0) break;
        length += (size_t)n;
    }
    close(sock);

    if (response == NULL) {
        return -1;
    }
    response[length] = '\0';

    int status = -1;
    sscanf(response, "HTTP/1.%*d %d", &status);

    char *body_start = strstr(response, "\r\n\r\n");
    if (body_start != NULL) {
        *body = strdup(body_start + 4);
    }
    free(response);
    return status;
}

// GET with retries; transport errors and 5xx are retried, 4xx is not. Returns 0 with the body on HTTP 200.
static int perform_http_request_retries(const char *path, char **body, int max_retries) {
    for (int attempt = 0; attempt < max_retries; attempt++) {
        if (attempt > 0) {
            sleep(attempt);
        }
        int status = http_get(path, body);
        if (status == 200 && *body != NULL) {
            return 0;
        }
        printf("<err> HTTP: %s failed (status %d, attempt %d)\n", path, status, attempt + 1);
        free(*body);
        *body = NULL;
        if (status >= 400 && status < 500) {
            break;
        }
    }
    return -1;
}

static int perform_http_request(const char *path, char **body) {
    return perform_http_request_retries(path, body, MAX_RETRIES);
}

////////////////////////////////////////////////// API //////////////////////////////////////////////////

int getregisternode(const char *role) {
    char path[128];
    if (role != NULL) {
        snprintf(path, sizeof(path), "%s?role=%s", GET_REGISTER_NODE, role);
    } else {
        snprintf(path, sizeof(path), "%s", GET_REGISTER_NODE);
    }

    char *body;
    if (perform_http_request(path, &body) != 0) {
        return 0;
    }

    int registered = 0;
    cJSON *req = cJSON_Parse(body);
    if (req != NULL) {
        cJSON *status_item = cJSON_GetObjectItem(req, "status");
        if (cJSON_IsString(status_item)) {
            printf("Node registered: %s\n", status_item->valuestring);
            registered = strcmp(status_item->valuestring, "added") == 0 ||
                         strcmp(status_item->valuestring, "registred") == 0;
        }
        cJSON_Delete(req);
    }
    free(body);
    return registered;
}

int getglobalmodelstatus(int *round, char *phase, size_t phase_len, char *task, size_t task_len) {
    char *body;
    if (perform_http_request(GET_GLOBAL_MODEL_STATUS, &body) != 0) {
        return 0;
    }

    int status = 0;
    cJSON *req = cJSON_Parse(body);
    if (req != NULL) {
        cJSON *status_item = cJSON_GetObjectItem(req, "status");
        cJSON *round_item = cJSON_GetObjectItem(req, "round");
        cJSON *phase_item = cJSON_GetObjectItem(req, "phase");
        cJSON *task_item = cJSON_GetObjectItem(req, "task");
        if (cJSON_IsNumber(status_item) && cJSON_IsNumber(round_item)) {
            status = status_item->valueint;
            *round = round_item->valueint;
        }
        snprintf(phase, phase_len, "%s", cJSON_IsString(phase_item) ? phase_item->valuestring : "");
        snprintf(task, task_len, "%s", cJSON_IsString(task_item) ? task_item->valuestring : "train");
        cJSON_Delete(req);
    }
    free(body);
    return status;
}

static FederatedLearning *get_model(const char *path, int max_retries) {
    char *body;
    if (perform_http_request_retries(path, &body, max_retries) != 0) {
        return NULL;
    }

    FederatedLearning *model = NULL;
    cJSON *req = cJSON_Parse(body);
    if (req != NULL) {
        model = JSONToFederatedLearning(req);
        cJSON_Delete(req);
    } else {
        printf("<err> HTTP: invalid JSON from %s\n", path);
    }
    free(body);
    return model;
}

FederatedLearning *getglobalmodel(void) {
    return get_model(GET_GLOBAL_MODEL, MAX_RETRIES);
}

FederatedLearning *getteachertask(void) {
    return get_model(GET_TEACHER_TASK, 1);
}

// Same behaviour as the node: one attempt, the cached teacher is kept on failure
int getteachermodel(void) {
    char *body;
    if (perform_http_request_retries(GET_TEACHER_MODEL, &body, 1) != 0) {
        printf("<wrn> Teacher unavailable: %s\n", KnowledgeDistillationGetTeacher() != NULL ?
               "keeping the cached teacher" : "training without distillation");
        return -1;
    }

    int ret = -1;
    cJSON *req = cJSON_Parse(body);
    free(body);
    if (req == NULL) {
        printf("<err> Teacher JSON parse failed\n");
        return -1;
    }

    float temperature = 0;
    float alpha = -1;
    cJSON *distillation = cJSON_GetObjectItem(req, "distillation");
    if (cJSON_IsObject(distillation)) {
        cJSON *temperature_item = cJSON_GetObjectItem(distillation, "temperature");
        cJSON *alpha_item = cJSON_GetObjectItem(distillation, "alpha");
        if (cJSON_IsNumber(temperature_item)) temperature = (float)temperature_item->valuedouble;
        if (cJSON_IsNumber(alpha_item)) alpha = (float)alpha_item->valuedouble;
    }

    FederatedLearning *teacher = JSONToFederatedLearning(req);
    cJSON_Delete(req);
    if (teacher != NULL) {
        ret = KnowledgeDistillationSetTeacher(teacher, temperature, alpha);
    } else {
        printf("<err> Invalid teacher model\n");
    }
    return ret;
}
