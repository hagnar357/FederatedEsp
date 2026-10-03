#ifndef _federatedlearning
#define _federatedlearning

//activation function
#define PERCEPTRON 1
#define RELU 2
#define SIGMOID 3

//output activation function
#define SOFTMAX 4

//identity activation, used for the autoencoder teacher's reconstruction layer
#define LINEAR 5

//loss function
#define MINIMAL_MEAN_SQUARE 1
#define CATEGORICAL_CROSS_ENTROPY 2

//regularization
#define NONE_REGULARIZATION 0
#define L1 1
#define L2 2

typedef struct Weight {
  float weight;
  struct Weight * previousweight, * nextweight;
}Weight;

typedef struct Neuron {
  char neurontype[20];
  int weights;
  float activationfunctionvalue;
  float preactivation; // Z before the activation (logit on the output layer), not serialized
  float bias;
  struct Weight * firstweight, * lastweight;
  struct Neuron * nextneuron, * previousneuron;
}Neuron;

typedef struct Layer {
  int activationfunctiontype;
  int neurons;
  struct Neuron * firstneuron, * lastneuron;
  struct Layer * previouslayer, * nextlayer;
}Layer;

typedef struct LayerConfig{
  struct LayerConfig *first, *next;
  int neurons;
  int activationfunctiontype;
}LayerConfig;

typedef struct NeuralNetwork {
  int epoch;
  float alpha;
  int regularization;
  float lambda;
  int percentualtraining;
  int lossfunctiontype;
  int layers;
  struct Layer * firstlayer, * lastlayer;
}NeuralNetwork;

typedef struct FederatedLearning{
    int globalmodelstatus;
    int trainingscounter;
    NeuralNetwork *neuralnetwork;
}FederatedLearning;

FederatedLearning *getFederatedLearningInstance();
void replaceNeuralNetwork(FederatedLearning * newfederatedlearninginstance);
void freeNeuralNetwork(NeuralNetwork *neuralnetwork);
void freeFederatedLearning(FederatedLearning *federatedlearning);
void mergeNeuralNetwork(FederatedLearning * newfederatedlearninginstance);
void PrintNeuralNetwork(NeuralNetwork * neuralnetwork);
//void NeuralNetworkTraining(NeuralNetwork * neuralnetwork, float LearningRate,int epoch,int PercentualTraining);
void NeuralNetworkTraining();
void FeedFoward(NeuralNetwork * neuralnetwork);
float ActivationFunctionCalculaton(float Z, int activationfunctiontype);
float ActivationFunctionDerivativeCalculation(float a, int activationfunctiontype);
void BackPropagation(NeuralNetwork *neuralnetwork, const float *outputdelta, float alpha, int regularization, float lambda, const float *hintdelta);
void CrossEntropyOutputDelta(NeuralNetwork *neuralnetwork, const float *label, float *delta);
void ReconstructionOutputDelta(NeuralNetwork *neuralnetwork, const float *label, float *delta);
float RidgeRegressionCalculation(NeuralNetwork *neuralnetwork,float lambda);
float LassoRegressionCalculation(NeuralNetwork *neuralnetwork,float lambda);
float LossFunctionCalculation(NeuralNetwork * neuralnetwork, float *labelvector, int regularization,float lambda);
void teste();

int contar_instancias(const char *caminho_arquivo);

#endif