#include "JSONConverter.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

//JSON TO FFEDERATED LEARNING  
cJSON* federatedLearningToJSON(const FederatedLearning* federatedLearning) {
    cJSON* root = cJSON_CreateObject();

    cJSON_AddItemToObject(root, "globalmodelstatus", cJSON_CreateNumber(federatedLearning->globalmodelstatus));
    cJSON_AddItemToObject(root, "trainingscounter", cJSON_CreateNumber(federatedLearning->trainingscounter));
    
    cJSON* jsonNeuralNetwork = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "neuralnetwork", jsonNeuralNetwork);

    cJSON_AddItemToObject(jsonNeuralNetwork, "epoch", cJSON_CreateNumber(federatedLearning->neuralnetwork->epoch));
    cJSON_AddItemToObject(jsonNeuralNetwork, "alpha", cJSON_CreateNumber(federatedLearning->neuralnetwork->alpha));
    cJSON_AddItemToObject(jsonNeuralNetwork, "regularization", cJSON_CreateNumber(federatedLearning->neuralnetwork->regularization));
    cJSON_AddItemToObject(jsonNeuralNetwork, "lambda", cJSON_CreateNumber(federatedLearning->neuralnetwork->lambda));
    cJSON_AddItemToObject(jsonNeuralNetwork, "percentualtraining", cJSON_CreateNumber(federatedLearning->neuralnetwork->percentualtraining));
    cJSON_AddItemToObject(jsonNeuralNetwork, "lossfunctiontype", cJSON_CreateNumber(federatedLearning->neuralnetwork->lossfunctiontype));
    cJSON_AddItemToObject(jsonNeuralNetwork, "layers", cJSON_CreateNumber(federatedLearning->neuralnetwork->layers));

    cJSON* layersArray = cJSON_CreateArray();
    struct Layer* currentLayer = federatedLearning->neuralnetwork->firstlayer;

    while (currentLayer != NULL) {
        cJSON* layerObject = cJSON_CreateObject();
        cJSON_AddItemToObject(layerObject, "activationfunctiontype", cJSON_CreateNumber(currentLayer->activationfunctiontype));
        cJSON_AddItemToObject(layerObject, "neurons", cJSON_CreateNumber(currentLayer->neurons));

        cJSON* neuronsArray = cJSON_CreateArray();
        struct Neuron* currentNeuron = currentLayer->firstneuron;

        while (currentNeuron != NULL) {
            cJSON* neuronObject = cJSON_CreateObject();
            cJSON_AddItemToObject(neuronObject, "neurontype", cJSON_CreateString(currentNeuron->neurontype));
            cJSON_AddItemToObject(neuronObject, "weights", cJSON_CreateNumber(currentNeuron->weights));
            cJSON_AddItemToObject(neuronObject, "bias", cJSON_CreateNumber(currentNeuron->bias));

            cJSON* weightsArray = cJSON_CreateArray();
            struct Weight* currentWeight = currentNeuron->firstweight;

            while (currentWeight != NULL) {
                cJSON_AddItemToArray(weightsArray, cJSON_CreateNumber(currentWeight->weight));
                currentWeight = currentWeight->nextweight;
            }

            cJSON_AddItemToObject(neuronObject, "weightsArray", weightsArray);

            cJSON_AddItemToArray(neuronsArray, neuronObject);
            currentNeuron = currentNeuron->nextneuron;
        }

        cJSON_AddItemToObject(layerObject, "neuronsArray", neuronsArray);
        cJSON_AddItemToArray(layersArray, layerObject);
        currentLayer = currentLayer->nextlayer;
    }

    cJSON_AddItemToObject(jsonNeuralNetwork, "layersArray", layersArray);



    return root;
}

//JSON TO FEDERATED LEARNING

static int GetInt(const cJSON* object, const char* name, int* value) {
    cJSON* item = cJSON_GetObjectItem(object, name);
    if (!cJSON_IsNumber(item)) {
        return 0;
    }
    *value = item->valueint;
    return 1;
}

static int GetFloat(const cJSON* object, const char* name, float* value) {
    cJSON* item = cJSON_GetObjectItem(object, name);
    if (!cJSON_IsNumber(item)) {
        return 0;
    }
    *value = (float)item->valuedouble;
    return 1;
}

