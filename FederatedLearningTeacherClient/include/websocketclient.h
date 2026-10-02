#ifndef _websocketclient
#define _websocketclient

void websocketclient_configure(const char *server_ip, int port, const char *bind_ip);

// sends the current local model; type is "teacher" (phase 1) or "local" (phase 2); returns 0 on success
int websocket_send_model(const char *type, int round);

#endif
