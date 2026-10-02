#ifndef JSONCONVERTER_H
#define JSONCONVERTER_H

#include "federatedlearning.h"
#include "../lib/cJSON.h"

cJSON* NeuralNetworkToJSON(const NeuralNetwork* neuralnetwork);
cJSON* FederatedLearningToJSON(FederatedLearning* federatedLearning);
cJSON* TeacherTaskToJSON();
cJSON* TeacherModelToJSON();
FederatedLearning* JSONToFederatedLearning(const cJSON* json);

#endif