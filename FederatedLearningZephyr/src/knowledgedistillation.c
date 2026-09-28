#include <math.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "knowledgedistillation.h"

LOG_MODULE_REGISTER(KnowledgeDistillation, LOG_LEVEL_INF);

#define KD_MIN_PROBABILITY 1e-7f

// local configuration (Kconfig defaults, changed by the runtime API)
static KnowledgeDistillationConfig localconfig = {
  .enabled = IS_ENABLED(CONFIG_FL_KNOWLEDGE_DISTILLATION),
  .temperature = CONFIG_FL_KD_TEMPERATURE_X10 / 10.0f,
  .alpha = CONFIG_FL_KD_ALPHA_PERCENT / 100.0f,
};

static NeuralNetwork *teacher = NULL;
// hyperparameters sent with the teacher (<= 0: use the local configuration)
static float teachertemperature = 0;
static float teacheralpha = -1;

static bool ValidTemperature(float temperature){
  return temperature > 0 && isfinite(temperature);
}

static bool ValidAlpha(float alpha){
  return alpha >= 0 && alpha <= 1;
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
  if (ValidTemperature(teachertemperature)) {
    config.temperature = teachertemperature;
  }
  if (ValidAlpha(teacheralpha)) {
    config.alpha = teacheralpha;
  }
  return config;
}

//////////////////////////////////////////////////TEACHER//////////////////////////////////////////////////

void KnowledgeDistillationClearTeacher(void){
  freeNeuralNetwork(teacher);
  teacher = NULL;
  teachertemperature = 0;
  teacheralpha = -1;
}

int KnowledgeDistillationSetTeacher(FederatedLearning *newteacher, float temperature, float alpha){
  if (newteacher == NULL) {
    return -1;
  }

  NeuralNetwork *network = newteacher->neuralnetwork;
  newteacher->neuralnetwork = NULL;
  freeFederatedLearning(newteacher);

  if (network == NULL || network->firstlayer == NULL || network->lastlayer == NULL ||
      network->lastlayer->activationfunctiontype != SOFTMAX) {
    LOG_ERR("Teacher rejected: it needs a SOFTMAX output layer");
    freeNeuralNetwork(network);
    return -1;
  }

  KnowledgeDistillationClearTeacher();
  teacher = network;
  teachertemperature = ValidTemperature(temperature) ? temperature : 0;
  teacheralpha = ValidAlpha(alpha) ? alpha : -1;

  KnowledgeDistillationConfig config = KnowledgeDistillationGetConfig();
  LOG_INF("Teacher updated: %d layers, T=%.2f, alpha=%.2f", teacher->layers,
          (double)config.temperature, (double)config.alpha);
  return 0;
}

NeuralNetwork *KnowledgeDistillationGetTeacher(void){
  return teacher;
}

bool KnowledgeDistillationTeacherCompatible(const NeuralNetwork *student){
  return teacher != NULL && student != NULL &&
         teacher->firstlayer->neurons == student->firstlayer->neurons &&
         teacher->lastlayer->neurons == student->lastlayer->neurons &&
         student->lastlayer->activationfunctiontype == SOFTMAX;
}

//////////////////////////////////////////////////LOSS//////////////////////////////////////////////////

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
