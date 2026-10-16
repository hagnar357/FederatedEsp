# FederatedLearningTeacherClient

Cliente simulado (Linux) responsável pelo **modelo teacher** da destilação de conhecimento. Ele roda no mesmo PC do servidor e usa **o mesmo código de treino das placas** (`../FederatedLearningZephyr/src/federatedlearning.c`, `JSONConverter.c` e `knowledgedistillation.c`, compilados sem alteração através dos shims em `include/zephyr/`).

## Fluxo

1. Registra-se no servidor com `GET /api/noderegister?role=teacher`.
2. **Fase TEACHER** (o servidor responde `task: "teacher"`): baixa o teacher não treinado em `GET /api/getteachertask`, treina com o dataset local (cross-entropy pura, `TEACHER_EPOCHS` épocas definidas pelo servidor) e envia pelo WebSocket com `"type": "teacher"`.
3. **Fase FEDERATED** (`task: "train"`): participa como um cliente comum — baixa o modelo global a cada rodada, baixa o teacher em `GET /api/getteachermodel`, treina com destilação e envia o modelo local (`"type": "local"`).

## Compilação e uso

```bash
make build
./FederatedLearningTeacherClient --server 192.168.1.10 --dataset ../datasets/dataset_no_2.csv
```

Opções:

| Opção | Descrição |
|---|---|
| `--server IP` | IP do servidor (obrigatório) |
| `--dataset caminho` | CSV local, mesmo formato das placas (obrigatório) |
| `--port` / `--wsport` | Portas HTTP (8888) e WebSocket (8080) |
| `--no-kd` | Treina sem destilação na fase federada |
| `--role client` | Não treina o teacher; participa só como cliente comum |

O `run_zephyr.sh` compila e inicia este cliente automaticamente com a partição extra do dataset (`datasets/dataset_no_<N>.csv`, onde `N` é o número de placas). O log fica em `log_run/<run>/teacher_client.log`.

Dependências: `libwebsockets` (a mesma do servidor).

## Nó professor em outro computador

O `run_zephyr.sh` pode deixar de iniciar o cliente local e esperar um cliente em outro computador da mesma rede:

1. No computador do servidor e das placas, inicie as runs com `TEACHER_REMOTE=1` (por exemplo `TEACHER_REMOTE=1 ./run_sweep.sh` ou `TEACHER_REMOTE=1 ./run_zephyr.sh 0.5`). Em cada run o script serve a pasta `datasets/` (só leitura) por HTTP na porta 8000 (`TEACHER_DATA_PORT`) e grava `datasets/teacher_remote.args` (`--no-kd` quando `KD=0`). O `teacher_client.log` da run recebe só a linha inicial, com as linhas do professor.
2. No outro computador, com o repositório na mesma versão e as dependências (`gcc`, `make`, `libwebsockets-dev`, `curl`), rode **antes** de iniciar as runs:

```bash
./run_remote_teacher.sh <IP do computador do servidor>
```

O script espera o servidor subir, baixa a fatia do professor da run e o flag de KD, roda o cliente até o servidor sumir (fim da run) e volta a esperar a próxima run. Os logs do cliente remoto ficam em `log_teacher_remoto/`. O firewall do computador do servidor precisa liberar as portas 8888, 8080 e 8000 para esse outro computador.

