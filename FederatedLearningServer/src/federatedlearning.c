#include "../lib/federatedlearning.h"
#include "../lib/JSONConverter.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "configs.h"

static pthread_mutex_t singletonMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t flStateMutex = PTHREAD_MUTEX_INITIALIZER;

void FederatedLearningLock(){
  pthread_mutex_lock(&flStateMutex);
}

void FederatedLearningUnlock(){
  pthread_mutex_unlock(&flStateMutex);
}

//////////////////////////////////////////////////PRINT//////////////////////////////////////////////////

void PrintNeuralNeuralNetwork(NeuralNetwork * neuralnetwork) {

  printf("epoch: %d\n",neuralnetwork->epoch);
  printf("alpha: %.10f\n",neuralnetwork->alpha);
  printf("regularization: %d\n",neuralnetwork->regularization);
  printf("lambda: %.10f\n",neuralnetwork->lambda);
  printf("percentualtraining: %d\n",neuralnetwork->percentualtraining);
  printf("lossfunctiontype: %d\n",neuralnetwork->lossfunctiontype);
  printf("layers: %d\n",neuralnetwork->layers);

  int i = 0, j = 0, k = 0;

  Layer * currentlayer = neuralnetwork -> firstlayer;
  while (currentlayer != NULL) {

    i++;
    printf("activationfunctiontype: %d - layer %d - neurons: %d\n",currentlayer->activationfunctiontype,i,currentlayer -> neurons);
    Neuron * currentneuron = currentlayer -> firstneuron;

    while (currentneuron != NULL) {

      j++;
      printf("neurontype %s - neuron %d - weights: %d\n",currentneuron->neurontype , j,currentneuron -> weights);
      printf("bias: %.10f\n",currentneuron->bias);

      Weight * currentweight = currentneuron -> firstweight;

      while (currentweight != NULL) {

        k++;
        printf("weight: %d - value: %.10f ", k, currentweight -> weight);

        currentweight = currentweight -> nextweight;
      }
      printf("\n");
      k = 0;
      currentneuron = currentneuron -> nextneuron;
    }
    j = 0;
    currentlayer = currentlayer -> nextlayer;

  }
  i = 0;
  printf("\n");

}

//////////////////////////////////////////////////MEMORY//////////////////////////////////////////////////

Weight *CopyWeight(Weight *currentweight){
  
  Weight *newweight= (Weight *)malloc(sizeof(Weight));
  newweight->weight = currentweight->weight;
  return newweight;
}

Neuron *CopyNeuron(Neuron *neuronsource){

  Neuron *newnueron = (Neuron *)malloc(sizeof(Neuron));

  strcpy(newnueron->neurontype, neuronsource->neurontype);
  newnueron ->weights=neuronsource->weights;
  newnueron-> activationfunctionvalue=neuronsource->activationfunctionvalue;
  newnueron ->bias=neuronsource->bias;
  newnueron->firstweight = NULL;
  newnueron->lastweight = NULL;

  Weight *currentweight = neuronsource->firstweight;

  while (currentweight != NULL){

    Weight * newweight = CopyWeight(currentweight);

    if(newnueron->firstweight==NULL){
      newweight->previousweight=NULL;
      newweight->nextweight=NULL;
      newnueron->firstweight=newweight;
      newnueron->lastweight=newweight;
      currentweight = currentweight->nextweight;
      continue;
    }

    newweight->nextweight=NULL;
    newweight->previousweight= newnueron->lastweight;
    newnueron->lastweight->nextweight = newweight;
    newnueron->lastweight= newweight;

    currentweight = currentweight->nextweight;
  }
  return newnueron;
}

Layer *CopyLayer(Layer *layersource){

  Layer *newlayer = (Layer *)malloc(sizeof(Layer));
  
  newlayer->activationfunctiontype = layersource->activationfunctiontype;
  newlayer->neurons = layersource->neurons;
  newlayer->firstneuron = NULL;
  newlayer->lastneuron = newlayer->firstneuron;

  Neuron *currentneuron = layersource->firstneuron;

  while (currentneuron != NULL){
    Neuron *newneuron = CopyNeuron(currentneuron);

    if(newlayer->firstneuron==NULL){
      newneuron->previousneuron=NULL;
      newneuron->nextneuron=NULL;
      newlayer->firstneuron=newneuron;
      newlayer->lastneuron=newneuron;
      currentneuron = currentneuron->nextneuron;
      continue;
    }

    newneuron->nextneuron=NULL;
    newneuron->previousneuron= newlayer->lastneuron;
    newlayer->lastneuron->nextneuron = newneuron;
    newlayer->lastneuron= newneuron;

    currentneuron = currentneuron->nextneuron;
  }

  return newlayer;
}

NeuralNetwork *CopyNeuralNetwork(NeuralNetwork* neuralnetworksource){

  NeuralNetwork * newneuralnetwork= (NeuralNetwork *)malloc(sizeof(NeuralNetwork));

  newneuralnetwork->epoch=neuralnetworksource->epoch;
  newneuralnetwork->alpha=neuralnetworksource->alpha;
  newneuralnetwork->regularization=neuralnetworksource->regularization;
  newneuralnetwork->lambda=neuralnetworksource->lambda;
  newneuralnetwork->percentualtraining=neuralnetworksource->percentualtraining;
  newneuralnetwork->lossfunctiontype=neuralnetworksource->lossfunctiontype;
  newneuralnetwork->layers=neuralnetworksource->layers;
  newneuralnetwork->firstlayer = NULL;
  newneuralnetwork->lastlayer = newneuralnetwork->firstlayer;

  Layer * currentLayer = neuralnetworksource->firstlayer;

  while (currentLayer != NULL){
    Layer *newlayer = CopyLayer(currentLayer);

    if(newneuralnetwork->firstlayer==NULL){
      newlayer->previouslayer=NULL;
      newlayer->nextlayer=NULL;
      newneuralnetwork->firstlayer=newlayer;
      newneuralnetwork->lastlayer=newlayer;
      currentLayer = currentLayer->nextlayer;
      continue;
    }

    newlayer->nextlayer=NULL;
    newlayer->previouslayer= newneuralnetwork->lastlayer;
    newneuralnetwork->lastlayer->nextlayer = newlayer;
    newneuralnetwork->lastlayer= newlayer;

    currentLayer = currentLayer->nextlayer;
  }

  return newneuralnetwork;
}

