#define IP_ADDRESS "10.162.26.184"

// Training settings
#define CLIENTS_NUM 3
#define EPOCHS 10
#define ITERATIONS 32
#define ALPHA 0.01 //learning rate
#define LAMBDA 0.0 //regularization
#define TRAINING_SAMPLES 40 //TODO: Remover, não é mais necessário

// Round control
#define ROUND_TIMEOUT_S 120 //seconds to wait for all nodes before aggregating with the received models

// Teacher (phase 1): architecture and training of the teacher model trained by the teacher client
#define TEACHER_HIDDEN_LAYERS 2
#define TEACHER_HIDDEN_NEURONS 8      // 4-8-8-3 (input/output sizes follow the global model)
#define TEACHER_EPOCHS 500
#define TEACHER_TIMEOUT_S 600         // give up waiting for the teacher and start FL without distillation

// Knowledge distillation parameters sent to the nodes together with the teacher
#define KD_TEMPERATURE 3.0f
#define KD_ALPHA 0.5f

// Teacher architecture: TEACHER_ARCH_CLASSIFIER (flat MLP, logit+KL distillation, previous behavior)
// or TEACHER_ARCH_ENCODER_DECODER (pure autoencoder: encoder/latent/decoder/reconstruction, latent+MSE distillation)
#define TEACHER_ARCH_CLASSIFIER 0
#define TEACHER_ARCH_ENCODER_DECODER 1
#define TEACHER_ARCHITECTURE TEACHER_ARCH_ENCODER_DECODER

// used when TEACHER_ARCHITECTURE == TEACHER_ARCH_ENCODER_DECODER
#define TEACHER_ENCODER_LAYERS 2
#define TEACHER_ENCODER_NEURONS 16
#define TEACHER_LATENT_NEURONS 3   // MUST equal the global model's last-hidden-layer width (layerconfig2, see setFederatedLearningGlobalModel)
#define TEACHER_DECODER_LAYERS 2
#define TEACHER_DECODER_NEURONS 16