static Neuron* JSONToNeuron(const cJSON* neuronElement) {

    Neuron* neuron = (Neuron*)calloc(1, sizeof(Neuron));
    if (neuron == NULL) {
        return NULL;
    }

    cJSON* neurontypeItem = cJSON_GetObjectItem(neuronElement, "neurontype");
    cJSON* weightsArrayItem = cJSON_GetObjectItem(neuronElement, "weightsArray");

    if (!cJSON_IsString(neurontypeItem) ||
        !GetInt(neuronElement, "weights", &neuron->weights) ||
        !GetFloat(neuronElement, "bias", &neuron->bias) ||
        !cJSON_IsArray(weightsArrayItem)) {
        free(neuron);
        return NULL;
    }

    strncpy(neuron->neurontype, neurontypeItem->valuestring, sizeof(neuron->neurontype) - 1);
    neuron->neurontype[sizeof(neuron->neurontype) - 1] = '\0';

    int k = 0;
    cJSON* weightElement;
    cJSON_ArrayForEach(weightElement, weightsArrayItem) {

        Weight* weight = (Weight*)calloc(1, sizeof(Weight));
        if (weight == NULL || !cJSON_IsNumber(weightElement)) {
            free(weight);
            k = -1;
            break;
        }

        weight->weight = (float)weightElement->valuedouble;

        if (neuron->lastweight == NULL) {
            neuron->firstweight = neuron->lastweight = weight;
        } else {
            neuron->lastweight->nextweight = weight;
            weight->previousweight = neuron->lastweight;
            neuron->lastweight = weight;
        }
        k++;
    }

    //the weights counter must match the weights list
    if (k != neuron->weights) {
        Weight* currentweight = neuron->firstweight;
        while (currentweight != NULL) {
            Weight* nextweight = currentweight->nextweight;
            free(currentweight);
            currentweight = nextweight;
        }
        free(neuron);
        return NULL;
    }

    return neuron;
}

static int JSONToLayers(const cJSON* layersArrayItem, NeuralNetwork* neuralnetwork) {

    int i = 0;
    cJSON* layerElement;
    cJSON_ArrayForEach(layerElement, layersArrayItem) {

        Layer* layer = (Layer*)calloc(1, sizeof(Layer));
        if (layer == NULL) {
            return 0;
        }

        //link first so freeNeuralNetwork releases it on error
        if (neuralnetwork->lastlayer == NULL) {
            neuralnetwork->firstlayer = neuralnetwork->lastlayer = layer;
        } else {
            neuralnetwork->lastlayer->nextlayer = layer;
            layer->previouslayer = neuralnetwork->lastlayer;
            neuralnetwork->lastlayer = layer;
        }
        i++;

        cJSON* neuronsArrayItem = cJSON_GetObjectItem(layerElement, "neuronsArray");
        if (!GetInt(layerElement, "neurons", &layer->neurons) ||
            !GetInt(layerElement, "activationfunctiontype", &layer->activationfunctiontype) ||
            !cJSON_IsArray(neuronsArrayItem)) {
            return 0;
        }

        int j = 0;
        cJSON* neuronElement;
        cJSON_ArrayForEach(neuronElement, neuronsArrayItem) {

            Neuron* neuron = JSONToNeuron(neuronElement);
            if (neuron == NULL) {
                return 0;
            }

            if (layer->lastneuron == NULL) {
                layer->firstneuron = layer->lastneuron = neuron;
            } else {
                layer->lastneuron->nextneuron = neuron;
                neuron->previousneuron = layer->lastneuron;
                layer->lastneuron = neuron;
            }
            j++;
        }

        if (j != layer->neurons || j == 0) {
            return 0;
        }
    }

    return i == neuralnetwork->layers;
}

