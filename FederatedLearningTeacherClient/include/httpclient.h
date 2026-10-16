#ifndef _httpclient
#define _httpclient

#include "federatedlearning.h"

/* HTTP client of the simulated teacher client (same API names as the node's httpclient.h) */

#define GET_GLOBAL_MODEL        "/api/getglobalmodel"
#define GET_GLOBAL_MODEL_FLAT   "/api/getglobalmodelflat"
#define GET_GLOBAL_MODEL_STATUS "/api/checkglobalmodel"
#define GET_REGISTER_NODE       "/api/noderegister"
#define GET_TEACHER_TASK        "/api/getteachertask"
#define GET_TEACHER_MODEL       "/api/getteachermodel"
#define GET_TEACHER_MODEL_FLAT  "/api/getteachermodelflat"

// bind_ip: local address to bind (NULL = any); lets several simulated clients run on one host
void httpclient_configure(const char *server_ip, int port, const char *bind_ip);

// role: "teacher" or NULL; returns 1 if the node is registered (new or existing)
int getregisternode(const char *role);
// returns status (0/1); round, phase and task are filled from the server answer
int getglobalmodelstatus(int *round, char *phase, size_t phase_len, char *task, size_t task_len);
FederatedLearning *getglobalmodel(void);
FederatedLearning *getteachertask(void);
// fetches the trained teacher and installs it in the knowledge distillation module; -1 if unavailable
int getteachermodel(void);
// 1 (default): fetch the teacher and the global model, and upload, in the flat form like the boards do (falls back
// to JSON); 0 (--json-only): JSON only
extern int teacherclient_flat_teacher;
// 1 after the last global model came in the flat form: the uploads use the flat form too
extern int teacherclient_server_flat;

#endif
