#ifndef JSONCONVERTER_H
#define JSONCONVERTER_H

#include "federatedlearning.h"
#include "../lib/cJSON.h"

cJSON* FederatedLearningToJSON(FederatedLearning* federatedLearning);
FederatedLearning* JSONToFederatedLearning(const cJSON* json);

#endif