void freeWeight(Weight *weight) {
    if(weight!=NULL){
        free(weight);        
    }
}

void freeNeuron(Neuron *neuron) {
    if(neuron!=NULL){
        free(neuron);        
    }
}

void freeLayer(Layer *layer) {
    if(layer!=NULL){
        free(layer);        
    }
}

void freeNeuralNetwork(NeuralNetwork *neuralnetwork) {
      //printf("teste 0");
    if(neuralnetwork==NULL){
        return;
    }

    Layer *currentlayer = neuralnetwork->firstlayer;

    while (currentlayer != NULL) {
      //printf("teste 1");
        // Libere a memória dos neurônios na camada atual
        Neuron *currentneuron = currentlayer->firstneuron;
        while (currentneuron != NULL) {
            //printf("teste 2");

            // Libere a memória dos pesos associados ao neurônio atual
            Weight *currentweight = currentneuron->firstweight;
            while (currentweight != NULL) {
                //printf("teste 3");

                Weight *nextweight = currentweight->nextweight;
                freeWeight(currentweight);
                currentweight = nextweight;
            }

            // Libere a memória do neurônio atual
            Neuron *nextneuron = currentneuron->nextneuron;
            freeNeuron(currentneuron);
            currentneuron = nextneuron;
        }

        // Libere a memória da camada atual
        Layer *nextlayer = currentlayer->nextlayer;
        freeLayer(currentlayer);
        currentlayer = nextlayer;
    }

    free(neuralnetwork);
}

void freeFederatedLearningModel(FederatedLearning *federatedlearning) {
    if(federatedlearning==NULL){
        return;
    }
    freeNeuralNetwork(federatedlearning->neuralnetwork);
    free(federatedlearning);
}

//////////////////////////////////////////////////DEEPLEARNINGMATH//////////////////////////////////////////////////

float Perceptron(float Z){
    if(Z>0)
    {
      return 1;
    }
    else{
     return 0; 
    }
}

float ReLU(float Z){
  if(Z>0){

    return Z;
  }else{
    return 0;
  }
}

float Sigmoid(float Z) {
  return 1.0 / (1.0 + exp(-Z));
}

//numerically stable softmax: the neurons hold Z in activationfunctionvalue when called
void SoftMaxLayer(Layer *layer){
  Neuron *currentneuron = layer->firstneuron;
  float maxZ = currentneuron->activationfunctionvalue;
  for (currentneuron = layer->firstneuron; currentneuron != NULL; currentneuron = currentneuron->nextneuron) {
    if (currentneuron->activationfunctionvalue > maxZ) {
      maxZ = currentneuron->activationfunctionvalue;
    }
  }

  float softmaxsum = 0;
  for (currentneuron = layer->firstneuron; currentneuron != NULL; currentneuron = currentneuron->nextneuron) {
    currentneuron->activationfunctionvalue = exp(currentneuron->activationfunctionvalue - maxZ);
    softmaxsum += currentneuron->activationfunctionvalue;
  }

  for (currentneuron = layer->firstneuron; currentneuron != NULL; currentneuron = currentneuron->nextneuron) {
    currentneuron->activationfunctionvalue /= softmaxsum;
  }
}

float CategoricalCrossEntropy(float * label, float * output, int labelsize) {
  float sum = 0;
  for (int i = 0; i < labelsize; i++) {
    sum += label[i] * log(fmaxf(output[i], 1e-7f));
  }
  return -sum;
}

//////////////////////////////////////////////////DEEPLEARNINGNEURALNETWORK//////////////////////////////////////////////////

float RidgeRegressionCalculation(NeuralNetwork *neuralnetwork,float lambda){

  float sum=0;

  Layer *currentlayer;
  Neuron *currentneuron;
  Weight *currentweight;

  currentlayer = neuralnetwork->firstlayer; 
  while(currentlayer != NULL){
    currentneuron = currentlayer->firstneuron;

    while (currentneuron!=NULL){
      currentweight = currentneuron->firstweight;

      while (currentweight!=NULL){
        sum += currentweight->weight*currentweight->weight;
        currentweight = currentweight->nextweight;
      }
      currentneuron= currentneuron->nextneuron;
    }
      currentlayer = currentlayer->nextlayer;
  }
  return sum*lambda;
}

float LassoRegressionCalculation(NeuralNetwork *neuralnetwork,float lambda){
  
  float sum=0;

  Layer *currentlayer;
  Neuron *currentneuron;
  Weight *currentweight;

  currentlayer = neuralnetwork->firstlayer; 
  while(currentlayer != NULL){
    currentneuron = currentlayer->firstneuron;

    while (currentneuron!=NULL){
      currentweight = currentneuron->firstweight;

      while (currentweight!=NULL){

        if (currentweight->weight>=0){
          sum += currentweight->weight;
        }
        else{
          sum += -currentweight->weight;
        }

        currentweight = currentweight->nextweight;
      }
      currentneuron= currentneuron->nextneuron;
    }
      currentlayer = currentlayer->nextlayer;
  }
  return sum*lambda;
}

float LossFunctionCalculation(NeuralNetwork * neuralnetwork, float *labelvector, int regularization,float lambda){  
  
  float outputdata[neuralnetwork -> lastlayer->neurons];
  Neuron *currentneuron = neuralnetwork -> lastlayer -> firstneuron;
  //getting the neurons output value for loss function
  for (int i = 0; i < neuralnetwork->lastlayer->neurons; i++) {
    outputdata[i] = currentneuron -> activationfunctionvalue;
    currentneuron = currentneuron -> nextneuron;
    //printf("label %f output %f \n",labelvector[i],outputdata[i]);
  }

  switch (neuralnetwork->lossfunctiontype){
  case MINIMAL_MEAN_SQUARE:
    break;
  
  case CATEGORICAL_CROSS_ENTROPY:

    switch (regularization){
      
      case 0:
        return  CategoricalCrossEntropy(labelvector, outputdata, neuralnetwork->lastlayer->neurons) ;
        break;

      case 1:
        return  CategoricalCrossEntropy(labelvector, outputdata, neuralnetwork->lastlayer->neurons) + LassoRegressionCalculation(neuralnetwork,lambda);
        break;

      case 2:
        return  CategoricalCrossEntropy(labelvector, outputdata, neuralnetwork->lastlayer->neurons) + RidgeRegressionCalculation(neuralnetwork,lambda);
        break;
      default:
        break;
    }
    break;
  default:
    return 0;
    break;

  }

  return 0;
  
}

