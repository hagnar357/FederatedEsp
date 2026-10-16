#ifndef JSONCONVERTER_H
#define JSONCONVERTER_H

#include "federatedlearning.h"
#include "../lib/cJSON.h"

cJSON* NeuralNetworkToJSON(const NeuralNetwork* neuralnetwork);
cJSON* FederatedLearningToJSON(FederatedLearning* federatedLearning);
cJSON* TeacherTaskToJSON();
cJSON* TeacherModelToJSON();
cJSON* TeacherModelForNodesToJSON();
cJSON* TeacherModelForNodesFlatToJSON();
cJSON* NeuralNetworkToFlatJSON(const NeuralNetwork* neuralnetwork);
FederatedLearning* JSONToFederatedLearning(const cJSON* json);
cJSON* FederatedLearningToFlatJSON(FederatedLearning* federatedLearning);
// the nodes' uploads in the flat form ("format":"flat")
FederatedLearning* JSONFlatToFederatedLearning(const cJSON* json);

#endif