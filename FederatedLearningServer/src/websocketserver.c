#include "../lib/websocketserver.h"
#include "../lib/websockethandlers.h"
#include "../lib/federatedlearning.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <libwebsockets.h>

// Estrutura para passar argumentos para a thread
struct ThreadArgs {
    int port;
};

// The message queue is used only by the WebSocket thread (filled in the lws callback and consumed after lws_service)
typedef struct client_message {
    char* message;
    int length;
    char ip_id[INET_ADDRSTRLEN];
    struct client_message *previous;
    struct client_message *next;
} client_message;

typedef struct buffer_client_message {
    client_message *first;
    client_message *last;
} buffer_client_message;

void init_buffer_client_message(buffer_client_message **handle_message) {
    if (*handle_message == NULL) {
        *handle_message = (buffer_client_message *)malloc(sizeof(buffer_client_message));
        (*handle_message)->first = NULL;
        (*handle_message)->last = NULL;
    }
}

buffer_client_message* get_buffer_client_message() {
    static buffer_client_message *buffer = NULL;
    if (buffer == NULL) {
        init_buffer_client_message(&buffer);
    }
    return buffer;
}

int is_buffer_client_message_empty() {
    buffer_client_message *buffer = get_buffer_client_message();
    return buffer->last == NULL;
}

void insert_buffer_client_message(const char* message, int length, const char*ip_addr) {

    buffer_client_message *buffer = get_buffer_client_message();
    client_message *new_client_message = (client_message *)malloc(sizeof(client_message));
    if (new_client_message == NULL) {
        printf("Failed to allocate memory for new client message.\n");
        return;
    }

    new_client_message->message = strndup(message, length);
    if (new_client_message->message == NULL) {
        printf("Failed to allocate memory for message copy.\n");
        free(new_client_message);
        return;
    }

    new_client_message->length = length;
    snprintf(new_client_message->ip_id, sizeof(new_client_message->ip_id), "%s", ip_addr);
    new_client_message->next = NULL;

    if (is_buffer_client_message_empty()) {
        new_client_message->previous = NULL;
        buffer->first = new_client_message;
        buffer->last = new_client_message;
    } else {
        buffer->last->next = new_client_message;
        new_client_message->previous = buffer->last;
        buffer->last = new_client_message;
    }
}

void remove_buffer_client_message() {

    buffer_client_message *buffer = get_buffer_client_message();
    if (is_buffer_client_message_empty()) {
        printf("Buffer is empty. Cannot remove.\n");
        return;
    }

    client_message *removed_message = buffer->first;
    handle_clint_model_message(removed_message->message,removed_message->length,removed_message->ip_id);

    if (buffer->first == buffer->last) {
        buffer->first = NULL;
        buffer->last = NULL;
    } else {
        buffer->first = buffer->first->next;
        buffer->first->previous = NULL;
    }

    free(removed_message->message);
    free(removed_message);
}

void handle_message() {
    while (!is_buffer_client_message_empty())
    {
        remove_buffer_client_message();
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_JSON_SIZE 16384 // String JSON max size

// Each connection has its own reassembly buffer, so fragments of different nodes never mix
struct per_session_data {
    char client_ip[INET_ADDRSTRLEN];
    size_t json_len;
    char json[MAX_JSON_SIZE + 1];
};

void get_client_ip(struct lws *wsi, char *client_ip, size_t len) {
    char full_ip[128];
    lws_get_peer_simple(wsi, full_ip, sizeof(full_ip));

    // IPv4-mapped IPv6 addresses look like ::ffff:192.168.1.10
    const char *ipv4_part = strrchr(full_ip, ':');
    snprintf(client_ip, len, "%s", ipv4_part != NULL ? ipv4_part + 1 : full_ip);
}

// Callback for received messages
static int callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {

    struct per_session_data *pss = (struct per_session_data *)user;

    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            pss->json_len = 0;
            get_client_ip(wsi, pss->client_ip, sizeof(pss->client_ip));
            break;

        case LWS_CALLBACK_RECEIVE:

            if (pss->json_len + len > MAX_JSON_SIZE) {
                fprintf(stderr, "Received message too big from %s\n", pss->client_ip);
                pss->json_len = 0;
                return -1;
            }

            memcpy(pss->json + pss->json_len, in, len);
            pss->json_len += len;

            // complete message: last fragment and nothing left of the current frame
            if (lws_is_final_fragment(wsi) && lws_remaining_packet_payload(wsi) == 0) {
                pss->json[pss->json_len] = '\0';
                insert_buffer_client_message(pss->json, pss->json_len, pss->client_ip);
                pss->json_len = 0;
            }

            break;

        case LWS_CALLBACK_CLOSED:
            pss->json_len = 0;
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            break;
        default:
            break;
    }
    return 0;
}

void *start_websocketserver(void *args) {
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)args;
    int port = threadArgs->port;

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = port;
    info.gid = -1;
    info.uid = -1;
    info.options = 0;
    info.max_http_header_data = 16384;


    struct lws_protocols protocols[] = {
        {"echo-protocol", callback_echo, sizeof(struct per_session_data), MAX_JSON_SIZE, 0, NULL, 0},
        {NULL, NULL, 0, 0, 0, NULL, 0}
    };

    info.protocols = protocols;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Erro ao criar o contexto do servidor WebSocket\n");
        return NULL;
    }

    printf("WebSocket Server running in ws://%s:%d\n",IP_ADDRESS, port);

    while (1) {
        lws_service(context, 50);
        handle_message();
        CheckRoundTimeout();
    }

    lws_context_destroy(context);
    return NULL;
}