float ActivationFunctionCalculaton(float Z, int activationfunctiontype){

//printf("activaiton function type: %d",activationfunctiontype);

switch (activationfunctiontype){
  case PERCEPTRON:
    return Perceptron(Z);
  case RELU:
    return ReLU(Z);
  case SIGMOID:
    return Sigmoid(Z);
  case SOFTMAX:
    //keeps Z, the whole layer is normalized by SoftMaxLayer
    return Z;
  default:
    return 0;
  }

}

void FeedFoward(NeuralNetwork * neuralnetwork){

  Layer *currentlayer = neuralnetwork->firstlayer->nextlayer;
  Neuron *currentneuron;
  Weight *currentweight;
  
  //printf("FEEDFOWARD\n");

  for (int layer = 1; layer < neuralnetwork -> layers; layer++) {

    //printf("LAYER %d\n",layer);
            
    Neuron *previousneuron = currentlayer->previouslayer->firstneuron;
    currentneuron = currentlayer -> firstneuron;
    float Z = 0;
          
    //printf("neurons %d\n",currentlayer->neurons);

      for (int i = 0; i < currentlayer -> neurons; i++) {
        currentweight= currentneuron->firstweight;
        //printf("wieghts %d \nB %f - ",currentneuron->weights,currentneuron->bias);
        Z+= currentneuron->bias;
        for (int j = 0;  j< currentneuron -> weights; j++) {
          Z += currentweight->weight * previousneuron->activationfunctionvalue;
          //printf("W %f IN %f + ",currentweight->weight,previousneuron->activationfunctionvalue);
          currentweight =currentweight->nextweight;
          previousneuron = previousneuron ->nextneuron;
        }
        currentneuron -> activationfunctionvalue =  ActivationFunctionCalculaton(Z,currentlayer->activationfunctiontype);
        //printf("Z %f ATIVACAO %f\n",Z,currentneuron -> activationfunctionvalue);
        Z = 0;
        currentneuron = currentneuron -> nextneuron;
        previousneuron = currentlayer->previouslayer->firstneuron;
      }
    if (currentlayer->activationfunctiontype == SOFTMAX) {
      SoftMaxLayer(currentlayer);
    }
    currentlayer = currentlayer -> nextlayer;
    }
}

float WeightValue(int weightvalue){
  switch (weightvalue){
    case 0:
      return 0;
      break;
    case 1:
      return (float)(rand() % 100) / 100.0;
      break;
    case 2:
      return 0.5;
      break;
    default:
      break;
  }
  return 0;
}

void InitializeNeuralNetWork(NeuralNetwork * neuralnetwork,int layers,LayerConfig *layersconfig, int lossfunctiontype, int weightvalue) {

  LayerConfig *currrentlayerconfig = layersconfig->first;
  Layer *currrentlayer;

  //Inicialize a semente do gerador de números aleatórios
  srand((unsigned) time(NULL));

  neuralnetwork->layers = layers;
  neuralnetwork->lossfunctiontype = lossfunctiontype;
  neuralnetwork -> firstlayer = NULL;
  neuralnetwork -> lastlayer = NULL;

  currrentlayerconfig = layersconfig->first;
 
 for (int i = 0; i < neuralnetwork -> layers; i++) {

    Layer * newLayer = (Layer * ) malloc(sizeof(Layer));

    if (newLayer == NULL) {
      fprintf(stderr, "Alocation memory Failure\n");
      exit(1);
    }

    newLayer -> neurons = currrentlayerconfig->neurons;
    newLayer -> activationfunctiontype = currrentlayerconfig->activationfunctiontype;
    newLayer -> nextlayer = NULL;
    newLayer -> previouslayer = newLayer -> nextlayer;
    newLayer -> firstneuron = NULL;
    newLayer -> lastneuron = newLayer -> firstneuron;

    if (neuralnetwork -> lastlayer == NULL) {
      neuralnetwork -> firstlayer = newLayer;
    } else {
      neuralnetwork -> lastlayer -> nextlayer = newLayer;
      newLayer -> previouslayer = neuralnetwork -> lastlayer;
    }

    neuralnetwork -> lastlayer = newLayer;
    
    currrentlayer = newLayer;
    
    for (int j = 0; j < newLayer -> neurons; j++) {

      Neuron * newneuron = (Neuron * ) malloc(sizeof(Neuron));

      if (newneuron == NULL) {
        fprintf(stderr, "Alocation memory Failure\n");
        exit(1);
      }

      if (i == 0) {
        strcpy(newneuron->neurontype, "INPUT");
        newneuron -> weights = 0;
      }
      else if(i==neuralnetwork -> layers - 1){
        strcpy(newneuron->neurontype, "OUTPUT");
        newneuron -> weights = currrentlayer->previouslayer->neurons;
      } 
      else {
        strcpy(newneuron->neurontype, "HIDDEN");
        newneuron -> weights = currrentlayer->previouslayer->neurons;
      }

      //newneuron -> bias = (float)(rand() % 100) / 100.0; 
      newneuron -> bias = 0.0;

      newneuron -> nextneuron = NULL;
      newneuron -> previousneuron = newneuron -> nextneuron;
      newneuron -> firstweight = NULL;
      newneuron -> lastweight = newneuron -> firstweight;
      newneuron -> nextneuron = NULL;
      newneuron -> previousneuron = newneuron -> nextneuron;

      if (neuralnetwork -> lastlayer -> lastneuron == NULL) {

        neuralnetwork -> lastlayer -> firstneuron = newneuron;
      } else {

        neuralnetwork -> lastlayer -> lastneuron -> nextneuron = newneuron;
        newneuron -> previousneuron = neuralnetwork -> lastlayer -> lastneuron;
      }

      neuralnetwork -> lastlayer -> lastneuron = newneuron;

      for (int k = 0; k < newneuron -> weights; k++) {

        Weight * newweight = (Weight * ) malloc(sizeof(Weight));

        if (newweight == NULL) {
          fprintf(stderr, "Alocation memory Failure\n");
          exit(1);
        }
        
        newweight -> weight = WeightValue(weightvalue); 
        newweight -> nextweight = NULL;
        newweight -> previousweight = newweight -> nextweight;

        if (newneuron -> lastweight == NULL) {
          neuralnetwork -> lastlayer -> lastneuron -> firstweight = newweight;
        } else {
          neuralnetwork -> lastlayer -> lastneuron -> lastweight -> nextweight = newweight;
          newweight -> previousweight = neuralnetwork -> lastlayer -> lastneuron -> lastweight;
        }
        newneuron -> lastweight = newweight;
      }
    }

    currrentlayerconfig = currrentlayerconfig->next;
  }
}