FederatedLearning* JSONToFederatedLearning(const cJSON* json) {
    if (json == NULL || !cJSON_IsObject(json)) {
        printf("Não foi possível converter.\n");
        return NULL;
    }

    FederatedLearning* federatedLearning = (FederatedLearning*)calloc(1, sizeof(FederatedLearning));
    if (federatedLearning == NULL) {
        return NULL;  // Falha na alocação de memória
    }

    GetInt(json, "globalmodelstatus", &federatedLearning->globalmodelstatus);
    GetInt(json, "trainingscounter", &federatedLearning->trainingscounter);

    cJSON* neuralNetworkItem = cJSON_GetObjectItem(json, "neuralnetwork");
    if (!cJSON_IsObject(neuralNetworkItem)) {
        printf("JSON sem o campo neuralnetwork.\n");
        freeFederatedLearning(federatedLearning);
        return NULL;
    }

    NeuralNetwork* neuralnetwork = (NeuralNetwork*)calloc(1, sizeof(NeuralNetwork));
    if (neuralnetwork == NULL) {
        freeFederatedLearning(federatedLearning);
        return NULL;
    }
    federatedLearning->neuralnetwork = neuralnetwork;

    GetInt(neuralNetworkItem, "epoch", &neuralnetwork->epoch);
    GetFloat(neuralNetworkItem, "alpha", &neuralnetwork->alpha);
    GetInt(neuralNetworkItem, "regularization", &neuralnetwork->regularization);
    GetFloat(neuralNetworkItem, "lambda", &neuralnetwork->lambda);
    GetInt(neuralNetworkItem, "percentualtraining", &neuralnetwork->percentualtraining);
    GetInt(neuralNetworkItem, "lossfunctiontype", &neuralnetwork->lossfunctiontype);

    cJSON* layersArrayItem = cJSON_GetObjectItem(neuralNetworkItem, "layersArray");

    if (!GetInt(neuralNetworkItem, "layers", &neuralnetwork->layers) ||
        neuralnetwork->layers < 2 ||
        !cJSON_IsArray(layersArrayItem) ||
        !JSONToLayers(layersArrayItem, neuralnetwork)) {
        printf("Estrutura da rede neural inválida no JSON.\n");
        freeFederatedLearning(federatedLearning);
        return NULL;
    }

    return federatedLearning;
}

//FLAT FORM OF THE NETWORK (see NeuralNetworkToFlatJSON in the server)
//the weights and biases arrive as comma separated strings; they are read with strtof straight into the Weight
//lists, so no cJSON node is created per number (that was ~76 B/weight, more than a board with 80 KB of heap has)

//reads the next float of the list and moves the cursor past the comma; 0 when there is none
static int NextFloat(const char** cursor, float* value) {
    char* end = NULL;
    float v = strtof(*cursor, &end);
    if (end == *cursor) {
        return 0;
    }
    *value = v;
    *cursor = (*end == ',') ? end + 1 : end;
    return 1;
}

//the integers of a JSON array into out[count]; 0 when the array does not have exactly count numbers
static int GetIntArray(const cJSON* object, const char* name, int* out, int count) {
    cJSON* array = cJSON_GetObjectItem(object, name);
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) != count) {
        return 0;
    }
    int i = 0;
    cJSON* element;
    cJSON_ArrayForEach(element, array) {
        if (!cJSON_IsNumber(element)) {
            return 0;
        }
        out[i++] = element->valueint;
    }
    return 1;
}

