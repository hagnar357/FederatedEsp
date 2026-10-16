#ifndef _jsonconverter
#define _jsonconverter

#include "federatedlearning.h"
#include <cJSON.h>

cJSON* federatedLearningToJSON(const FederatedLearning* federatedLearning);
FederatedLearning* JSONToFederatedLearning(const cJSON* json);
// the "flat" network of GET /api/getteachermodelflat (weights and biases as comma separated strings)
FederatedLearning* JSONFlatToFederatedLearning(const cJSON* json);
// the model as one flat JSON text, written without a cJSON tree (the node's upload); round < 0 and type NULL are
// left out. malloc'ed, free() it; NULL when there is no memory or the network is not fully connected
char* federatedLearningToFlatString(const FederatedLearning* federatedLearning, int round, const char* type);

#endif