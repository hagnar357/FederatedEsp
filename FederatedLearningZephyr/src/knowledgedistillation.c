#include <math.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "knowledgedistillation.h"

LOG_MODULE_REGISTER(KnowledgeDistillation, LOG_LEVEL_INF);

#define KD_MIN_PROBABILITY 1e-7f

// local configuration (Kconfig defaults, changed by the runtime API); mode/latentlayerindex are only
// ever known once a teacher is received, so they are not part of the Kconfig-backed defaults
static KnowledgeDistillationConfig localconfig = {
  .enabled = IS_ENABLED(CONFIG_FL_KNOWLEDGE_DISTILLATION),
  .mode = KD_MODE_LOGIT,
  .temperature = CONFIG_FL_KD_TEMPERATURE_X10 / 10.0f,
  .alpha = CONFIG_FL_KD_ALPHA_PERCENT / 100.0f,
  .latentlayerindex = -1,
};

static NeuralNetwork *teacher = NULL;
// hyperparameters sent with the teacher
static int teachermode = KD_MODE_LOGIT;
static float teachertemperature = 0;     // <= 0: use the local configuration (KD_MODE_LOGIT only)
static float teacheralpha = -1;          // <  0: use the local configuration
static int teacherlatentlayerindex = -1; // KD_MODE_LATENT only

static bool ValidTemperature(float temperature){
  return temperature > 0 && isfinite(temperature);
}

static bool ValidAlpha(float alpha){
  return alpha >= 0 && alpha <= 1;
}

// walks from the first layer, index 0 = the input layer; NULL if out of range
static Layer *GetLayerAtIndex(const NeuralNetwork *network, int index){
  if (network == NULL || index < 0) {
    return NULL;
  }
  Layer *layer = network->firstlayer;
  for (int i = 0; i < index && layer != NULL; i++) {
    layer = layer->nextlayer;
  }
  return layer;
}

//////////////////////////////////////////////////CONFIGURATION//////////////////////////////////////////////////

int KnowledgeDistillationEnable(float temperature, float alpha){
  if (!ValidTemperature(temperature) || !ValidAlpha(alpha)) {
    LOG_ERR("Invalid distillation parameters (T=%f, alpha=%f)", (double)temperature, (double)alpha);
    return -1;
  }
  localconfig.enabled = true;
  localconfig.temperature = temperature;
  localconfig.alpha = alpha;
  LOG_INF("Knowledge distillation enabled (T=%.2f, alpha=%.2f)", (double)temperature, (double)alpha);
  return 0;
}

void KnowledgeDistillationDisable(void){
  localconfig.enabled = false;
  LOG_INF("Knowledge distillation disabled");
}

bool KnowledgeDistillationIsEnabled(void){
  return localconfig.enabled;
}

KnowledgeDistillationConfig KnowledgeDistillationGetConfig(void){
  KnowledgeDistillationConfig config = localconfig;
  config.mode = teachermode;
  if (teachermode == KD_MODE_LOGIT && ValidTemperature(teachertemperature)) {
    config.temperature = teachertemperature;
  }
  if (ValidAlpha(teacheralpha)) {
    config.alpha = teacheralpha;
  }
  config.latentlayerindex = teacherlatentlayerindex;
  return config;
}

//////////////////////////////////////////////////TEACHER//////////////////////////////////////////////////

void KnowledgeDistillationClearTeacher(void){
  freeNeuralNetwork(teacher);
  teacher = NULL;
  teachermode = KD_MODE_LOGIT;
  teachertemperature = 0;
  teacheralpha = -1;
  teacherlatentlayerindex = -1;
}

int KnowledgeDistillationSetTeacher(FederatedLearning *newteacher, int mode, float temperature, float alpha,
                                    int latentlayerindex){
  if (newteacher == NULL) {
    return -1;
  }

  NeuralNetwork *network = newteacher->neuralnetwork;
  newteacher->neuralnetwork = NULL;
  freeFederatedLearning(newteacher);

  if (network == NULL || network->firstlayer == NULL || network->lastlayer == NULL) {
    LOG_ERR("Teacher rejected: empty network");
    freeNeuralNetwork(network);
    return -1;
  }

  if (mode == KD_MODE_LOGIT) {
    if (network->lastlayer->activationfunctiontype != SOFTMAX) {
      LOG_ERR("Teacher rejected: KD_MODE_LOGIT needs a SOFTMAX output layer");
      freeNeuralNetwork(network);
      return -1;
    }
  } else if (mode == KD_MODE_LATENT) {
    // the latent layer must be an actual hidden layer: not the input (0) nor the output (layers-1)
    if (latentlayerindex < 1 || latentlayerindex >= network->layers - 1) {
      LOG_ERR("Teacher rejected: invalid latentlayerindex %d for a %d-layer network", latentlayerindex, network->layers);
      freeNeuralNetwork(network);
      return -1;
    }
  } else {
    LOG_ERR("Teacher rejected: unknown distillation mode %d", mode);
    freeNeuralNetwork(network);
    return -1;
  }

  KnowledgeDistillationClearTeacher();
  teacher = network;
  teachermode = mode;
  teachertemperature = ValidTemperature(temperature) ? temperature : 0;
  teacheralpha = ValidAlpha(alpha) ? alpha : -1;
  teacherlatentlayerindex = (mode == KD_MODE_LATENT) ? latentlayerindex : -1;

  KnowledgeDistillationConfig config = KnowledgeDistillationGetConfig();
  if (mode == KD_MODE_LATENT) {
    LOG_INF("Teacher updated: %d layers, mode=latent, latentlayerindex=%d, alpha=%.2f",
            teacher->layers, teacherlatentlayerindex, (double)config.alpha);
  } else {
    LOG_INF("Teacher updated: %d layers, mode=logit, T=%.2f, alpha=%.2f",
            teacher->layers, (double)config.temperature, (double)config.alpha);
  }
  return 0;
}