static int FlatToLayers(const cJSON* networkItem, NeuralNetwork* neuralnetwork) {
    int layers = neuralnetwork->layers;
    int* neurons = (int*)malloc(2 * layers * sizeof(int));
    if (neurons == NULL) {
        return 0;
    }
    int* activations = neurons + layers;

    cJSON* weightsItem = cJSON_GetObjectItem(networkItem, "weights");
    cJSON* biasesItem = cJSON_GetObjectItem(networkItem, "biases");
    if (!GetIntArray(networkItem, "neurons", neurons, layers) ||
        !GetIntArray(networkItem, "activations", activations, layers) ||
        !cJSON_IsString(weightsItem) || !cJSON_IsString(biasesItem)) {
        free(neurons);
        return 0;
    }

    const char* weightscursor = weightsItem->valuestring;
    const char* biasescursor = biasesItem->valuestring;
    int ok = 1;

    for (int i = 0; i < layers && ok; i++) {
        Layer* layer = (Layer*)calloc(1, sizeof(Layer));
        if (layer == NULL) {
            ok = 0;
            break;
        }
        //link first so freeNeuralNetwork releases it on error
        if (neuralnetwork->lastlayer == NULL) {
            neuralnetwork->firstlayer = neuralnetwork->lastlayer = layer;
        } else {
            neuralnetwork->lastlayer->nextlayer = layer;
            layer->previouslayer = neuralnetwork->lastlayer;
            neuralnetwork->lastlayer = layer;
        }
        layer->neurons = neurons[i];
        layer->activationfunctiontype = activations[i];
        if (layer->neurons <= 0) {
            ok = 0;
            break;
        }

        //fully connected: as many weights as neurons in the previous layer (none in the input layer)
        int weightsperneuron = (i == 0) ? 0 : neurons[i - 1];
        const char* type = (i == 0) ? "INPUT" : (i == layers - 1) ? "OUTPUT" : "HIDDEN";

        for (int j = 0; j < layer->neurons && ok; j++) {
            Neuron* neuron = (Neuron*)calloc(1, sizeof(Neuron));
            if (neuron == NULL) {
                ok = 0;
                break;
            }
            if (layer->lastneuron == NULL) {
                layer->firstneuron = layer->lastneuron = neuron;
            } else {
                layer->lastneuron->nextneuron = neuron;
                neuron->previousneuron = layer->lastneuron;
                layer->lastneuron = neuron;
            }
            strncpy(neuron->neurontype, type, sizeof(neuron->neurontype) - 1);
            neuron->weights = weightsperneuron;
            if (!NextFloat(&biasescursor, &neuron->bias)) {
                ok = 0;
                break;
            }
            for (int k = 0; k < weightsperneuron; k++) {
                Weight* weight = (Weight*)calloc(1, sizeof(Weight));
                if (weight == NULL) {
                    ok = 0;
                    break;
                }
                if (neuron->lastweight == NULL) {
                    neuron->firstweight = neuron->lastweight = weight;
                } else {
                    neuron->lastweight->nextweight = weight;
                    weight->previousweight = neuron->lastweight;
                    neuron->lastweight = weight;
                }
                if (!NextFloat(&weightscursor, &weight->weight)) {
                    ok = 0;
                    break;
                }
            }
        }
    }
    free(neurons);

    //both lists must be used up exactly: a shorter or longer list means the sender and this node disagree
    while (*weightscursor == ' ') weightscursor++;
    while (*biasescursor == ' ') biasescursor++;
    return ok && *weightscursor == '\0' && *biasescursor == '\0';
}

FederatedLearning* JSONFlatToFederatedLearning(const cJSON* json) {
    if (json == NULL || !cJSON_IsObject(json)) {
        printf("Não foi possível converter.\n");
        return NULL;
    }

    FederatedLearning* federatedLearning = (FederatedLearning*)calloc(1, sizeof(FederatedLearning));
    if (federatedLearning == NULL) {
        return NULL;
    }

    GetInt(json, "globalmodelstatus", &federatedLearning->globalmodelstatus);
    GetInt(json, "trainingscounter", &federatedLearning->trainingscounter);

    cJSON* networkItem = cJSON_GetObjectItem(json, "neuralnetwork");
    if (!cJSON_IsObject(networkItem)) {
        printf("JSON sem o campo neuralnetwork.\n");
        freeFederatedLearning(federatedLearning);
        return NULL;
    }

    NeuralNetwork* neuralnetwork = (NeuralNetwork*)calloc(1, sizeof(NeuralNetwork));
    if (neuralnetwork == NULL) {
        freeFederatedLearning(federatedLearning);
        return NULL;
    }
    federatedLearning->neuralnetwork = neuralnetwork;

    GetInt(networkItem, "epoch", &neuralnetwork->epoch);
    GetFloat(networkItem, "alpha", &neuralnetwork->alpha);
    GetInt(networkItem, "regularization", &neuralnetwork->regularization);
    GetFloat(networkItem, "lambda", &neuralnetwork->lambda);
    GetInt(networkItem, "percentualtraining", &neuralnetwork->percentualtraining);
    GetInt(networkItem, "lossfunctiontype", &neuralnetwork->lossfunctiontype);

    if (!GetInt(networkItem, "layers", &neuralnetwork->layers) || neuralnetwork->layers < 2 || neuralnetwork->layers > 64 ||
        !FlatToLayers(networkItem, neuralnetwork)) {
        printf("Estrutura da rede neural inválida no JSON plano.\n");
        freeFederatedLearning(federatedLearning);
        return NULL;
    }

    return federatedLearning;
}

//THE MODEL AS ONE FLAT JSON TEXT (the node's upload, see NeuralNetworkToFlatJSON in the server for the format)
//written straight into one buffer: the cJSON tree of the same model is ~76 B per weight, more than a board with
//80 KB of heap can hold next to the teacher; here 260 weights take ~5 KB
static int FlatAppend(char* buf, size_t cap, size_t* len, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + *len, cap - *len, fmt, args);
    va_end(args);
    if (n < 0 || (size_t)n >= cap - *len) {
        return 0;
    }
    *len += (size_t)n;
    return 1;
}

