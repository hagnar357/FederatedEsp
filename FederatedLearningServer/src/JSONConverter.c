#include "../lib/JSONConverter.h"
#include "../lib/federatedlearning.h"
#include "../lib/configs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//FFEDERATED LEARNING TO JSON
//the "neuralnetwork" object shared by the global model, the teacher task and the teacher model
cJSON* NeuralNetworkToJSON(const NeuralNetwork* neuralnetwork) {
    cJSON* jsonNeuralNetwork = cJSON_CreateObject();
    if (!jsonNeuralNetwork) return NULL;

    cJSON_AddItemToObject(jsonNeuralNetwork, "epoch", cJSON_CreateNumber(neuralnetwork->epoch));
    cJSON_AddItemToObject(jsonNeuralNetwork, "alpha", cJSON_CreateNumber(neuralnetwork->alpha));
    cJSON_AddItemToObject(jsonNeuralNetwork, "regularization", cJSON_CreateNumber(neuralnetwork->regularization));
    cJSON_AddItemToObject(jsonNeuralNetwork, "lambda", cJSON_CreateNumber(neuralnetwork->lambda));
    cJSON_AddItemToObject(jsonNeuralNetwork, "percentualtraining", cJSON_CreateNumber(neuralnetwork->percentualtraining));
    cJSON_AddItemToObject(jsonNeuralNetwork, "lossfunctiontype", cJSON_CreateNumber(neuralnetwork->lossfunctiontype));
    cJSON_AddItemToObject(jsonNeuralNetwork, "layers", cJSON_CreateNumber(neuralnetwork->layers));

    cJSON* layersArray = cJSON_CreateArray();
    struct Layer* currentLayer = neuralnetwork->firstlayer;

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

            // Adiciona um array de pesos para cada neurônio
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
    return jsonNeuralNetwork;
}

//FFEDERATED LEARNING TO JSON
cJSON* FederatedLearningToJSON(FederatedLearning* federatedLearning) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddItemToObject(root, "globalmodelstatus", cJSON_CreateNumber(federatedLearning->globalmodelstatus));
    cJSON_AddItemToObject(root, "trainingscounter", cJSON_CreateNumber(federatedLearning->trainingscounter));
    if (federatedLearning->nodecontrol != NULL) {
        cJSON_AddItemToObject(root, "round", cJSON_CreateNumber(federatedLearning->nodecontrol->currentinteraction));
    }
    cJSON_AddItemToObject(root, "neuralnetwork", NeuralNetworkToJSON(federatedLearning->neuralnetwork));

    // Adicione outros campos da estrutura FederatedLearning conforme necessário

    return root;
}

//TEACHER (phase 1): the untrained teacher task, in the FederatedLearning format so the client
//parses it with JSONToFederatedLearning
cJSON* TeacherTaskToJSON() {
    NodeControl* nodecontrol = getFederatedLearningInstance()->nodecontrol;
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddNumberToObject(root, "globalmodelstatus", 0);
    cJSON_AddNumberToObject(root, "trainingscounter", 0);
    cJSON_AddStringToObject(root, "task", "teacher");
    cJSON_AddItemToObject(root, "neuralnetwork", NeuralNetworkToJSON(nodecontrol->teacherneuralnetwork));
    return root;
}

//TEACHER (phase 2): the trained teacher plus the distillation parameters used by the nodes
//(contract of GET /api/getteachermodel); NULL while no trained teacher exists
cJSON* TeacherModelToJSON() {
    NodeControl* nodecontrol = getFederatedLearningInstance()->nodecontrol;
    if (!nodecontrol->teachertrained) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddNumberToObject(root, "globalmodelstatus", 1);
    cJSON_AddNumberToObject(root, "trainingscounter", nodecontrol->teachertrainingscounter);
    cJSON* distillation = cJSON_AddObjectToObject(root, "distillation");
    cJSON_AddNumberToObject(distillation, "temperature", KD_TEMPERATURE);
    cJSON_AddNumberToObject(distillation, "alpha", KD_ALPHA);
    cJSON_AddItemToObject(root, "neuralnetwork", NeuralNetworkToJSON(nodecontrol->teacherneuralnetwork));
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
        freeFederatedLearningModel(federatedLearning);
        return NULL;
    }

    NeuralNetwork* neuralnetwork = (NeuralNetwork*)calloc(1, sizeof(NeuralNetwork));
    if (neuralnetwork == NULL) {
        freeFederatedLearningModel(federatedLearning);
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
        freeFederatedLearningModel(federatedLearning);
        return NULL;
    }

    return federatedLearning;
}
