# Aprendizado Federado no Zephyr RTOS (ESP32-S3)

Este projeto implementa um nó de **Aprendizado Federado (Federated Edge Computing)** rodando nativamente no sistema operacional de tempo real **Zephyr RTOS**, utilizando um microcontrolador **ESP32-S3**. 

O nó é responsável por baixar um modelo global via HTTP, treinar uma Rede Neural localmente lendo dados de um arquivo `.csv` armazenado na memória flash (LittleFS), e enviar os pesos atualizados de volta para o servidor central através de WebSockets.

---

## 1. Pré-requisitos e Dependências

Para compilar e rodar este projeto, você precisa ter o ambiente do Zephyr configurado.

1. **Zephyr SDK** instalado e configurado (`west`).
2. Placa **ESP32-S3** conectada via USB.
3. **esptool.py** (ferramenta oficial da Espressif, já inclusa no ambiente Zephyr/ESP-IDF).
4. **mklittlefs** (ferramenta para criar a imagem do sistema de arquivos).
   * *Para instalar no Linux:*
     ```bash
      git clone https://github.com/earlephilhower/mklittlefs.git
      cd mklittlefs
      git submodule update --init
      make
      sudo mv mklittlefs /usr/local/bin/
      cd ..
      rm -rf mklittlefs
     ```

---

## 2. Habilitando a Biblioteca cJSON no Zephyr

O projeto utiliza a biblioteca **cJSON** para fazer o *parse* do modelo global e montar o *payload* do modelo local. 

Para adicionar a biblioteca cJSON nativamente ao zephyr, dentro da pasta zephyrproject(criada quando você fez a instalação do zephyr), abra o arquivo:
```./zephyr/west.yaml```
e dentro do bloco projects, adicione:

```yaml
[...]
  projects:
  [...]
    - name: cjson-zephyr
      remote: mendersoftware
      revision: main
      repo-path: cjson-zephyr
      path: modules/lib/cjson-zephyr
[...]
```

Após isso, na pasta raiz do zephyr (zephyrproject) rode os comandos:

```bash
west update
cd modules/lib/cjson-zephyr
git submodule update --init --recursive
```
---

## 3. Gravando o Dataset (Arquivo CSV) na Placa

Como o Zephyr não possui um sistema de arquivos dinâmico por padrão, nós usamos o **LittleFS** para gerenciar arquivos na memória Flash. O `csv` usado no treinamento da Rede Neural precisa ser empacotado e gravado no endereço correto da memória.

### Passo 3.1: Preparar a Imagem
Crie uma pasta chamada `fs_data` na raiz do projeto e coloque o seu arquivo CSV dentro dela:
```bash
mkdir fs_data
cp /caminho/para/seu/dataset.csv fs_data/
```

### Passo 3.2: Gerar o arquivo `.bin`
Execute o `mklittlefs` para empacotar a pasta em uma imagem de 1MB (tamanho padrão da partição de armazenamento):
```bash
mklittlefs -c fs_data -p 16 -b 4096 -s 1048576 storage.bin
```

### Passo 3.3: Injetar a imagem na Flash do ESP32-S3
Use o `esptool.py` para gravar o arquivo `storage.bin` exatamente no endereço de montagem da partição (`0x110000`):
```bash
esptool.py --chip esp32s3 --port /dev/USB0 --baud 460800 write_flash 0x110000 storage.bin
```
*(Nota: Altere `/dev/ttyUSB0` para a porta USB correta onde seu ESP32 está conectado).*


---

## 5. Compilação e Execução

**Para compilar :**
```bash
west build -b heltec_wireless_stick_lite_v3/esp32s3/procpu ./FederatedEdgeComputing/FederatedLearningZephyr --pristine

```

**Para gravar o firmware:**
```bash
west flash
```

**Para visualizar os logs em tempo real:**
Você pode usar qualquer monitor serial (como minicom, picocom ou a extensão do VSCode). A taxa de transmissão padrão (Baud Rate) do console Zephyr é `115200`.
```bash
west espressif monitor
```

