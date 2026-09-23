#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "federatedlearning.h"
#include "espconfiguration.h"
#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(FedLearning, LOG_LEVEL_INF);

//////////////////////////////////////////////////PRINT//////////////////////////////////////////////////

void teste(){
    printf("🐉\n");
}

ssize_t zephyr_fgets(char *buf, size_t max_len, struct fs_file_t *file) {
    size_t i = 0;
    char c;
    while (i < max_len - 1) {
        ssize_t bytes_read = fs_read(file, &c, 1);
        if (bytes_read < 0) return bytes_read; // Erro de leitura
        if (bytes_read == 0) break;            // Fim do Arquivo (EOF)
        
        buf[i++] = c;
        if (c == '\n') break; // Fim da linha
    }
    buf[i] = '\0';
    return i; // Retorna quantos bytes leu (0 = EOF)
}

int contar_instancias(const char *caminho_arquivo) {
    struct fs_file_t file;
    fs_file_t_init(&file);

    int rc = fs_open(&file, caminho_arquivo, FS_O_READ);
    if (rc < 0) {
        LOG_ERR("Erro %d ao abrir o arquivo %s para contagem.", rc, caminho_arquivo);
        return -1;
    }

    uint8_t buffer[READ_BUFFER_SIZE];
    int total_linhas = 0;
    ssize_t bytes_lidos;
    uint8_t ultimo_caractere = '\n'; // Previne contagem errada em arquivos vazios

    // Lê o arquivo em blocos
    while ((bytes_lidos = fs_read(&file, buffer, READ_BUFFER_SIZE)) > 0) {
        for (ssize_t i = 0; i < bytes_lidos; i++) {
            if (buffer[i] == '\n') {
                total_linhas++;
            }
        }
        // Guarda o último caractere do bloco atual
        ultimo_caractere = buffer[bytes_lidos - 1];
    }

    if (bytes_lidos < 0) {
        LOG_ERR("Erro %d durante a leitura do arquivo para contagem.", (int)bytes_lidos);
        fs_close(&file);
        return -1;
    }

    // Se o dataset não termina com '\n', a última linha precisa ser contada
    if (ultimo_caractere != '\n') {
        total_linhas++;
    }

    fs_close(&file);
    return total_linhas;
}

