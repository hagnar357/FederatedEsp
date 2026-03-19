#include <stdio.h>

#include <zephyr/kernel.h>          // Substitui TUDO do FreeRTOS
#include <zephyr/fs/fs.h>           // Substitui o esp_spiffs.h
#include <zephyr/fs/littlefs.h>     // Sistema de arquivos recomendado para Flash
#include <zephyr/drivers/i2c.h>     // Substitui driver/i2c.h
#include <zephyr/sys/reboot.h>      // Provável substituto para funções do esp_system.h
#include <zephyr/net/net_ip.h>

#include "espconfiguration.h"
#include "httpclient.h"
#include "websocketclient.h"
#include "federatedlearning.h"
#include "JSONConverter.h"


// Defina o endereço I2C do INA219 (geralmente é 0x40)
#define INA219_ADDR 0x40

// Pinos I2C
#define SDA_PIN 21
#define SCL_PIN 22

void brink_error_led(int blink){

    for(int i=0;i<blink;i++){
        // gpio_set_level(LED_PIN_ERROR, 1);
        k_msleep(100);
        // gpio_set_level(LED_PIN_ERROR, 0);
    }
}

void node_register(){
    // gpio_set_level(LED_PIN_SYNC, 1);
    getregisternode();
    k_msleep(100);
    // gpio_set_level(LED_PIN_SYNC, 0);
}

int global_model_status(){
    int status=0;
    // gpio_set_level(LED_PIN_SYNC, 1);
    status=getglobalmodelstatus();
    k_msleep(50);
    // gpio_set_level(LED_PIN_SYNC, 0);
    k_msleep(50);
    return status;
}

FederatedLearning *global_model(){
    // gpio_set_level(LED_PIN_SYNC, 1);
    FederatedLearning *globalmodelinstance = getglobalmodel();
    // gpio_set_level(LED_PIN_SYNC, 0);
    if(globalmodelinstance==NULL){
        while (globalmodelinstance==NULL){
            printf("Json null\n");
            brink_error_led(2);
            // gpio_set_level(LED_PIN_SYNC, 1);
            globalmodelinstance = getglobalmodel();     
            // gpio_set_level(LED_PIN_SYNC, 0);
        }
    }else{
            printf("Json not null\n");
        
    }
    
    return globalmodelinstance;
}

void deep_learning(){

    int ctrl=0;

    while (1){
        if(global_model_status()){
            replaceNeuralNetwork(global_model());
            NeuralNetworkTraining();
            websocket_send_local_model();
        }else{
            k_msleep(4000);
        }
        ctrl++;
    }
}


void deep_learning_test(){
    if(global_model_status()){
        FederatedLearning *globalmodelinstance = getglobalmodel();
        printf("getmodel\n");
        replaceNeuralNetwork(globalmodelinstance);
        PrintNeuralNetwork(globalmodelinstance->neuralnetwork);
        NeuralNetworkTraining();
        FederatedLearning *FDI = getFederatedLearningInstance();
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
    int startled = 0;
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
    printf("1");
    start_federated_learning_system_button();

    // Tenta pegar a ficha. Se retornar 0, significa que pegou com sucesso!
    if (k_sem_take(&wifi_connected_sem, K_SECONDS(15)) == 0) {
        printf("Acesso liberado! Iniciando comunicacao com o servidor...\n");
    } else {
        printf("ERRO FATAL: Timeout. O roteador nao forneceu o IP.\n");
        // Opcional: Você pode colocar um k_msleep aqui e reiniciar a placa (sys_reboot)
    }
    
    printf("enter noderegister\n");
    node_register();
    printf("pass noderegister\n");
    deep_learning();

    return 0;
}
