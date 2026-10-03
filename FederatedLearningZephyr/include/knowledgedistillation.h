#ifndef _knowledgedistillation
#define _knowledgedistillation

#include <stdbool.h>
#include "federatedlearning.h"

/*
 * Knowledge distillation (KD) from a teacher model sent by the server. Two modes:
 *
 * KD_MODE_LOGIT (classifier teacher, output-level distillation):
 *   Loss per sample (z_s: student logits, z_t: teacher logits, T: temperature, alpha: distillation weight):
 *     L = (1 - alpha) * CE(y, softmax(z_s)) + alpha * T^2 * KL(softmax(z_t / T) || softmax(z_s / T))
 *   Gradient w.r.t. the student logits:
 *     dL/dz_s_i = (1 - alpha) * (p_i - y_i) + alpha * T * (q_s_i - q_t_i)
 *   The teacher needs the same number of inputs/outputs as the student and a SOFTMAX output layer.
 *
 * KD_MODE_LATENT (autoencoder teacher, latent/hidden-layer distillation):
 *   Loss per sample (h_s: student's last hidden layer, h_t: teacher's latent layer, both post-activation,
 *   same width n):
 *     L = (1 - alpha) * CE(y, softmax(z_s)) + alpha * MSE(h_s, h_t)
 *   KnowledgeDistillationFeatureDelta gives dMSE/d(h_s_i), i.e. w.r.t. the hint layer's POST-activation
 *   value. The caller must multiply it by alpha AND by the hint layer's own activation derivative
 *   (ActivationFunctionDerivativeCalculation) to get dLoss/d(preactivation) before adding it, as hintdelta,
 *   to what BackPropagation already computes there from the (1 - alpha)-scaled output-layer delta:
 *     hintdelta_i = alpha * (2/n) * (h_s_i - h_t_i) * f'(Z_i)
 *   temperature is accepted but unused in this mode. The teacher needs the same number of inputs as the
 *   student, and its layer at latentlayerindex must have the same width as the student's last hidden layer
 *   (student->lastlayer->previouslayer). The teacher's own output layer/activation is irrelevant here.
 *
 * All functions must be called from the same thread that runs the training.
 */

#define KD_MODE_LOGIT 0
#define KD_MODE_LATENT 1

typedef struct KnowledgeDistillationConfig {
  bool enabled;
  int mode;             // KD_MODE_LOGIT or KD_MODE_LATENT (from the teacher once one is set)
  float temperature;    // KD_MODE_LOGIT only
  float alpha;
  int latentlayerindex; // KD_MODE_LATENT only; 0-based, 0 = teacher's input layer
} KnowledgeDistillationConfig;

// runtime activation; the defaults come from CONFIG_FL_KNOWLEDGE_DISTILLATION, CONFIG_FL_KD_TEMPERATURE_X10
// and CONFIG_FL_KD_ALPHA_PERCENT. mode/latentlayerindex are only ever known once a teacher is received.
int KnowledgeDistillationEnable(float temperature, float alpha);
void KnowledgeDistillationDisable(void);
bool KnowledgeDistillationIsEnabled(void);

// effective configuration: the values sent with the teacher override the local ones
KnowledgeDistillationConfig KnowledgeDistillationGetConfig(void);

// takes ownership of teacher (also on error). mode: KD_MODE_LOGIT or KD_MODE_LATENT.
// temperature/alpha <= 0 or out of range keep the local defaults (KD_MODE_LOGIT only; ignored for LATENT).
// latentlayerindex is required (and validated) when mode == KD_MODE_LATENT.
int KnowledgeDistillationSetTeacher(FederatedLearning *teacher, int mode, float temperature, float alpha,
                                    int latentlayerindex);
NeuralNetwork *KnowledgeDistillationGetTeacher(void);
void KnowledgeDistillationClearTeacher(void);

// checks the current teacher against student per the active mode (see the comment above)
bool KnowledgeDistillationTeacherCompatible(const NeuralNetwork *student);

// KD_MODE_LOGIT math (output logits, temperature-scaled softmax + KL)
float KnowledgeDistillationLoss(const float *label, const float *studentlogits, const float *teacherlogits,
                                int n, float temperature, float alpha);
void KnowledgeDistillationOutputDelta(const float *label, const float *studentlogits, const float *teacherlogits,
                                      int n, float temperature, float alpha, float *delta);

// KD_MODE_LATENT math (hidden-layer feature matching, mean squared error)
float KnowledgeDistillationFeatureLoss(const float *studenthidden, const float *teacherhidden, int n);
void KnowledgeDistillationFeatureDelta(const float *studenthidden, const float *teacherhidden, int n, float *delta);

#endif
