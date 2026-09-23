#include <stdio.h>

#include <zephyr/kernel.h>          // Substitui TUDO do FreeRTOS
#include <zephyr/fs/fs.h>           // Substitui o esp_spiffs.h
#include <zephyr/fs/littlefs.h>     // Sistema de arquivos recomendado para Flash
#include <zephyr/drivers/i2c.h>     // Substitui driver/i2c.h
#include <zephyr/sys/reboot.h>      // Provável substituto para funções do esp_system.h
#include <zephyr/net/net_ip.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include "espconfiguration.h"
#include "httpclient.h"
#include "websocketclient.h"
#include "federatedlearning.h"
#include "JSONConverter.h"

LOG_MODULE_REGISTER(MAIN, LOG_LEVEL_INF);

// Defina o endereço I2C do INA219 (geralmente é 0x40)
#define INA219_ADDR 0x40

// Pinos I2C
#define SDA_PIN 21
#define SCL_PIN 22

// Define a configuração padrão do LittleFS
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);

// Estrutura que mapeia a partição física para o caminho virtual
// static struct fs_mount_t lfs_storage_mnt = {
//     .type = FS_LITTLEFS,
//     .fs_data = &storage,
//     .storage_dev = (void *)FIXED_PARTITION_ID(storage_partition),
//     .mnt_point = "/storage",
// };


// // Função para montar o disco
// void mount_filesystem(void) {
//     int res = fs_mount(&lfs_storage_mnt);
//     if (res == 0) {
//         LOG_INF("LittleFS montado com sucesso em %s", lfs_storage_mnt.mnt_point);
//     } else {
//         LOG_ERR("Erro ao montar LittleFS: %d", res);
//     }
// }

void brink_error_led(int blink){

    for(int i=0;i<blink;i++){
        // gpio_set_level(LED_PIN_ERROR, 1);
        k_msleep(100);
        // gpio_set_level(LED_PIN_ERROR, 0);
    }
}

#define GLOBAL_MODEL_MAX_ATTEMPTS 5
#define SEND_MODEL_MAX_ATTEMPTS 3

void node_register(){
    // gpio_set_level(LED_PIN_SYNC, 1);
    // repete até o servidor confirmar o registro (novo ou já existente)
    while (!getregisternode()) {
        printf("Registro falhou, tentando novamente...\n");
        brink_error_led(1);
        k_msleep(2000);
        wifi_wait_connected();
    }
    // gpio_set_level(LED_PIN_SYNC, 0);
}

int global_model_status(int *round){
    int status=0;
    // gpio_set_level(LED_PIN_SYNC, 1);
    status=getglobalmodelstatus(round);
    k_msleep(50);
    // gpio_set_level(LED_PIN_SYNC, 0);
    k_msleep(50);
    return status;
}

// Tentativas limitadas com backoff; retorna NULL e o loop volta a checar o status
FederatedLearning *global_model(){
    for (int attempt = 1; attempt <= GLOBAL_MODEL_MAX_ATTEMPTS; attempt++) {
        // gpio_set_level(LED_PIN_SYNC, 1);
        FederatedLearning *globalmodelinstance = getglobalmodel();
        // gpio_set_level(LED_PIN_SYNC, 0);
        if (globalmodelinstance != NULL) {
            printf("Json not null\n");
            return globalmodelinstance;
        }
        printf("Json null (tentativa %d)\n", attempt);
        brink_error_led(2);
        k_msleep(1000 * attempt);
    }
    return NULL;
}

// Envia o modelo já treinado, sem retreinar, até SEND_MODEL_MAX_ATTEMPTS vezes
int send_local_model(int round){
    for (int attempt = 1; attempt <= SEND_MODEL_MAX_ATTEMPTS; attempt++) {
        if (websocket_send_local_model(round) == 0) {
            return 0;
        }
        printf("Envio do modelo falhou (tentativa %d)\n", attempt);
        k_msleep(1000 * attempt);
        wifi_wait_connected();
    }
    return -1;
}

void deep_learning(){

    // rodada do último modelo global treinado e enviado; evita treinar 2x a mesma rodada
    int last_trained_round = -1;

    while (1){
        wifi_wait_connected();

        int round = -1;
        if(!global_model_status(&round) || round == last_trained_round){
            k_msleep(4000);
            continue;
        }

        FederatedLearning *globalmodelinstance = global_model();
        if (globalmodelinstance == NULL) {
            continue;
        }

        replaceNeuralNetwork(globalmodelinstance);
        NeuralNetworkTraining();

        if (getFederatedLearningInstance()->trainingscounter <= 0) {
            printf("Nada foi treinado, modelo não enviado\n");
            brink_error_led(3);
            k_msleep(4000);
            continue;
        }

        if (send_local_model(round) == 0) {
            last_trained_round = round;
        }
    }
}


void deep_learning_test(){
    int round = -1;
    if(global_model_status(&round)){
        FederatedLearning *globalmodelinstance = getglobalmodel();
        if (globalmodelinstance == NULL) {
            return;
        }
        printf("getmodel\n");
        // replaceNeuralNetwork assume a posse do modelo recebido
        replaceNeuralNetwork(globalmodelinstance);
        FederatedLearning *FDI = getFederatedLearningInstance();
        PrintNeuralNetwork(FDI->neuralnetwork);
        NeuralNetworkTraining();
        PrintNeuralNetwork(FDI->neuralnetwork);
    }
}


void start_esp32_configuration(){

    // UARTConfiguration();
    GPIOConfiguration();
    WIFIConfiguration();
    // SPIFFSConfiguration();
    
    // gpio_set_level(LED_PIN_ERROR, 0);
    // gpio_set_level(LED_PIN_SYNC, 0);
    k_msleep(2000);

}

void start_federated_learning_system_button(){
    //int startled = 0;
    /*while (gpio_get_level(BUTTON_PIN)){
        startled = ~startled;
        gpio_set_level(LED_PIN_WORKING, startled);
        k_msleep(250);
    }*/
    //gpio_set_level(LED_PIN_WORKING, 1);
}


int main(){
    printf("iniciou");
    start_esp32_configuration();
    // mount_filesystem();
    printf("1");
    start_federated_learning_system_button();

    // Não segue sem IP: espera (com novas tentativas de conexão) até o roteador fornecer o endereço
    wifi_wait_connected();
    printf("Acesso liberado! Iniciando comunicacao com o servidor...\n");
    
    printf("enter noderegister\n");
    node_register();
    printf("pass noderegister\n");
    deep_learning();

    return 0;
}