//////////////////////////////////////////////////FEDERATEDLEARNING//////////////////////////////////////////////////

FederatedLearning *getFederatedLearningInstance() {

    static FederatedLearning instance;
    pthread_mutex_lock(&singletonMutex);
    if (instance.neuralnetwork == NULL) {
        instance.neuralnetwork = (NeuralNetwork *)malloc(sizeof(NeuralNetwork));
        instance.globalmodelstatus = 0;
        instance.trainingscounter = 0;

        instance.nodecontrol = (NodeControl *)malloc(sizeof(NodeControl));
        instance.nodecontrol->firstclientnode=NULL;
        instance.nodecontrol->lastclientnode=instance.nodecontrol->firstclientnode;
        instance.nodecontrol->currentinteraction=0;
        instance.nodecontrol->phase = PHASE_REGISTRATION;
        instance.nodecontrol->teacherstarttime = 0;
        instance.nodecontrol->teachertrained = 0;
        instance.nodecontrol->teachertrainingscounter = 0;
        instance.nodecontrol->teacherneuralnetwork = (NeuralNetwork *)malloc(sizeof(NeuralNetwork));
        instance.nodecontrol->neuralnetwork = (NeuralNetwork *)malloc(sizeof(NeuralNetwork));
    }
    pthread_mutex_unlock(&singletonMutex);  
    return &instance;
}

void setFederatedLearningGlobalModel() {

  FederatedLearning *federatedLearningInstance = getFederatedLearningInstance();
  LayerConfig *layerconfig0 = malloc(sizeof(LayerConfig));
  LayerConfig *layerconfig1 = malloc(sizeof(LayerConfig));
  LayerConfig *layerconfig2 = malloc(sizeof(LayerConfig));
  LayerConfig *layerconfig3 = malloc(sizeof(LayerConfig));

  layerconfig0->first =layerconfig0;
  layerconfig0->neurons= 4;
  layerconfig0->activationfunctiontype = 0;
  layerconfig0->next = layerconfig1;

  layerconfig1->neurons= 3;
  layerconfig1->activationfunctiontype = RELU;
  layerconfig1->next = layerconfig2;

  layerconfig2->neurons= 3;
  layerconfig2->activationfunctiontype = RELU;
  layerconfig2->next = layerconfig3;

  layerconfig3->neurons= 3;
  layerconfig3->activationfunctiontype = SOFTMAX;
  layerconfig3->next = NULL;

  InitializeNeuralNetWork(federatedLearningInstance->neuralnetwork, 4 ,layerconfig0, CATEGORICAL_CROSS_ENTROPY,WEIGHT_VALUE_RANDOM);
  
  federatedLearningInstance->neuralnetwork->percentualtraining = TRAINING_SAMPLES;
  federatedLearningInstance->neuralnetwork->epoch = EPOCHS;
  federatedLearningInstance->neuralnetwork->alpha=ALPHA;
  federatedLearningInstance->neuralnetwork->regularization=L2;
  federatedLearningInstance->neuralnetwork->lambda =LAMBDA;

  federatedLearningInstance->nodecontrol->interactioncycle=ITERATIONS;
  federatedLearningInstance->nodecontrol->clientnodes=CLIENTS_NUM;
  federatedLearningInstance->nodecontrol->clientnodesregistered=0;
  federatedLearningInstance->nodecontrol->modelsreceived=0;
  federatedLearningInstance->nodecontrol->roundstarttime=time(NULL);
  //the aggregation accumulator must start at zero
  InitializeNeuralNetWork(federatedLearningInstance->nodecontrol->neuralnetwork, 4 ,layerconfig0, CATEGORICAL_CROSS_ENTROPY,WEIGHT_VALUE_ZERO);
  federatedLearningInstance->nodecontrol->neuralnetwork->alpha=ALPHA;
  federatedLearningInstance->nodecontrol->neuralnetwork->epoch = EPOCHS;
  federatedLearningInstance->nodecontrol->neuralnetwork->regularization=L2;
  federatedLearningInstance->nodecontrol->neuralnetwork->lambda =LAMBDA;
  federatedLearningInstance->nodecontrol->neuralnetwork->percentualtraining = TRAINING_SAMPLES;

  federatedLearningInstance->globalmodelstatus=0;
  federatedLearningInstance->nodecontrol->phase = PHASE_REGISTRATION;

  //teacher task (phase 1): same inputs/outputs as the global model, TEACHER_HIDDEN_LAYERS hidden layers
  int teacherlayers = 2 + TEACHER_HIDDEN_LAYERS;
  LayerConfig *teacherconfig = calloc(teacherlayers, sizeof(LayerConfig));
  for (int i = 0; i < teacherlayers; i++) {
    teacherconfig[i].first = teacherconfig;
    teacherconfig[i].next = (i + 1 < teacherlayers) ? &teacherconfig[i + 1] : NULL;
    if (i == 0) {
      teacherconfig[i].neurons = layerconfig0->neurons;
      teacherconfig[i].activationfunctiontype = 0;
    } else if (i == teacherlayers - 1) {
      teacherconfig[i].neurons = layerconfig3->neurons;
      teacherconfig[i].activationfunctiontype = SOFTMAX;
    } else {
      teacherconfig[i].neurons = TEACHER_HIDDEN_NEURONS;
      teacherconfig[i].activationfunctiontype = RELU;
    }
  }
  NeuralNetwork *teacher = federatedLearningInstance->nodecontrol->teacherneuralnetwork;
  InitializeNeuralNetWork(teacher, teacherlayers, teacherconfig, CATEGORICAL_CROSS_ENTROPY, WEIGHT_VALUE_RANDOM);
  teacher->epoch = TEACHER_EPOCHS;
  teacher->alpha = ALPHA;
  teacher->regularization = L2;
  teacher->lambda = LAMBDA;
  teacher->percentualtraining = TRAINING_SAMPLES;
  free(teacherconfig);

  free(layerconfig0);
  free(layerconfig1);
  free(layerconfig2);
  free(layerconfig3);

  printf("Neural Network Compiled!\n");

}

