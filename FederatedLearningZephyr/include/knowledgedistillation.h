#ifndef _knowledgedistillation
#define _knowledgedistillation

#include <stdbool.h>
#include "federatedlearning.h"

/*
 * Knowledge distillation (KD) from a teacher model sent by the server.
 *
 * Loss per sample (z_s: student logits, z_t: teacher logits, T: temperature, alpha: distillation weight):
 *   L = (1 - alpha) * CE(y, softmax(z_s)) + alpha * T^2 * KL(softmax(z_t / T) || softmax(z_s / T))
 * Gradient w.r.t. the student logits:
 *   dL/dz_s_i = (1 - alpha) * (p_i - y_i) + alpha * T * (q_s_i - q_t_i)
 *
 * The student keeps the global model architecture (the server validates it). The teacher may have any
 * architecture with the same number of inputs and outputs. All functions must be called from the same
 * thread that runs the training.
 */

typedef struct KnowledgeDistillationConfig {
  bool enabled;
  float temperature;
  float alpha;
} KnowledgeDistillationConfig;

// runtime activation; the defaults come from CONFIG_FL_KNOWLEDGE_DISTILLATION, CONFIG_FL_KD_TEMPERATURE_X10
// and CONFIG_FL_KD_ALPHA_PERCENT
int KnowledgeDistillationEnable(float temperature, float alpha);
void KnowledgeDistillationDisable(void);
bool KnowledgeDistillationIsEnabled(void);

// effective configuration: the values sent with the teacher override the local ones
KnowledgeDistillationConfig KnowledgeDistillationGetConfig(void);

// takes ownership of teacher (also on error); temperature/alpha <= 0 or out of range keep the local values
int KnowledgeDistillationSetTeacher(FederatedLearning *teacher, float temperature, float alpha);
NeuralNetwork *KnowledgeDistillationGetTeacher(void);
void KnowledgeDistillationClearTeacher(void);

// teacher and student have the same input and output sizes and a SOFTMAX output layer
bool KnowledgeDistillationTeacherCompatible(const NeuralNetwork *student);

float KnowledgeDistillationLoss(const float *label, const float *studentlogits, const float *teacherlogits,
                                int n, float temperature, float alpha);
void KnowledgeDistillationOutputDelta(const float *label, const float *studentlogits, const float *teacherlogits,
                                      int n, float temperature, float alpha, float *delta);

#endif