NeuralNetwork *KnowledgeDistillationGetTeacher(void){
  return teacher;
}

bool KnowledgeDistillationTeacherCompatible(const NeuralNetwork *student){
  if (teacher == NULL || student == NULL || teacher->firstlayer->neurons != student->firstlayer->neurons) {
    return false;
  }

  if (teachermode == KD_MODE_LATENT) {
    Layer *latentlayer = GetLayerAtIndex(teacher, teacherlatentlayerindex);
    Layer *hintlayer = student->lastlayer->previouslayer;
    return latentlayer != NULL && hintlayer != NULL && latentlayer->neurons == hintlayer->neurons;
  }

  // KD_MODE_LOGIT
  return teacher->lastlayer->neurons == student->lastlayer->neurons &&
         student->lastlayer->activationfunctiontype == SOFTMAX;
}

//////////////////////////////////////////////////LOSS: KD_MODE_LOGIT//////////////////////////////////////////////////

// numerically stable softmax(logits / temperature)
static void TemperedSoftMax(const float *logits, int n, float temperature, float *probabilities){
  float maxlogit = logits[0];
  for (int i = 1; i < n; i++) {
    if (logits[i] > maxlogit) {
      maxlogit = logits[i];
    }
  }

  float sum = 0;
  for (int i = 0; i < n; i++) {
    probabilities[i] = expf((logits[i] - maxlogit) / temperature);
    sum += probabilities[i];
  }

  for (int i = 0; i < n; i++) {
    probabilities[i] /= sum;
  }
}

float KnowledgeDistillationLoss(const float *label, const float *studentlogits, const float *teacherlogits,
                                int n, float temperature, float alpha){
  float studentprobabilities[n];
  float studentsoft[n];
  float teachersoft[n];

  TemperedSoftMax(studentlogits, n, 1.0f, studentprobabilities);
  TemperedSoftMax(studentlogits, n, temperature, studentsoft);
  TemperedSoftMax(teacherlogits, n, temperature, teachersoft);

  float crossentropy = 0;
  float kullbackleibler = 0;
  for (int i = 0; i < n; i++) {
    crossentropy -= label[i] * logf(fmaxf(studentprobabilities[i], KD_MIN_PROBABILITY));
    if (teachersoft[i] > 0) {
      kullbackleibler += teachersoft[i] * (logf(fmaxf(teachersoft[i], KD_MIN_PROBABILITY)) -
                                           logf(fmaxf(studentsoft[i], KD_MIN_PROBABILITY)));
    }
  }

  // T^2 keeps the gradient scale of the soft term independent of the temperature
  return (1 - alpha) * crossentropy + alpha * temperature * temperature * kullbackleibler;
}

void KnowledgeDistillationOutputDelta(const float *label, const float *studentlogits, const float *teacherlogits,
                                      int n, float temperature, float alpha, float *delta){
  float studentprobabilities[n];
  float studentsoft[n];
  float teachersoft[n];

  TemperedSoftMax(studentlogits, n, 1.0f, studentprobabilities);
  TemperedSoftMax(studentlogits, n, temperature, studentsoft);
  TemperedSoftMax(teacherlogits, n, temperature, teachersoft);

  for (int i = 0; i < n; i++) {
    delta[i] = (1 - alpha) * (studentprobabilities[i] - label[i]) +
               alpha * temperature * (studentsoft[i] - teachersoft[i]);
  }
}

//////////////////////////////////////////////////LOSS: KD_MODE_LATENT//////////////////////////////////////////////////

float KnowledgeDistillationFeatureLoss(const float *studenthidden, const float *teacherhidden, int n){
  float sum = 0;
  for (int i = 0; i < n; i++) {
    float diff = studenthidden[i] - teacherhidden[i];
    sum += diff * diff;
  }
  return sum / n;
}

void KnowledgeDistillationFeatureDelta(const float *studenthidden, const float *teacherhidden, int n, float *delta){
  for (int i = 0; i < n; i++) {
    delta[i] = (2.0f / n) * (studenthidden[i] - teacherhidden[i]);
  }
}