void FederatedAveraging(NeuralNetwork *globalneuralnetwork, int currenttrainingcounter){

  Layer * currentgloballayer = globalneuralnetwork -> firstlayer;
  while (currentgloballayer != NULL) {
    Neuron * currentglobalneuron = currentgloballayer -> firstneuron;
    while (currentglobalneuron != NULL) {
      //printf("bias:%.5f / %d = %.5f \n",currentglobalneuron->bias,currenttrainingcounter,currentglobalneuron->bias / currenttrainingcounter);
      currentglobalneuron->bias = currentglobalneuron->bias / currenttrainingcounter;
      Weight * currentglobalweight = currentglobalneuron -> firstweight;
      while (currentglobalweight != NULL) {
        //printf("weight:%.5f / %d = %.5f \n",currentglobalweight->weight,currenttrainingcounter,currentglobalweight->weight/currenttrainingcounter);
        currentglobalweight->weight = currentglobalweight->weight/currenttrainingcounter;
        currentglobalweight = currentglobalweight -> nextweight;
      }
      currentglobalneuron = currentglobalneuron -> nextneuron;
    }
    currentgloballayer = currentgloballayer -> nextlayer;
  }
}

void FederatedAveragingSum(NeuralNetwork *globalneuralnetwork, NeuralNetwork *clientneuralnetwork, int clienttrainingcounter){

  Layer * currentgloballayer = globalneuralnetwork -> firstlayer;
  Layer * currentclientlayer = clientneuralnetwork -> firstlayer;

  while (currentgloballayer != NULL) {

    Neuron * currentglobalneuron = currentgloballayer -> firstneuron;
    Neuron * currentclientneuron = currentclientlayer -> firstneuron;

    while (currentglobalneuron != NULL) {
      //printf("bias:%.5f * %d = %.5f\n",currentclientneuron->bias,clienttrainingcounter,currentclientneuron->bias*clienttrainingcounter);
      currentglobalneuron->bias += currentclientneuron->bias*clienttrainingcounter;
      Weight * currentglobalweight = currentglobalneuron -> firstweight;
      Weight * currentclientweight = currentclientneuron -> firstweight;

      while (currentglobalweight != NULL) {

        //printf("weight:%.5f * %d = %.5f\n",currentclientweight->weight,clienttrainingcounter,currentclientweight->weight*clienttrainingcounter);

        currentglobalweight->weight += currentclientweight->weight*clienttrainingcounter;
        currentglobalweight = currentglobalweight -> nextweight;
        currentclientweight = currentclientweight-> nextweight;
      }
      currentglobalneuron = currentglobalneuron -> nextneuron;
      currentclientneuron = currentclientneuron -> nextneuron;
    }
    currentgloballayer = currentgloballayer -> nextlayer;
    currentclientlayer = currentclientlayer -> nextlayer;
  }
}

void SetNeuralNetworkZeroWeightValue(NeuralNetwork *globalneuralnetwork){
  Layer * currentgloballayer = globalneuralnetwork -> firstlayer;
  while (currentgloballayer != NULL) {
    Neuron * currentglobalneuron = currentgloballayer -> firstneuron;
    while (currentglobalneuron != NULL) {
      currentglobalneuron->bias = 0;
      Weight * currentglobalweight = currentglobalneuron -> firstweight;
      while (currentglobalweight != NULL) {
        currentglobalweight->weight = 0;
        currentglobalweight = currentglobalweight -> nextweight;
      }
      currentglobalneuron = currentglobalneuron -> nextneuron;
    }
    currentgloballayer = currentgloballayer -> nextlayer;
  }
}

//////////////////////////////////////////////////RUNOUTPUT//////////////////////////////////////////////////

//output directories of the current run (modelos/<run id> and resultados/<run id>)
static char runModelsDir[128] = "modelos";
static char runResultsDir[128] = "resultados";

static int ValidRunId(const char *runid){
  size_t len = strlen(runid);
  if(len == 0 || len > 63){
    return 0;
  }
  for(size_t i = 0; i < len; i++){
    char c = runid[i];
    if(!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')){
      return 0;
    }
  }
  return 1;
}

static int MakeDirectory(const char *path){
  if(mkdir(path, 0755) == 0 || errno == EEXIST){
    return 0;
  }
  perror(path);
  return -1;
}

//creates the run subfolders; the run id comes from RUN_ID or from the current date and time
//an existing folder is never reused, so consecutive runs do not overwrite previous results
int InitRunOutput(){

  char baseid[64];
  const char *envid = getenv("RUN_ID");

  if(envid != NULL && ValidRunId(envid)){
    snprintf(baseid, sizeof(baseid), "%s", envid);
  }else{
    if(envid != NULL){
      printf("Invalid RUN_ID '%s', using a timestamp\n", envid);
    }
    time_t now = time(NULL);
    strftime(baseid, sizeof(baseid), "run_%Y%m%d_%H%M%S", localtime(&now));
  }

  if(MakeDirectory("modelos") != 0 || MakeDirectory("resultados") != 0){
    return -1;
  }

  char runid[80];
  snprintf(runid, sizeof(runid), "%s", baseid);

  for(int suffix = 1; suffix <= 100; suffix++){
    snprintf(runModelsDir, sizeof(runModelsDir), "modelos/%s", runid);
    snprintf(runResultsDir, sizeof(runResultsDir), "resultados/%s", runid);

    struct stat info;
    if(stat(runModelsDir, &info) != 0 && stat(runResultsDir, &info) != 0){
      if(MakeDirectory(runModelsDir) != 0 || MakeDirectory(runResultsDir) != 0){
        return -1;
      }
      if(strcmp(runid, baseid) != 0){
        printf("Run id %s already used, using %s\n", baseid, runid);
      }
      printf("Run output: %s | %s\n", runModelsDir, runResultsDir);
      return 0;
    }

    snprintf(runid, sizeof(runid), "%s_%d", baseid, suffix);
  }

  fprintf(stderr, "Could not create a new output folder for run %s\n", baseid);
  return -1;
}