**Fazendo tudo junto**
```bash
west build -b heltec_wireless_stick_lite_v3/esp32s3/procpu ./FederatedEdgeComputing/FederatedLearningZephyr --pristine  && west flash --esp-device /dev/ttyUSB0  && west espressif monitor -p /dev/ttyUSB0
```
---

## 6. Notas de Arquitetura e Solução de Problemas

* **Leitura Nativa de Arquivos:** O projeto não utiliza a biblioteca padrão de C (`stdio.h`, `fopen`, `fgets`), pois isso causa exceções de hardware (Crash `EXCCAUSE 28`) no ESP32 sob o Zephyr. A leitura é feita puramente com a API Nativa do File System (`<zephyr/fs/fs.h>`).
* **Memória Dinâmica (RAM):** O gerenciamento de memória ao receber pacotes HTTP ou montar JSONs é feito estritamente utilizando a biblioteca nativa do C (`realloc`, `free`), garantindo a estabilidade do Heap do Zephyr.
* **Handshake WebSocket:** O servidor central (`libwebsockets` em C/Node/Python) exige um subprotocolo estrito para aceitar conexões. A requisição WebSocket no cliente Zephyr foi modificada para injetar o cabeçalho obrigatório: `Sec-WebSocket-Protocol: echo-protocol`. Sem ele, a conexão retorna o erro `-113 (ECONNABORTED)`.


---

## 7. Knowledge Distillation (Destilação de Conhecimento)

O nó pode treinar o modelo local destilando um **teacher enviado pelo servidor**. O modelo local (student) mantém a arquitetura do modelo global, que é o que o servidor aceita na agregação. O teacher pode ter qualquer arquitetura, desde que tenha o mesmo número de entradas e de saídas e a camada de saída seja SOFTMAX.

### Função de perda
Para cada amostra, com logits do student `z_s`, logits do teacher `z_t`, temperatura `T` e peso `alpha`:

```
L = (1 - alpha) * CE(y, softmax(z_s)) + alpha * T^2 * KL( softmax(z_t/T) || softmax(z_s/T) )   (+ termo L1/L2 do modelo)
dL/dz_s = (1 - alpha) * (softmax(z_s) - y) + alpha * T * (softmax(z_s/T) - softmax(z_t/T))
```
Com `alpha = 0`, o treino é idêntico ao treino sem destilação.

### Ativação
Por Kconfig (`prj.conf`), desligado por padrão:
```
CONFIG_FL_KNOWLEDGE_DISTILLATION=y
CONFIG_FL_KD_TEMPERATURE_X10=30   # T = 3.0
CONFIG_FL_KD_ALPHA_PERCENT=50     # alpha = 0.5
```
Ou só no build: `west build ... -- -DCONFIG_FL_KNOWLEDGE_DISTILLATION=y`.

Em tempo de execução (`include/knowledgedistillation.h`), chamado na mesma thread do treino:
```c
KnowledgeDistillationEnable(3.0f, 0.5f);   // liga com T e alpha locais
KnowledgeDistillationDisable();
KnowledgeDistillationGetConfig();          // valores efetivos (os do servidor têm prioridade)
```

### Contrato com o servidor
Com a destilação ligada, a cada rodada o nó faz `GET /api/getteachermodel` (uma tentativa, sem repetir em 4xx) antes de treinar. A resposta usa o **mesmo JSON de `/api/getglobalmodel`**, com um objeto opcional de parâmetros:
```json
{
  "distillation": { "temperature": 3.0, "alpha": 0.5 },
  "neuralnetwork": { "layers": 3, "layersArray": [ ... ] }
}
```
- `temperature` e `alpha`, quando enviados, substituem os valores locais.
- Se a requisição falhar, o nó usa o teacher recebido numa rodada anterior. Se nunca recebeu um teacher, treina sem destilação. O modelo continua sendo enviado normalmente.
- O teacher é parseado com cJSON no heap (`CONFIG_HEAP_MEM_POOL_SIZE`). Um teacher muito maior que o modelo atual pode exigir aumentar esse heap.
