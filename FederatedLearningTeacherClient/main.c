/*
 * Simulated teacher client.
 *
 * Phase 1 (task "teacher"): downloads the untrained teacher task from the server, trains it on
 * the local dataset with plain cross-entropy and sends it back ("type": "teacher").
 * Phase 2 (task "train"): behaves like a Zephyr node — downloads the global model each round,
 * distills the teacher (if enabled) and sends the local model ("type": "local").
 *
 * The training code is the same one that runs on the boards (FederatedLearningZephyr/src).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <zephyr/kernel.h> // host shim: CONFIG_FL_KD_* defaults
#include "federatedlearning.h"
#include "knowledgedistillation.h"
#include "httpclient.h"
#include "websocketclient.h"
#include "espconfiguration.h"

#define POLL_INTERVAL_S 4
#define GLOBAL_MODEL_MAX_ATTEMPTS 5
#define SEND_MODEL_MAX_ATTEMPTS 3

static void usage(const char *program) {
    fprintf(stderr,
            "Usage: %s --server IP --dataset path [--port 8888] [--wsport 8080] [--no-kd] [--role teacher|client] [--bind IP]\n"
            "  --no-kd   train without knowledge distillation in the federated phase\n"
            "  --role    client: do not train the teacher (default: teacher)\n"
            "  --bind    local IP to connect from (several simulated clients on one host)\n",
            program);
}

// sends the already trained model, without retraining, like send_local_model() on the node
static int send_model(const char *type, int round) {
    for (int attempt = 1; attempt <= SEND_MODEL_MAX_ATTEMPTS; attempt++) {
        if (websocket_send_model(type, round) == 0) {
            return 0;
        }
        printf("Model send failed (attempt %d)\n", attempt);
        sleep(attempt);
    }
    return -1;
}

static FederatedLearning *fetch_global_model(void) {
    for (int attempt = 1; attempt <= GLOBAL_MODEL_MAX_ATTEMPTS; attempt++) {
        FederatedLearning *model = getglobalmodel();
        if (model != NULL) {
            return model;
        }
        printf("Global model unavailable (attempt %d)\n", attempt);
        sleep(attempt);
    }
    return NULL;
}

// phase 1: train the teacher task with plain cross-entropy and send it back
static void train_teacher(int round) {
    FederatedLearning *teachertask = getteachertask();
    if (teachertask == NULL) {
        printf("Teacher task unavailable\n");
        sleep(2);
        return;
    }

    printf("=== Phase TEACHER: training the teacher (%d layers, %d epochs) ===\n",
           teachertask->neuralnetwork->layers, teachertask->neuralnetwork->epoch);
    replaceNeuralNetwork(teachertask);

    // the teacher itself is trained without distillation
    KnowledgeDistillationConfig config = KnowledgeDistillationGetConfig();
    KnowledgeDistillationDisable();
    NeuralNetworkTraining();
    if (config.enabled) {
        KnowledgeDistillationEnable(config.temperature, config.alpha);
    }

    if (getFederatedLearningInstance()->trainingscounter <= 0) {
        printf("Nothing was trained, teacher not sent\n");
        sleep(POLL_INTERVAL_S);
        return;
    }

    if (send_model("teacher", round) == 0) {
        printf("=== Teacher sent to the server ===\n");
    }
}

// phase 2: same loop as deep_learning() on the node
static void train_local_round(int round, int *last_trained_round) {
    FederatedLearning *global = fetch_global_model();
    if (global == NULL) {
        return;
    }
    replaceNeuralNetwork(global);

    if (KnowledgeDistillationIsEnabled()) {
        getteachermodel();
    }

    NeuralNetworkTraining();

    if (getFederatedLearningInstance()->trainingscounter <= 0) {
        printf("Nothing was trained, model not sent\n");
        sleep(POLL_INTERVAL_S);
        return;
    }

    if (send_model("local", round) == 0) {
        *last_trained_round = round;
    }
}

int main(int argc, char **argv) {
    const char *server = NULL;
    const char *dataset = NULL;
    const char *role = "teacher";
    const char *bind_ip = NULL;
    int port = 8888, wsport = 8080, use_kd = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--server") == 0 && i + 1 < argc) server = argv[++i];
        else if (strcmp(argv[i], "--dataset") == 0 && i + 1 < argc) dataset = argv[++i];
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--wsport") == 0 && i + 1 < argc) wsport = atoi(argv[++i]);
        else if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) role = argv[++i];
        else if (strcmp(argv[i], "--bind") == 0 && i + 1 < argc) bind_ip = argv[++i];
        else if (strcmp(argv[i], "--no-kd") == 0) use_kd = 0;
        else { usage(argv[0]); return 1; }
    }
    if (server == NULL || dataset == NULL) {
        usage(argv[0]);
        return 1;
    }

    setbuf(stdout, NULL);
    teacherclient_dataset_path = dataset;
    httpclient_configure(server, port, bind_ip);
    websocketclient_configure(server, wsport, bind_ip);

    if (use_kd) {
        // local defaults; the values sent by the server with the teacher take precedence
        KnowledgeDistillationEnable(CONFIG_FL_KD_TEMPERATURE_X10 / 10.0f, CONFIG_FL_KD_ALPHA_PERCENT / 100.0f);
    }

    int samples = contar_instancias(dataset);
    if (samples <= 0) {
        fprintf(stderr, "Dataset %s is empty or unreadable\n", dataset);
        return 1;
    }
    printf("Teacher client: server %s:%d/%d, dataset %s (%d samples), role %s, KD %s\n",
           server, port, wsport, dataset, samples, role, use_kd ? "on" : "off");

    while (!getregisternode(strcmp(role, "teacher") == 0 ? "teacher" : NULL)) {
        printf("Registration failed, retrying...\n");
        sleep(2);
    }

    int last_trained_round = -1;
    for (;;) {
        int round = -1;
        char phase[16], task[16];
        if (!getglobalmodelstatus(&round, phase, sizeof(phase), task, sizeof(task))) {
            sleep(POLL_INTERVAL_S);
            continue;
        }

        if (strcmp(task, "teacher") == 0) {
            train_teacher(round);
            continue;
        }

        if (round == last_trained_round) {
            sleep(POLL_INTERVAL_S);
            continue;
        }
        train_local_round(round, &last_trained_round);
    }

    return 0;
}
