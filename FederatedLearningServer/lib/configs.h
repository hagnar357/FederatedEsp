#define IP_ADDRESS "10.190.120.184"

// Training settings
#define CLIENTS_NUM 3
#define EPOCHS 10
#define ITERATIONS 32
#define ALPHA 0.001 //learning rate
#define LAMBDA 0.01 //regularization
#define TRAINING_SAMPLES 40 //TODO: Remover, não é mais necessário

// Round control
#define ROUND_TIMEOUT_S 120 //seconds to wait for all nodes before aggregating with the received models

// Teacher (phase 1): architecture and training of the teacher model trained by the teacher client
#define TEACHER_HIDDEN_LAYERS 2
#define TEACHER_HIDDEN_NEURONS 8      // 4-8-8-3 (input/output sizes follow the global model)
#define TEACHER_EPOCHS 30
#define TEACHER_TIMEOUT_S 600         // give up waiting for the teacher and start FL without distillation

// Knowledge distillation parameters sent to the nodes together with the teacher
#define KD_TEMPERATURE 3.0f
#define KD_ALPHA 0.5f
