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