void SaveModel(int interaction){

  FederatedLearning *instance = getFederatedLearningInstance();

  cJSON *json = FederatedLearningToJSON(instance);
  char *jsonstring = cJSON_Print(json);

  char filename[256];
  snprintf(filename, sizeof(filename), "%s/modelo_%d.json", runModelsDir, interaction);

  FILE *modelfile = fopen(filename, "w");
  if (modelfile != NULL) {
    fprintf(modelfile, "%s", jsonstring);
    fclose(modelfile);
    printf("Model saved: %s\n", filename);
  } else {
    perror("Erro ao salvar o modelo");
  }

  cJSON_Delete(json);
  free(jsonstring);
}

//the trained teacher is saved once, when it arrives, as <run>/teacher.json
static void SaveTeacherModel(){

  cJSON *json = TeacherModelToJSON();
  if (json == NULL) {
    return;
  }
  char *jsonstring = cJSON_Print(json);

  char filename[256];
  snprintf(filename, sizeof(filename), "%s/teacher.json", runModelsDir);

  FILE *modelfile = fopen(filename, "w");
  if (modelfile != NULL) {
    fprintf(modelfile, "%s", jsonstring);
    fclose(modelfile);
    printf("Teacher saved: %s\n", filename);
  } else {
    perror("Erro ao salvar o teacher");
  }

  cJSON_Delete(json);
  free(jsonstring);
}

//check if the client model has exactly the same structure of the global model
static int ValidateTopology(NeuralNetwork *reference, NeuralNetwork *client){

  if(reference==NULL || client==NULL || reference->layers!=client->layers){
    return 0;
  }

  Layer *referencelayer = reference->firstlayer;
  Layer *clientlayer = client->firstlayer;

  while(referencelayer!=NULL){
    if(clientlayer==NULL || referencelayer->neurons!=clientlayer->neurons){
      return 0;
    }

    Neuron *referenceneuron = referencelayer->firstneuron;
    Neuron *clientneuron = clientlayer->firstneuron;

    while(referenceneuron!=NULL){
      if(clientneuron==NULL || referenceneuron->weights!=clientneuron->weights){
        return 0;
      }

      Weight *referenceweight = referenceneuron->firstweight;
      Weight *clientweight = clientneuron->firstweight;
      while(referenceweight!=NULL){
        if(clientweight==NULL){
          return 0;
        }
        referenceweight = referenceweight->nextweight;
        clientweight = clientweight->nextweight;
      }
      if(clientweight!=NULL){
        return 0;
      }

      referenceneuron = referenceneuron->nextneuron;
      clientneuron = clientneuron->nextneuron;
    }
    if(clientneuron!=NULL){
      return 0;
    }

    referencelayer = referencelayer->nextlayer;
    clientlayer = clientlayer->nextlayer;
  }

  return clientlayer==NULL;
}

//close the current round: average the received models and start the next round
//the caller must hold FederatedLearningLock
static void FinalizeRound(){

  FederatedLearning * federatedlearninginstance = getFederatedLearningInstance();
  NodeControl *nodecontrol = federatedlearninginstance->nodecontrol;

  federatedlearninginstance->globalmodelstatus=0;

  if(federatedlearninginstance->trainingscounter > 0){
    printf("Calculating the new global model (%d models)\n", nodecontrol->modelsreceived);

    //calculate the final weight value
    FederatedAveraging(nodecontrol->neuralnetwork,federatedlearninginstance->trainingscounter);

    //free the global neural network and copy the neuralnetwork to the global one
    freeNeuralNetwork(federatedlearninginstance->neuralnetwork);
    federatedlearninginstance->neuralnetwork = CopyNeuralNetwork(nodecontrol->neuralnetwork);
  }else{
    printf("No training samples received, keeping the current global model\n");
  }

  federatedlearninginstance->trainingscounter = 0;

  // set 0 the neural network
  SetNeuralNetworkZeroWeightValue(nodecontrol->neuralnetwork);
  PerformanceMetrics(federatedlearninginstance->neuralnetwork,30,0.5,nodecontrol->clientnodes);
  nodecontrol->currentinteraction++;
  SaveModel(nodecontrol->currentinteraction);

  //align all nodes, including the ones that missed the round, with the new round
  ClientNode *currentclientnode = nodecontrol->firstclientnode;
  while (currentclientnode!=NULL){
    currentclientnode->interaction = nodecontrol->currentinteraction;
    currentclientnode = currentclientnode->nextclientnode;
  }

  nodecontrol->modelsreceived = 0;
  nodecontrol->roundstarttime = time(NULL);

  if(nodecontrol->currentinteraction >= nodecontrol->interactioncycle){
    printf("The application reached the max interation\n");
    federatedlearninginstance->globalmodelstatus=0;
  }else{
    federatedlearninginstance->globalmodelstatus=1;
  }
}

