#define IP_ADDRESS "192.168.3.5"

// Global/student model shape: MUST match the dataset's CSV columns (id,x1..xN,y0..yC-1) and
// separate_dataset.py's NUM_FEATURES/NUM_CLASSES. Currently: HAR reduced to 32 features (PCA from
// the original 561) over 6 activity classes (see FederatedLearningZephyr/data/dataset_full.csv).
#define INPUT_FEATURES 32
#define OUTPUT_CLASSES 6
#define HIDDEN_LAYERS 2 // hidden layers of the global/student model
#define HIDDEN_NEURONS 16 // both hidden layers of the global/student model
#define EVAL_SAMPLES 300  // rows read from data/datasetevaluation.csv per evaluation (it now has 1000)

// Training settings
#define CLIENTS_NUM 3
#define EPOCHS 10
#define ITERATIONS 32
#define ALPHA 0.01 //learning rate
#define LAMBDA 0.0 //regularization
#define TRAINING_SAMPLES 40 //TODO: Remover, não é mais necessário
// Seed of the initial weights (global model and teacher): 0 = new random weights every run (time-based);
// any other value = the same initial weights in every run, for comparing configurations fairly
#define WEIGHT_INIT_SEED 0

// Round control
// 240s: safety margin for the first real-hardware run at the HAR scale (32-16-16-6, ~860 weights,
// plus the teacher's encoder forward pass for KD_MODE_LATENT) -- bigger than the old Iris-scale
// network (4-3-3-3) this value was originally tuned for, and never timed on actual ESP32-S3 boards.
// Once real per-round timings are seen in the server log, this can be tightened back down.
#define ROUND_TIMEOUT_S 240 //seconds to wait for all nodes before aggregating with the received models

// Teacher (phase 1): architecture and training of the teacher model trained by the teacher client
#define TEACHER_HIDDEN_LAYERS 2
#define TEACHER_HIDDEN_NEURONS 16     // input/output sizes follow the global model (TEACHER_ARCH_CLASSIFIER only)
#define TEACHER_ALPHA 0.01       // learning rate of the teacher training (the students use ALPHA)
#define TEACHER_LAMBDA LAMBDA     // L2 regularization of the teacher training (the students use LAMBDA)
#define TEACHER_EPOCHS 250
#define TEACHER_TIMEOUT_S 600         // give up waiting for the teacher and start FL without distillation

// Knowledge distillation parameters sent to the nodes together with the teacher
#define KD_TEMPERATURE 3.0f
#define KD_ALPHA 0.5f
// Latent mode only: student layer matched against the teacher's latent layer, 0-based (0 = input,
// 1 = first hidden, 2 = second hidden). Must be a hidden layer with TEACHER_LATENT_NEURONS neurons.
#define KD_STUDENT_HINT_LAYER 2

// Teacher architecture: TEACHER_ARCH_CLASSIFIER (flat MLP, logit+KL distillation, previous behavior)
// or TEACHER_ARCH_ENCODER_DECODER (pure autoencoder: encoder/latent/decoder/reconstruction, latent+MSE distillation)
#define TEACHER_ARCH_CLASSIFIER 0
#define TEACHER_ARCH_ENCODER_DECODER 1
#define TEACHER_ARCHITECTURE TEACHER_ARCH_ENCODER_DECODER

// used when TEACHER_ARCHITECTURE == TEACHER_ARCH_ENCODER_DECODER
#define TEACHER_ENCODER_LAYERS 2
#define TEACHER_ENCODER_NEURONS 16
#define TEACHER_LATENT_NEURONS HIDDEN_NEURONS // MUST equal the width of the student layer KD_STUDENT_HINT_LAYER
#define TEACHER_DECODER_LAYERS 2
#define TEACHER_DECODER_NEURONS 16
