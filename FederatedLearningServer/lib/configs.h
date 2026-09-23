#define IP_ADDRESS "192.168.1.195"

// Training settings
#define CLIENTS_NUM 2
#define EPOCHS 10
#define ITERATIONS 32
#define ALPHA 0.001 //learning rate
#define LAMBDA 0.01 //regularization
#define TRAINING_SAMPLES 40 //TODO: Remover, não é mais necessário

// Round control
#define ROUND_TIMEOUT_S 120 //seconds to wait for all nodes before aggregating with the received models