//returns 1 if the client model was aggregated, 0 if it was rejected
//the caller must hold FederatedLearningLock
int AggregationModel(FederatedLearning * clientmodel, ClientNode * clientnode, int round){

  FederatedLearning * federatedlearninginstance = getFederatedLearningInstance();
  NodeControl *nodecontrol = federatedlearninginstance->nodecontrol;

  if(nodecontrol->phase != PHASE_FEDERATED){
    printf("Model from %s rejected: not in phase FEDERATED (phase %s)\n", clientnode->ip_id, PhaseName(nodecontrol->phase));
    return 0;
  }

  if(nodecontrol->currentinteraction >= nodecontrol->interactioncycle){
    printf("The application reached the max interation\n");
    return 0;
  }

  if(round != nodecontrol->currentinteraction || clientnode->interaction != nodecontrol->currentinteraction){
    printf("Model from %s rejected: duplicated or late (model round %d, node round %d, current round %d)\n",
           clientnode->ip_id, round, clientnode->interaction, nodecontrol->currentinteraction);
    return 0;
  }

  if(!ValidateTopology(nodecontrol->neuralnetwork, clientmodel->neuralnetwork)){
    printf("Model from %s rejected: topology differs from the global model\n", clientnode->ip_id);
    return 0;
  }

  if(clientmodel->trainingscounter <= 0){
    printf("Model from %s rejected: invalid trainingscounter %d\n", clientnode->ip_id, clientmodel->trainingscounter);
    return 0;
  }

  printf("Aggregating Local Model %s (round %d)\n", clientnode->ip_id, round);

  FederatedAveragingSum(nodecontrol->neuralnetwork,
                  clientmodel->neuralnetwork,
                  clientmodel->trainingscounter);

  federatedlearninginstance->trainingscounter += clientmodel->trainingscounter;
  clientnode->interaction++;
  nodecontrol->modelsreceived++;

  //check if all nodes sent the local model
  ClientNode *currentclientnode = nodecontrol->firstclientnode;
  while (currentclientnode!=NULL){
    if(currentclientnode->interaction == nodecontrol->currentinteraction){
      return 1;
    }
    currentclientnode = currentclientnode->nextclientnode;
  }

  FinalizeRound();
  return 1;
}

//////////////////////////////////////////////////PHASES//////////////////////////////////////////////////

const char *PhaseName(int phase){
  switch (phase) {
    case PHASE_REGISTRATION: return "registration";
    case PHASE_TEACHER: return "teacher";
    case PHASE_FEDERATED: return "federated";
    default: return "unknown";
  }
}

//phase 1: only the teacher client works; the regular nodes keep polling with status 0
//the caller must hold FederatedLearningLock
void StartTeacherPhase(){
  FederatedLearning * federatedlearninginstance = getFederatedLearningInstance();
  NodeControl *nodecontrol = federatedlearninginstance->nodecontrol;

  nodecontrol->phase = PHASE_TEACHER;
  nodecontrol->teacherstarttime = time(NULL);
  federatedlearninginstance->globalmodelstatus = 0;
  printf("Phase TEACHER: waiting for the teacher client to train the teacher model\n");
}

//phase 2: federated rounds; the teacher client participates as a regular node
//the caller must hold FederatedLearningLock
void StartFederatedPhase(){
  FederatedLearning * federatedlearninginstance = getFederatedLearningInstance();
  NodeControl *nodecontrol = federatedlearninginstance->nodecontrol;

  nodecontrol->phase = PHASE_FEDERATED;

  ClientNode *currentclientnode = nodecontrol->firstclientnode;
  while (currentclientnode!=NULL){
    currentclientnode->interaction = nodecontrol->currentinteraction;
    currentclientnode = currentclientnode->nextclientnode;
  }

  nodecontrol->modelsreceived = 0;
  nodecontrol->roundstarttime = time(NULL);
  federatedlearninginstance->globalmodelstatus = 1;
  printf("Phase FEDERATED: %s\n", nodecontrol->teachertrained ?
         "teacher available for distillation" : "no teacher, plain federated averaging");
}

//stores the teacher trained by the teacher client and starts the federated phase
//returns 1 if accepted; the caller must hold FederatedLearningLock
int ReceiveTeacherModel(FederatedLearning * teachermodel, ClientNode * clientnode){

  FederatedLearning * federatedlearninginstance = getFederatedLearningInstance();
  NodeControl *nodecontrol = federatedlearninginstance->nodecontrol;

  if(nodecontrol->phase != PHASE_TEACHER){
    printf("Teacher from %s rejected: not in phase TEACHER (phase %s)\n", clientnode->ip_id, PhaseName(nodecontrol->phase));
    return 0;
  }

  if(!clientnode->isteacher){
    printf("Teacher from %s rejected: node is not the teacher client\n", clientnode->ip_id);
    return 0;
  }

  if(!ValidateTopology(nodecontrol->teacherneuralnetwork, teachermodel->neuralnetwork)){
    printf("Teacher from %s rejected: topology differs from the teacher task\n", clientnode->ip_id);
    return 0;
  }

  if(teachermodel->trainingscounter <= 0){
    printf("Teacher from %s rejected: invalid trainingscounter %d\n", clientnode->ip_id, teachermodel->trainingscounter);
    return 0;
  }

  freeNeuralNetwork(nodecontrol->teacherneuralnetwork);
  nodecontrol->teacherneuralnetwork = CopyNeuralNetwork(teachermodel->neuralnetwork);
  nodecontrol->teachertrained = 1;
  nodecontrol->teachertrainingscounter = teachermodel->trainingscounter;
  printf("Teacher model received from %s (%d samples)\n", clientnode->ip_id, teachermodel->trainingscounter);

  SaveTeacherModel();
  //teacher metrics go to metrics.csv with interaction = -1
  PerformanceMetricsTagged(nodecontrol->teacherneuralnetwork, 30, 0.5, nodecontrol->clientnodes, -1);

  StartFederatedPhase();
  return 1;
}

//close the round with the models already received when some node does not answer
//in phase TEACHER, start the federated phase without a teacher after TEACHER_TIMEOUT_S
void CheckRoundTimeout(){

  FederatedLearningLock();

  FederatedLearning * federatedlearninginstance = getFederatedLearningInstance();
  NodeControl *nodecontrol = federatedlearninginstance->nodecontrol;

  if(nodecontrol->phase == PHASE_TEACHER &&
     time(NULL) - nodecontrol->teacherstarttime >= TEACHER_TIMEOUT_S){
    printf("Teacher timeout (%d s), starting federated training without teacher\n", TEACHER_TIMEOUT_S);
    StartFederatedPhase();
  }

  if(nodecontrol->phase == PHASE_FEDERATED &&
     federatedlearninginstance->globalmodelstatus &&
     time(NULL) - nodecontrol->roundstarttime >= ROUND_TIMEOUT_S){

    if(nodecontrol->modelsreceived == 0){
      //nobody answered, keep waiting
      nodecontrol->roundstarttime = time(NULL);
    }else{
      printf("Round %d timeout, missing nodes:", nodecontrol->currentinteraction);
      ClientNode *currentclientnode = nodecontrol->firstclientnode;
      while (currentclientnode!=NULL){
        if(currentclientnode->interaction == nodecontrol->currentinteraction){
          printf(" %s", currentclientnode->ip_id);
        }
        currentclientnode = currentclientnode->nextclientnode;
      }
      printf("\n");
      FinalizeRound();
    }
  }

  FederatedLearningUnlock();
}