void PrintNeuralNetwork(NeuralNetwork * neuralnetwork) {

  int i = 0, j = 0, k = 0;

  Layer * currentlayer = neuralnetwork -> firstlayer;
  while (currentlayer != NULL) {

    i++;
    printf("layer %d neurons: %d\n", i, currentlayer -> neurons);

    Neuron * currentneuron = currentlayer -> firstneuron;

    while (currentneuron != NULL) {

      j++;
      printf("%s neuron %d - weights: %d\n",currentneuron->neurontype , j,currentneuron -> weights);
      printf("Bias: %.10f\n",currentneuron->bias);

      Weight * currentweight = currentneuron -> firstweight;

      while (currentweight != NULL) {

        k++;
        printf("%d value: %.10f ", k, currentweight -> weight);

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

void printVector(float *data,int size){
  for(int i =0; i< size;i++){
    printf("%f ", data[i]);
  }
  printf("\n");
}

void printMatriz(float **data, int linhas, int colunas) {
    for (int i = 0; i < linhas; i++) {
        for (int j = 0; j < colunas; j++) {
            printf("%f ", data[i][j]);
        }
        printf("\n");
    }
}

//////////////////////////////////////////////////DEEPLEARNINGMATHFUNCTIONS//////////////////////////////////////////////////

float Perceptron(float Z){
    if(Z>0)
    {
      return 1;
    }
    else{
     return 0; 
    }
}

float PerceptronDerivative(float a){
    if(a>0){
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

float ReLUDerivative(float a){
    if(a>0){
      return 1;
    }
    else{
     return 0; 
    }
}

float Sigmoid(float Z) {
  return 1.0 / (1.0 + exp(-Z));
}

float SigmoidDerivative(float a) {
  return a * (1 - a);
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

float SoftMaxDerivative1(float a){
  
  return a*(1-a);
}

float SoftMaxDerivative2(float a1 , float a2){
  
  return a1*a2;

}

float CategoricalCrossEntropyDerivative(float y, float a) {
  return -(y / a);
}

float CategoricalCrossEntropy(float * label, float * output, int labelsize) {
  float sum = 0;
  for (int i = 0; i < labelsize; i++) {
    sum += label[i] * log(fmaxf(output[i], 1e-7f));
  }
  return -sum;
}

//////////////////////////////////////////////////FREEALLOCATEDMEMORY//////////////////////////////////////////////////

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

void freeFederatedLearning(FederatedLearning *federatedlearning){

  if(federatedlearning==NULL){
    return;
  }
  freeNeuralNetwork(federatedlearning->neuralnetwork);
  free(federatedlearning);
}
void freeVector(float *vector) {
    if (vector != NULL) {
        free(vector);
    }
}

void freeMatrix(float **matrix, int lines) {
    if (matrix != NULL) {
        for (int i = 0; i < lines; i++) {
            if (matrix[i] != NULL) {
                free(matrix[i]);
            }
        }
        free(matrix);
    }
}

//////////////////////////////////////////////////DEEPLEARNINGNEURALNETWORK//////////////////////////////////////////////////

float ActivationFunctionCalculaton(float Z, int activationfunctiontype){

  //printf("activaiton function type: %d",activationfunctiontype);

switch (activationfunctiontype)
{
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

float ActivationFunctionDerivativeCalculation(float a, int activationfunctiontype){
  
switch (activationfunctiontype){

case 1:
  return PerceptronDerivative(a);
case 2:
  return ReLUDerivative(a);
case 3:
  return SigmoidDerivative(a); 
  
  default:
    return 0;
  }
}

float StochasticGradientDescentCalculation( float weight,float output ,float deltafunction,int regularization,float alpha,float lambda){

  switch (regularization){
    
    case 0:
      //printf("REGULATION 0 ");
      return -alpha * deltafunction * output;
    
    case 1:
      //printf("REGULATION 1 ");

      if(weight >0){
        return -alpha * (deltafunction * output + lambda);
      }
      else if(weight==0){
        return -alpha * (deltafunction * output);
      }
      else{
        return -alpha * (deltafunction * output - lambda);
      }
    case 2:
      //printf("REGULATION 2 ");

      return -alpha * (deltafunction * output + 2*lambda*weight);
      
    default:
      return 0;
  }
}

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
      case 1:
        return  CategoricalCrossEntropy(labelvector, outputdata, neuralnetwork->lastlayer->neurons) + LassoRegressionCalculation(neuralnetwork,lambda);
      case 2:
        return  CategoricalCrossEntropy(labelvector, outputdata, neuralnetwork->lastlayer->neurons) + RidgeRegressionCalculation(neuralnetwork,lambda);
      default:
        break;
    }

    break;
  default:
    return 0;
  }

  return 0;
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

void BackPropagation(NeuralNetwork *neuralnetwork, float *label, float alpha, int regularization, float lambda){
  //printf("BACKPROPAGATION\n");
      
  //aux memory 
  int  weightscolumn=0, weightslines=0, deltafunctionsnumber=0;
  float **weights = NULL;
  float *deltafunctions = NULL;
  // variables to cycle through pointers
  Layer * currentlayer = neuralnetwork -> firstlayer;
  Neuron * currentneuron = currentlayer -> firstneuron;
  Weight * currentweight;

  currentlayer = neuralnetwork -> lastlayer;
  for (int layer = neuralnetwork -> layers ; layer > 1; layer--) {

    //verify the last layer
    if (layer == neuralnetwork -> layers) {

      //printf("layer %d \n",layer);

      //allocate memory for delta functions
      deltafunctionsnumber = currentlayer->neurons;
      deltafunctions = (float * ) malloc(deltafunctionsnumber * sizeof(float));
      currentneuron = currentlayer -> firstneuron;

      for (int i = 0; i < currentlayer->neurons; i++) {
        deltafunctions[i] =  currentneuron->activationfunctionvalue - label[i];
        //printf("label %f output value %f delta %f\n",label[i],currentneuron -> activationfunctionvalue,deltafunctions[i]);
        currentneuron = currentneuron -> nextneuron;
      }

      //printf("\n");

      //alocate memory for weights matrix
      weightslines = currentlayer ->previouslayer -> neurons;
      weightscolumn = currentlayer->neurons;

      weights = (float ** ) malloc(weightslines * sizeof(float * ));

      for (int i = 0; i < currentlayer -> previouslayer -> neurons; i++) {
        weights[i] = (float * ) malloc(weightscolumn * sizeof(float));
      }

      currentneuron = currentlayer -> firstneuron;
      Neuron* previousneuron; 
          
      for (int i = 0; i < currentlayer->neurons; i++){  
        currentneuron->bias += - deltafunctions[i] * alpha;
        previousneuron = currentlayer->previouslayer->firstneuron;
        currentweight = currentneuron->firstweight;

        for(int j=0;j<currentlayer->previouslayer->neurons ;j++){
          //printf("W %f A %f ",currentweight->weight,previousneuron->activationfunctionvalue);
          //currentweight->weight +=  - deltafunctions[i] * previousneuron->activationfunctionvalue * alpha; 
          //the delta of the previous layer uses the weight before the update
          weights[j][i] = currentweight -> weight;
          currentweight->weight += StochasticGradientDescentCalculation(currentweight->weight,previousneuron->activationfunctionvalue,deltafunctions[i],regularization,alpha,lambda);
          //printf("D %f Wn %f\n",deltafunctions[i],currentweight->weight);
          previousneuron =previousneuron->nextneuron;
          currentweight = currentweight->nextweight;
        }
        currentneuron = currentneuron->nextneuron;
      }

    } 
    else{

      //printf("layer %d \n",layer);
      float * deltafunctionsaux = (float * ) malloc(deltafunctionsnumber * sizeof(float));
      memcpy(deltafunctionsaux, deltafunctions, deltafunctionsnumber * sizeof(float));
      deltafunctions = (float * ) realloc(deltafunctions, currentlayer->neurons * sizeof(float));
          
      float deltafunctionaccumulation;

      //calculatting delta
      currentneuron = currentlayer->firstneuron;
      for (int i = 0; i < weightslines; i++) {
        deltafunctionaccumulation = 0;
        for (int j = 0; j < weightscolumn; j++) {
          //printf("Dn %d D %f * W %f \n",j, deltafunctionsaux[j], weights[i][j]);
          deltafunctionaccumulation += deltafunctionsaux[j] * weights[i][j];
        }
          
        deltafunctions[i] = deltafunctionaccumulation *  ActivationFunctionDerivativeCalculation(currentneuron->activationfunctionvalue,currentlayer->activationfunctiontype);
        //printf("A %f Dn%i %f\n",currentneuron->activationfunctionvalue,i, deltafunctions[i]);            
        currentneuron = currentneuron->nextneuron;
      }

      freeMatrix(weights,weightslines);

      weightscolumn = weightslines;
      weightslines = currentlayer -> previouslayer -> neurons;
      deltafunctionsnumber = weightslines;
          
      //printf("wl %d wc %d\n",weightslines,weightscolumn);
          
      weights = (float ** ) malloc(weightslines * sizeof(float * ));          
      for (int i = 0; i < weightslines; i++) {
        weights[i] = (float * ) malloc(weightscolumn * sizeof(float));
      }

      currentneuron = currentlayer->firstneuron;
      Neuron *previousneuron;
          
      for(int i = 0; i< currentlayer->neurons;i++){            
        currentneuron->bias += - deltafunctions[i]*alpha;
        currentweight = currentneuron->firstweight;
        previousneuron = currentlayer->previouslayer->firstneuron;
        for (int j = 0; j < currentneuron->weights; j++){
          //printf("W %f D %f A %f GW %f",currentweight->weight,deltafunctions[i],previousneuron->activationfunctionvalue,- deltafunctions[i]*previousneuron->activationfunctionvalue*LearningRate);
          //the delta of the previous layer uses the weight before the update
          weights[j][i] = currentweight->weight;
          currentweight->weight +=  StochasticGradientDescentCalculation(currentweight->weight,previousneuron->activationfunctionvalue,deltafunctions[i],regularization,alpha,lambda); 
          //currentweight->weight +=  - deltafunctions[i] * previousneuron->activationfunctionvalue * alpha; 
          //printf(" Wn %f \n",currentweight->weight);
          currentweight = currentweight->nextweight;
          previousneuron = previousneuron->nextneuron;
        }
        //printf("\n");            
        currentneuron = currentneuron -> nextneuron;
      }
      //printMatriz(weights,weightslines,weightscolumn);
      freeVector(deltafunctionsaux);
    }
    currentlayer = currentlayer -> previouslayer;
  }

  freeMatrix(weights,weightslines);
  freeVector(deltafunctions);
}

void NeuralNetworkTraining() {

  FederatedLearning *federatedlearninginstance = getFederatedLearningInstance();
  NeuralNetwork * neuralnetwork = federatedlearninginstance->neuralnetwork;

  // trainingscounter stays 0 if nothing is trained, so the model is not sent
  federatedlearninginstance->trainingscounter = 0;

  if (neuralnetwork == NULL) {
      LOG_ERR("Sem modelo global para treinar.");
      return;
  }

  int num_data = neuralnetwork->firstlayer->neurons + neuralnetwork->lastlayer->neurons;
  float trainingsample[num_data];
  float label[neuralnetwork->lastlayer->neurons];
  float Error = 0.0;
  int trainedsamples = 0;

  char line[1024];
  Neuron * currentneuron = NULL;

  // 1. Conta as instâncias totais no CSV
  int total_instancias = contar_instancias(DATA_PATH);
  if (total_instancias <= 0) {
      LOG_ERR("Dataset vazio ou erro na leitura. Abortando treinamento.");
      return;
  }
  
  // 2. Usa todas as instâncias do dataset local no treinamento
  neuralnetwork->percentualtraining = (int)total_instancias;
  
  LOG_INF("Iniciando treino. Instâncias no Dataset: %d",neuralnetwork->percentualtraining);
  
  // Declara a estrutura do arquivo nativa do Zephyr
    struct fs_file_t file;

    for (int TrainingCycle = 0; TrainingCycle < neuralnetwork->epoch; TrainingCycle++) {
        // 1. Inicializa e abre o arquivo pelo Zephyr
        fs_file_t_init(&file);
        int rc = fs_open(&file, DATA_PATH, FS_O_READ);

        if (rc < 0) {
            LOG_ERR("Erro %d ao abrir o arquivo CSV.", rc);
            federatedlearninginstance->trainingscounter = 0;
            return;
        }

        int invalidlines = 0;
        float epochloss = 0;
        int epochsamples = 0;

        for(int count = 0; count < neuralnetwork->percentualtraining; count++){
          
            // 2. Lê a linha usando o nosso fgets customizado
            if (zephyr_fgets(line, sizeof(line), &file) <= 0) {
                // Se retornar 0 ou menos, o arquivo acabou ou deu erro de leitura
                break;
            }

            char * token = strtok(line, ","); 
            int fields = 0;

            for (int i = 0; i < num_data && token != NULL; i++){
                trainingsample[i] = strtof(token, NULL);
                fields++;
                token = strtok(NULL, ",");
            }

            // linha vazia ou incompleta: não treina com valores da linha anterior
            if (fields != num_data) {
                invalidlines++;
                continue;
            }

            //set the input data on inputdata vector
            currentneuron =  neuralnetwork->firstlayer->firstneuron;
            for (int i = 0; i < neuralnetwork->firstlayer->neurons; i++) {
                currentneuron -> activationfunctionvalue = trainingsample[i];
                currentneuron = currentneuron -> nextneuron;
            }

            //set the label on label vector
            for (int i = neuralnetwork->firstlayer->neurons; i < num_data; i++) {
                label[i - neuralnetwork->firstlayer->neurons] = trainingsample[i];
            }

            FeedFoward(neuralnetwork);
            Error = LossFunctionCalculation(neuralnetwork,label,neuralnetwork->regularization,neuralnetwork->lambda);
            BackPropagation(neuralnetwork,label,neuralnetwork->alpha,neuralnetwork->regularization,neuralnetwork->lambda);
            epochloss += Error;
            epochsamples++;
        }

        // 3. Fecha o arquivo usando a API do Zephyr
        fs_close(&file);

        if (invalidlines > 0) {
            LOG_WRN("Epoch %d: %d linhas inválidas ignoradas.", TrainingCycle+1, invalidlines);
        }

        trainedsamples += epochsamples;
        printf("Epoch %d Loss: %f\n", TrainingCycle+1, epochsamples > 0 ? epochloss / epochsamples : 0);
    }

    // peso do nó no FedAvg: amostras efetivamente treinadas em todas as épocas
    federatedlearninginstance->trainingscounter = trainedsamples;
}

//////////////////////////////////////////////////FEDERATEDLEARNING//////////////////////////////////////////////////

//takes ownership of the new model: the previous network and the received wrapper are released
void replaceNeuralNetwork(FederatedLearning * newfederatedlearninginstance){
    FederatedLearning * federatedlearninginstance =  getFederatedLearningInstance();
    freeNeuralNetwork(federatedlearninginstance->neuralnetwork);
    federatedlearninginstance->neuralnetwork = newfederatedlearninginstance->neuralnetwork;
    federatedlearninginstance->trainingscounter=0;
    newfederatedlearninginstance->neuralnetwork = NULL;
    free(newfederatedlearninginstance);
}

void mergeNeuralNetwork(FederatedLearning * newfederatedlearninginstance){
//fazendo nd sei la pq, mas n chama ent ta d boa
}

FederatedLearning *getFederatedLearningInstance() {
    
    //the neural network stays NULL until the first global model is received
    static FederatedLearning instance;
    return &instance;
}
