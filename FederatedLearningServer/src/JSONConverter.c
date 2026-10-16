#include "../lib/JSONConverter.h"
#include "../lib/federatedlearning.h"
#include "../lib/configs.h"
#include <stdbool.h>
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

//FLAT FORM OF THE NETWORK
//the same network as NeuralNetworkToJSON, but the weights and biases travel as two comma separated strings
//("w,w,w,..."): cJSON creates one node per number of an array (~76 B/weight on a board), a string is one node,
//and a node reads it straight into its Weight list with strtof (JSONFlatToFederatedLearning). Numbers use %.9g,
//the shortest form that gives back the exact float. Order: layer by layer (the input layer has no weights),
//neuron by neuron; biases: one per neuron of every layer. Only fully connected networks can be sent this way.
static int AppendFloatList(char *out, size_t cap, size_t *len, double value) {
    int n = snprintf(out + *len, cap - *len, *len == 0 ? "%.9g" : ",%.9g", value);
    if (n < 0 || (size_t)n >= cap - *len) return 0;
    *len += (size_t)n;
    return 1;
}

cJSON* NeuralNetworkToFlatJSON(const NeuralNetwork* neuralnetwork) {
    size_t weightscount = 0, biasescount = 0;
    int layers = 0;
    for (const Layer* l = neuralnetwork->firstlayer; l != NULL; l = l->nextlayer) {
        layers++;
        for (const Neuron* n = l->firstneuron; n != NULL; n = n->nextneuron) {
            biasescount++;
            const Weight* w = n->firstweight;
            for (int k = 0; k < n->weights; k++, w = w->nextweight) {
                if (w == NULL) return NULL;
                weightscount++;
            }
            //fully connected: as many weights as neurons in the previous layer
            if (n->weights != (l->previouslayer != NULL ? l->previouslayer->neurons : 0)) return NULL;
        }
    }
    if (layers != neuralnetwork->layers) return NULL;

    // "-1.23456789e-05," is the longest number: 16 characters
    size_t wcap = weightscount * 16 + 1, bcap = biasescount * 16 + 1;
    char* weightsstr = (char*)malloc(wcap);
    char* biasesstr = (char*)malloc(bcap);
    cJSON* root = cJSON_CreateObject();
    cJSON* neurons = cJSON_CreateArray();
    cJSON* activations = cJSON_CreateArray();
    if (!weightsstr || !biasesstr || !root || !neurons || !activations) {
        free(weightsstr); free(biasesstr); cJSON_Delete(root); cJSON_Delete(neurons); cJSON_Delete(activations);
        return NULL;
    }
    weightsstr[0] = biasesstr[0] = '\0';
    size_t wlen = 0, blen = 0;
    int ok = 1;

    cJSON_AddNumberToObject(root, "epoch", neuralnetwork->epoch);
    cJSON_AddNumberToObject(root, "alpha", neuralnetwork->alpha);
    cJSON_AddNumberToObject(root, "regularization", neuralnetwork->regularization);
    cJSON_AddNumberToObject(root, "lambda", neuralnetwork->lambda);
    cJSON_AddNumberToObject(root, "percentualtraining", neuralnetwork->percentualtraining);
    cJSON_AddNumberToObject(root, "lossfunctiontype", neuralnetwork->lossfunctiontype);
    cJSON_AddNumberToObject(root, "layers", neuralnetwork->layers);

    for (const Layer* l = neuralnetwork->firstlayer; l != NULL && ok; l = l->nextlayer) {
        cJSON_AddItemToArray(neurons, cJSON_CreateNumber(l->neurons));
        cJSON_AddItemToArray(activations, cJSON_CreateNumber(l->activationfunctiontype));
        for (const Neuron* n = l->firstneuron; n != NULL && ok; n = n->nextneuron) {
            ok = AppendFloatList(biasesstr, bcap, &blen, n->bias);
            for (const Weight* w = n->firstweight; w != NULL && ok; w = w->nextweight) {
                ok = AppendFloatList(weightsstr, wcap, &wlen, w->weight);
            }
        }
    }
    cJSON_AddItemToObject(root, "neurons", neurons);
    cJSON_AddItemToObject(root, "activations", activations);
    if (ok) {
        cJSON_AddStringToObject(root, "weights", weightsstr);
        cJSON_AddStringToObject(root, "biases", biasesstr);
    }
    free(weightsstr);
    free(biasesstr);
    if (!ok) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
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

//the global model with the network in the flat form (GET /api/getglobalmodelflat); NULL if the network cannot be sent that way
cJSON* FederatedLearningToFlatJSON(FederatedLearning* federatedLearning) {
    cJSON* network = NeuralNetworkToFlatJSON(federatedLearning->neuralnetwork);
    if (network == NULL) return NULL;
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        cJSON_Delete(network);
        return NULL;
    }

    cJSON_AddStringToObject(root, "format", "flat");
    cJSON_AddNumberToObject(root, "globalmodelstatus", federatedLearning->globalmodelstatus);
    cJSON_AddNumberToObject(root, "trainingscounter", federatedLearning->trainingscounter);
    if (federatedLearning->nodecontrol != NULL) {
        cJSON_AddNumberToObject(root, "round", federatedLearning->nodecontrol->currentinteraction);
    }
    cJSON_AddItemToObject(root, "neuralnetwork", network);
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

//TEACHER (phase 2): the trained teacher plus the distillation parameters used by the nodes.
//full=true serializes the whole network (used for the on-disk teacher.json artifact); full=false
//truncates it to Encoder+Latent only, dropping Decoder+Reconstruction, which KD_MODE_LATENT students
//never read (see NeuralNetworkTraining in FederatedLearningZephyr/src/federatedlearning.c) -- this is
//the payload actually served over the wire, roughly halving it for the encoder-decoder architecture.
//NULL while no trained teacher exists.
static cJSON* BuildTeacherModelJSON(bool full, bool flat) {
    NodeControl* nodecontrol = getFederatedLearningInstance()->nodecontrol;
    if (!nodecontrol->teachertrained) return NULL;

    NeuralNetwork *teacher = nodecontrol->teacherneuralnetwork;
    int latentindex = nodecontrol->teacherlatentlayerindex;

    // temporarily cut the chain right after the latent layer so NeuralNetworkToJSON's walk (and the
    // "layers" count it serializes) only sees Input..Latent; restored before returning. Safe because
    // every caller of this function already holds FederatedLearningLock().
    Layer *cutlayer = NULL;
    Layer *savednext = NULL;
    int savedlayers = teacher->layers;
    if (!full && latentindex >= 0) {
        cutlayer = teacher->firstlayer;
        for (int i = 0; i < latentindex; i++) {
            cutlayer = cutlayer->nextlayer;
        }
        savednext = cutlayer->nextlayer;
        cutlayer->nextlayer = NULL;
        teacher->layers = latentindex + 1;
    }

    cJSON* root = cJSON_CreateObject();
    if (root) {
        cJSON_AddNumberToObject(root, "globalmodelstatus", 1);
        cJSON_AddNumberToObject(root, "trainingscounter", nodecontrol->teachertrainingscounter);
        cJSON* distillation = cJSON_AddObjectToObject(root, "distillation");
        cJSON_AddStringToObject(distillation, "mode", latentindex >= 0 ? "latent" : "logit");
        cJSON_AddNumberToObject(distillation, "temperature", KD_TEMPERATURE); // kept, unused in "latent" mode
        cJSON_AddNumberToObject(distillation, "alpha", KD_ALPHA);
        if (latentindex >= 0) {
            cJSON_AddNumberToObject(distillation, "studentlayerindex", KD_STUDENT_HINT_LAYER);
        }
        cJSON* network = flat ? NeuralNetworkToFlatJSON(teacher) : NeuralNetworkToJSON(teacher);
        if (network == NULL) {
            cJSON_Delete(root);
            root = NULL;
        } else {
            if (flat) cJSON_AddStringToObject(root, "format", "flat");
            cJSON_AddItemToObject(root, "neuralnetwork", network);
        }
    }

    if (cutlayer != NULL) {
        cutlayer->nextlayer = savednext;
    }
    teacher->layers = savedlayers;

    return root;
}

cJSON* TeacherModelToJSON() {
    return BuildTeacherModelJSON(true, false);
}

//contract of GET /api/getteachermodel: the latent layer is always the last layer of the received
//network, so nodes no longer need a transmitted "latentlayerindex" to find it (same as KD_MODE_LOGIT
//already does with its output layer)
cJSON* TeacherModelForNodesToJSON() {
    return BuildTeacherModelJSON(false, false);
}

//same payload as TeacherModelForNodesToJSON with the network in the flat form (GET /api/getteachermodelflat)
cJSON* TeacherModelForNodesFlatToJSON() {
    return BuildTeacherModelJSON(false, true);
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


//FLAT FORM TO FEDERATED LEARNING
//the same parser as the nodes' JSONFlatToFederatedLearning (FederatedLearningZephyr/src/JSONConverter.c), for the
//models the nodes upload in the flat form: the weights and biases arrive as comma separated strings

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
        //link first so freeFederatedLearningModel releases it on error
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

    //both lists must be used up exactly: a shorter or longer list means the sender and this server disagree
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
        freeFederatedLearningModel(federatedLearning);
        return NULL;
    }

    NeuralNetwork* neuralnetwork = (NeuralNetwork*)calloc(1, sizeof(NeuralNetwork));
    if (neuralnetwork == NULL) {
        freeFederatedLearningModel(federatedLearning);
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
        freeFederatedLearningModel(federatedLearning);
        return NULL;
    }

    return federatedLearning;
}