static float SafeDivision(float numerator, float denominator){
  return denominator == 0 ? 0 : numerator / denominator;
}

float Accuracy(int truepositive,int truenegative,int falsepositive,int falsenegative){
  return SafeDivision(truenegative+truepositive, truenegative+truepositive+falsenegative+falsepositive);
}

float Precision(int truepositive,int falsepositive){
  return SafeDivision(truepositive, truepositive+falsepositive);
}

float Recall(int truepositive,int falsenegative){
  return SafeDivision(truepositive, truepositive+falsenegative);
}

float Specificity(int truenegative,int falsepositive){
  return SafeDivision(truenegative, truenegative+falsepositive);
}

float F1Score(int truepositive,int falsepositive,int falsenegative){
  float precision = Precision(truepositive,falsepositive);
  float recall = Recall(truepositive,falsenegative);
  return SafeDivision(2 * precision * recall, precision + recall);
}

void PerformanceMetrics(NeuralNetwork * neuralnetwork,int PercentualEvaluation,float Threshold, int clientnodes){
  PerformanceMetricsTagged(neuralnetwork, PercentualEvaluation, Threshold, clientnodes,
                           getFederatedLearningInstance()->nodecontrol->currentinteraction);
}

//interaction: round written in metrics.csv (-1 for the teacher model)
void PerformanceMetricsTagged(NeuralNetwork * neuralnetwork,int PercentualEvaluation,float Threshold, int clientnodes, int interaction){
  
  int num_data = neuralnetwork->firstlayer->neurons + neuralnetwork->lastlayer->neurons;
  float trainingsample[num_data];
  float label[neuralnetwork->lastlayer->neurons];
  int total=0,truepositive=0,falsepositive=0,truenegative=0,falsenegative=0;
  int samples=0;
  float totalloss=0;
  Neuron * currentneuron;

  FILE * file = NULL;
  char line[1024];

  file = fopen("data/datasetevaluation.csv", "r");

  if (file == NULL) {
    perror("Erro ao abrir o arquivo");
    return;
  }

  while(samples<PercentualEvaluation && fgets(line, sizeof(line), file) != NULL){

        // Divide a linha em campos usando a função strtok (o primeiro campo é o id)
        char * token = strtok(line, ",");
        int fields = 0;

        for (int i = 0; i < num_data; i++){
          token = strtok(NULL, ",");
          if (token == NULL) {
            break;
          }
          trainingsample[i] = strtof(token, NULL);
          fields++;
        }

        if (fields != num_data) {
          // linha vazia ou incompleta: não avalia com dados antigos
          printf("Erro: Não há dados suficientes para preencher InputData na linha.\n");
          continue;
        }
        samples++;

        currentneuron =  neuralnetwork->firstlayer->firstneuron;
        for (int i = 0; i < neuralnetwork->firstlayer->neurons; i++) {
          currentneuron -> activationfunctionvalue = trainingsample[i];
          currentneuron = currentneuron -> nextneuron;
        }

        //set the label on label vector
        for (int i = neuralnetwork->firstlayer->neurons; i < neuralnetwork->firstlayer->neurons + neuralnetwork->lastlayer->neurons; i++) {
          label[i - neuralnetwork->firstlayer->neurons] = trainingsample[i];
          
        }

      FeedFoward(neuralnetwork);
      //evaluation loss without the regularization term
      totalloss += LossFunctionCalculation(neuralnetwork, label, NONE_REGULARIZATION, 0);

      currentneuron = neuralnetwork->lastlayer->firstneuron;

      for(int i=0;i< neuralnetwork->lastlayer->neurons;i++){

        if(label[i]==1){
          if(currentneuron->activationfunctionvalue > Threshold){
            truepositive++;
          }else{
            falsenegative++;
          }
          total++;

        }else{
          if(currentneuron->activationfunctionvalue < Threshold){
            truenegative++;
          }else{
            falsepositive++;
          }
          total++;
        }
        currentneuron = currentneuron->nextneuron;
      }
  }

  fclose(file);
  float avgloss = SafeDivision(totalloss, samples);
  printf("total = %d, TP = %d, TN = %d, FP = %d, FN = %d\n",total,truepositive,truenegative,falsepositive,falsenegative);
  printf("Loss: %.4f\n",avgloss);
  printf("Accuracy: %.2f\n",Accuracy(truepositive,truenegative,falsepositive,falsenegative));
  printf("Precision: %.2f\n",Precision(truepositive,falsepositive));
  printf("Recall: %.2f\n",Recall(truepositive,falsenegative));
  printf("Specificity: %.2f\n",Specificity(truenegative,falsepositive));
  printf("F1-Score: %.2f\n",F1Score(truepositive,falsepositive,falsenegative));

  char metricsfilename[256];
  snprintf(metricsfilename, sizeof(metricsfilename), "%s/metrics.csv", runResultsDir);

  FILE *metricsfile = fopen(metricsfilename, "a");
  if (metricsfile != NULL) {
    if (ftell(metricsfile) == 0) {
      fprintf(metricsfile, "interaction,nodes,loss,accuracy,precision,recall,specificity,f1score,TP,TN,FP,FN\n");
    }
    fprintf(metricsfile, "%d,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d\n",
      interaction,
      clientnodes,
      avgloss,
      Accuracy(truepositive,truenegative,falsepositive,falsenegative),
      Precision(truepositive,falsepositive),
      Recall(truepositive,falsenegative),
      Specificity(truenegative,falsepositive),
      F1Score(truepositive,falsepositive,falsenegative),
      truepositive,
      truenegative,
      falsepositive,
      falsenegative
      );

    fclose(metricsfile);
  }

}
