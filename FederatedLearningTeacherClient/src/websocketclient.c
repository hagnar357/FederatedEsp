#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <libwebsockets.h>

#include <cJSON.h>
#include "websocketclient.h"
#include "JSONConverter.h"
#include "federatedlearning.h"

static char server_ip[INET_ADDRSTRLEN] = "127.0.0.1";
static int server_port = 8080;
static char bind_ip[INET_ADDRSTRLEN] = "";

void websocketclient_configure(const char *ip, int port, const char *local_ip) {
    snprintf(server_ip, sizeof(server_ip), "%s", ip);
    server_port = port;
    snprintf(bind_ip, sizeof(bind_ip), "%s", local_ip != NULL ? local_ip : "");
}

// state of the single message being sent
struct send_state {
    const char *payload;
    size_t length;
    int sent;
    int done;
    int error;
};

static int callback_client(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    (void)user;
    (void)in;
    (void)len;
    struct send_state *state = lws_context_user(lws_get_context(wsi));

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lws_callback_on_writable(wsi);
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            if (state->sent) {
                break;
            }
            unsigned char *buffer = malloc(LWS_PRE + state->length);
            if (buffer == NULL) {
                state->error = 1;
                state->done = 1;
                return -1;
            }
            memcpy(buffer + LWS_PRE, state->payload, state->length);
            int written = lws_write(wsi, buffer + LWS_PRE, state->length, LWS_WRITE_TEXT);
            free(buffer);
            if (written < (int)state->length) {
                state->error = 1;
                state->done = 1;
                return -1;
            }
            state->sent = 1;
            // give the server time to read the frame before closing
            lws_set_timer_usecs(wsi, 200 * LWS_US_PER_MS);
            break;
        }

        case LWS_CALLBACK_TIMER:
            state->done = 1;
            return -1; // closes the connection

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            printf("<err> WebSocket: connection error: %s\n", in != NULL ? (const char *)in : "?");
            state->error = 1;
            state->done = 1;
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
        case LWS_CALLBACK_CLOSED:
            state->done = 1;
            break;

        default:
            break;
    }
    return 0;
}

static const struct lws_protocols protocols[] = {
    { "echo-protocol", callback_client, 0, 0, 0, NULL, 0 },
    { NULL, NULL, 0, 0, 0, NULL, 0 }
};

int websocket_send_model(const char *type, int round) {
    cJSON *root = federatedLearningToJSON(getFederatedLearningInstance());
    if (root == NULL) {
        printf("<err> WebSocket: failed to serialize the model\n");
        return -1;
    }
    cJSON_AddStringToObject(root, "type", type);
    cJSON_AddNumberToObject(root, "round", round);

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json_string == NULL) {
        return -1;
    }

    struct send_state state = { .payload = json_string, .length = strlen(json_string) };

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.user = &state;

    struct lws_context *context = lws_create_context(&info);
    if (context == NULL) {
        printf("<err> WebSocket: failed to create the context\n");
        free(json_string);
        return -1;
    }

    struct lws_client_connect_info connect_info;
    memset(&connect_info, 0, sizeof(connect_info));
    connect_info.context = context;
    connect_info.address = server_ip;
    connect_info.port = server_port;
    connect_info.path = "/";
    connect_info.host = server_ip;
    connect_info.origin = server_ip;
    connect_info.protocol = "echo-protocol";
    if (bind_ip[0] != '\0') {
        connect_info.iface = bind_ip; // local address to bind
    }

    printf("<inf> WebSocket: sending %s model (%zu bytes) to %s:%d\n", type, state.length, server_ip, server_port);

    if (lws_client_connect_via_info(&connect_info) == NULL) {
        printf("<err> WebSocket: connect failed\n");
        lws_context_destroy(context);
        free(json_string);
        return -1;
    }

    // service until the message is sent (or an error), at most ~10 s
    for (int i = 0; i < 200 && !state.done; i++) {
        lws_service(context, 50);
    }

    lws_context_destroy(context);
    free(json_string);

    if (state.error || !state.sent) {
        printf("<err> WebSocket: model not sent\n");
        return -1;
    }
    printf("<inf> WebSocket: model sent\n");
    return 0;
}
