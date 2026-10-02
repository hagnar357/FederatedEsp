#!/bin/bash

set -e

# ==========================
# Configurações
# ==========================

ROOT_DIR=$(pwd)

ZEPHYR_DIR="FederatedLearningZephyr"
SERVER_DIR="FederatedLearningServer"
TEACHER_CLIENT_DIR="FederatedLearningTeacherClient"

# KD=0 desliga a destilação nas placas (o teacher continua sendo treinado pelo cliente simulado)
KD="${KD:-1}"

# ID único da execução: logs, modelos e métricas vão para subpastas com esse nome
BASE_RUN_ID="run_$(date +%Y%m%d_%H%M%S)"
RUN_ID="$BASE_RUN_ID"
n=1
while [ -e "log_run/$RUN_ID" ] || [ -e "$SERVER_DIR/modelos/$RUN_ID" ] || [ -e "$SERVER_DIR/resultados/$RUN_ID" ]; do
    RUN_ID="${BASE_RUN_ID}_$n"
    n=$((n + 1))
done

LOG_DIR="log_run/$RUN_ID"
CLIENT_LOG_DIR="$LOG_DIR/clients"

mkdir -p "$LOG_DIR"
mkdir -p "$CLIENT_LOG_DIR"

# ==========================
# Ler IP
# ==========================

read -p "IP do servidor: " IP

# ==========================
# Atualizar headers
# ==========================

if [ -n "$IP" ]; then
    sed -i '/^#define SERVER_IP /c\#define SERVER_IP "'"$IP"'"' \
        "$ZEPHYR_DIR/include/espconfiguration.h"

    sed -i '/^#define WS_SERVER_IP /c\#define WS_SERVER_IP "'"$IP"'"' \
        "$ZEPHYR_DIR/include/espconfiguration.h"

    sed -i '/IP_ADDRESS/c\#define IP_ADDRESS "'"$IP"'"' \
        "$SERVER_DIR/lib/configs.h"
fi

# ==========================
# Descobrir placas
# ==========================

shopt -s nullglob

TTYS=(/dev/ttyUSB*)

if [ ${#TTYS[@]} -eq 0 ]; then
    echo "Nenhuma placa encontrada."
    exit 1
fi

echo "Placas encontradas:"
printf '  %s\n' "${TTYS[@]}"

# As placas + o cliente simulado (teacher) participam do treino federado
NUM_BOARDS=${#TTYS[@]}
NUM_CLIENTS=$((NUM_BOARDS + 1))
TEACHER_NODE_ID=0

sed -i '/CLIENTS_NUM/c\#define CLIENTS_NUM '$NUM_CLIENTS'' \
        "$SERVER_DIR/lib/configs.h"

# ==========================
# Separar datasets (uma partição extra para o cliente teacher)
# ==========================

python3 separate_dataset.py "$NUM_CLIENTS"

# ==========================
# Gravar filesystem de cada nó
# ==========================

for tty in "${TTYS[@]}"; do

    NODE_ID="${tty##*ttyUSB}"

    echo "Preparando dataset para nó $NODE_ID ($tty)"

    cp \
      "datasets/dataset_no_${NODE_ID+1}.csv" \
      "$ZEPHYR_DIR/fs_data/dataset.csv"

    mklittlefs \
      -c "$ZEPHYR_DIR/fs_data" \
      -p 16 \
      -b 4096 \
      -s 1048576 \
      storage.bin

    esptool.py \
      --chip esp32s3 \
      --port "$tty" \
      --baud 460800 \
      write_flash \
      0x110000 \
      storage.bin

done

# ==========================
# Build do servidor
# ==========================

echo "Compilando servidor..."

(
    cd "$SERVER_DIR"
    make build > "../$LOG_DIR/server_build.log" 2>&1
)

# ==========================
# Iniciar servidor
# ==========================

TIMEOUT=30; COUNT=0
while ss -tulnp | grep -q "8888"; do
    echo "Porta 8888 em uso, esperando..."
    sleep 1
    COUNT=$((COUNT + 1))
    if [ $COUNT -ge $TIMEOUT ]; then
        echo "Timeout: porta 8888 continua em uso. Abortando."
        exit 1
    fi
done

trap 'kill $SERVER_PID 2>/dev/null || true' EXIT

(
    cd "$SERVER_DIR"
    RUN_ID="$RUN_ID" make run > "../$LOG_DIR/server.log" 2>&1
) &

SERVER_PID=$!

echo "Servidor PID: $SERVER_PID"

sleep 3

# ==========================
# Cliente simulado (treina o teacher e participa do treino federado)
# ==========================

echo "Compilando cliente teacher..."

make -C "$TEACHER_CLIENT_DIR" build > "$LOG_DIR/teacher_client_build.log" 2>&1

"$TEACHER_CLIENT_DIR/FederatedLearningTeacherClient" \
    --server "$IP" \
    --dataset "datasets/dataset_no_${TEACHER_NODE_ID}.csv" \
    > "$LOG_DIR/teacher_client.log" 2>&1 &

TEACHER_CLIENT_PID=$!

trap 'kill $SERVER_PID $TEACHER_CLIENT_PID 2>/dev/null || true' EXIT

echo "Cliente teacher PID: $TEACHER_CLIENT_PID"

# ==========================
# Build do firmware
# ==========================

echo "Compilando firmware Zephyr..."

cd "$ROOT_DIR"
cd ..

if [ "$KD" = "1" ]; then KD_CONFIG="y"; else KD_CONFIG="n"; fi

west build \
    -b heltec_wireless_stick_lite_v3/esp32s3/procpu \
    "$ROOT_DIR/$ZEPHYR_DIR" \
    --pristine \
    -- -DCONFIG_FL_KNOWLEDGE_DISTILLATION=$KD_CONFIG

# ==========================
# Flash dos nós
# ==========================

for tty in "${TTYS[@]}"; do

    echo "Flashando $tty"

    west flash --esp-device "$tty"

done

# ==========================
# Abrir monitores
# ==========================

MONITOR_PIDS=()

for tty in "${TTYS[@]}"; do

    NODE_ID="${tty##*ttyUSB}"

    python3 -u - "$tty" \
        > "$ROOT_DIR/$CLIENT_LOG_DIR/no_${NODE_ID}.log" 2>&1 <<'PYEOF' &
import serial, sys
with serial.Serial(sys.argv[1], 115200, timeout=1) as s:
    while True:
        line = s.readline()
        if line:
            print(line.decode('utf-8', errors='replace'), end='', flush=True)
PYEOF

    MONITOR_PIDS+=($!)

done

echo
echo "Sistema iniciado."
echo "Run: $RUN_ID"
echo "  Logs:      $LOG_DIR"
echo "  Modelos:   $SERVER_DIR/modelos/$RUN_ID"
echo "  Métricas:  $SERVER_DIR/resultados/$RUN_ID"
echo "Servidor PID: $SERVER_PID"
echo "Cliente teacher PID: $TEACHER_CLIENT_PID (log: $LOG_DIR/teacher_client.log)"
echo "Destilação nas placas: KD=$KD"
echo "Monitores: ${MONITOR_PIDS[*]}"
echo

wait