char* federatedLearningToFlatString(const FederatedLearning* federatedLearning, int round, const char* type) {
    if (federatedLearning == NULL || federatedLearning->neuralnetwork == NULL) {
        return NULL;
    }
    const NeuralNetwork* network = federatedLearning->neuralnetwork;

    size_t weightscount = 0, biasescount = 0;
    int layers = 0;
    for (const Layer* l = network->firstlayer; l != NULL; l = l->nextlayer) {
        layers++;
        for (const Neuron* n = l->firstneuron; n != NULL; n = n->nextneuron) {
            biasescount++;
            const Weight* w = n->firstweight;
            for (int k = 0; k < n->weights; k++, w = w->nextweight) {
                if (w == NULL) {
                    return NULL;
                }
                weightscount++;
            }
            //fully connected: as many weights as neurons in the previous layer
            if (n->weights != (l->previouslayer != NULL ? l->previouslayer->neurons : 0)) {
                return NULL;
            }
        }
    }
    if (layers != network->layers) {
        return NULL;
    }

    // header + per layer entries + 16 bytes per number ("-1.23456789e-05," is the longest)
    size_t cap = 640 + (type != NULL ? strlen(type) : 0) + (size_t)layers * 24 + (weightscount + biasescount) * 16;
    char* buf = (char*)malloc(cap);
    if (buf == NULL) {
        return NULL;
    }
    size_t len = 0;
    int ok = FlatAppend(buf, cap, &len, "{\"format\":\"flat\",\"globalmodelstatus\":%d,\"trainingscounter\":%d",
                        federatedLearning->globalmodelstatus, federatedLearning->trainingscounter);
    if (round >= 0) {
        ok = ok && FlatAppend(buf, cap, &len, ",\"round\":%d", round);
    }
    if (type != NULL) {
        ok = ok && FlatAppend(buf, cap, &len, ",\"type\":\"%s\"", type);
    }
    ok = ok && FlatAppend(buf, cap, &len,
                          ",\"neuralnetwork\":{\"epoch\":%d,\"alpha\":%.9g,\"regularization\":%d,\"lambda\":%.9g,"
                          "\"percentualtraining\":%d,\"lossfunctiontype\":%d,\"layers\":%d,\"neurons\":[",
                          network->epoch, (double)network->alpha, network->regularization, (double)network->lambda,
                          network->percentualtraining, network->lossfunctiontype, network->layers);
    for (const Layer* l = network->firstlayer; l != NULL && ok; l = l->nextlayer) {
        ok = FlatAppend(buf, cap, &len, l == network->firstlayer ? "%d" : ",%d", l->neurons);
    }
    ok = ok && FlatAppend(buf, cap, &len, "],\"activations\":[");
    for (const Layer* l = network->firstlayer; l != NULL && ok; l = l->nextlayer) {
        ok = FlatAppend(buf, cap, &len, l == network->firstlayer ? "%d" : ",%d", l->activationfunctiontype);
    }
    ok = ok && FlatAppend(buf, cap, &len, "],\"weights\":\"");
    int first = 1;
    for (const Layer* l = network->firstlayer; l != NULL && ok; l = l->nextlayer) {
        for (const Neuron* n = l->firstneuron; n != NULL && ok; n = n->nextneuron) {
            for (const Weight* w = n->firstweight; w != NULL && ok; w = w->nextweight) {
                ok = FlatAppend(buf, cap, &len, first ? "%.9g" : ",%.9g", (double)w->weight);
                first = 0;
            }
        }
    }
    ok = ok && FlatAppend(buf, cap, &len, "\",\"biases\":\"");
    first = 1;
    for (const Layer* l = network->firstlayer; l != NULL && ok; l = l->nextlayer) {
        for (const Neuron* n = l->firstneuron; n != NULL && ok; n = n->nextneuron) {
            ok = FlatAppend(buf, cap, &len, first ? "%.9g" : ",%.9g", (double)n->bias);
            first = 0;
        }
    }
    ok = ok && FlatAppend(buf, cap, &len, "\"}}");
    if (!ok) {
        free(buf);
        return NULL;
    }
    return buf;
}
