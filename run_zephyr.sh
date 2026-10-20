#!/bin/bash

set -e

# ==========================
# Configurações
# ==========================

# Uso: ./run_zephyr.sh [fração_teacher]   (float entre 0 e 1, opcional)
TEACHER_PART="${1:-}"

if [ -n "$TEACHER_PART" ] && ! awk -v v="$TEACHER_PART" 'BEGIN { exit !(v > 0 && v < 1) }'; then
    echo "Erro: fração do teacher deve ser um número entre 0 e 1 (recebido: $TEACHER_PART)"
    exit 1
fi

ROOT_DIR=$(pwd)

ZEPHYR_DIR="FederatedLearningZephyr"
SERVER_DIR="FederatedLearningServer"
TEACHER_CLIENT_DIR="FederatedLearningTeacherClient"

# Placas: o tipo de cada placa em /dev/ttyUSB* é detectado com o esptool e pode haver S3 e ESP32 clássico ao mesmo tempo
# (um firmware por tipo, gravado só nas placas do tipo; ver lib_boards.sh). Para forçar um tipo em todas as placas:
#   BOARD_TYPE=s3|esp32 ./run_zephyr.sh ...        ou, para uma placa fora desses dois:
#   BOARD=<placa west> CHIP=<chip esptool> OVERLAY=<arquivo, opcional> ./run_zephyr.sh ...
source "$ROOT_DIR/lib_boards.sh"

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

# TTY_GLOB: padrão /dev/ttyUSB* (as placas com USB nativo aparecem como /dev/ttyACM*)
TTYS=(${TTY_GLOB:-/dev/ttyUSB*})

if [ ${#TTYS[@]} -eq 0 ]; then
    echo "Nenhuma placa encontrada."
    exit 1
fi

# tipo de cada placa: forçado por BOARD_TYPE (s3|esp32) ou BOARD/CHIP, senão detectado com o esptool
FORCED=""
if [ -n "${BOARD_TYPE:-}" ] && [ "$BOARD_TYPE" != auto ]; then FORCED=$BOARD_TYPE
elif [ -n "${BOARD:-}${CHIP:-}" ]; then FORCED=custom; fi
PORT_TYPE=(); PORT_CHIP=(); PORT_BUILD=(); PORT_BOARD=(); PORT_OVERLAY=()
for tty in "${TTYS[@]}"; do
    type=${FORCED:-$(board_detect_type "$tty")}
    if [ "$type" = unknown ]; then
        echo "Aviso: não consegui ler o chip de $tty; usando o firmware da S3 (force com BOARD_TYPE=s3|esp32)"
        type=s3
    fi
    read -r pboard pchip poverlay pbuild <<< "$(board_params "$type")"
    PORT_TYPE+=("$type"); PORT_BOARD+=("$pboard"); PORT_CHIP+=("$pchip"); PORT_BUILD+=("$pbuild")
    PORT_OVERLAY+=("$([ "$poverlay" = - ] && echo "" || echo "$poverlay")")
done

echo "Placas encontradas:"
for i in "${!TTYS[@]}"; do echo "  ${TTYS[$i]}  (${PORT_TYPE[$i]})"; done

# As placas + o cliente simulado (teacher) participam do treino federado
NUM_BOARDS=${#TTYS[@]}
NUM_CLIENTS=$((NUM_BOARDS + 1))
TEACHER_NODE_ID=0

sed -i '/CLIENTS_NUM/c\#define CLIENTS_NUM '$NUM_CLIENTS'' \
        "$SERVER_DIR/lib/configs.h"

# ==========================
# Separar datasets (uma partição extra para o cliente teacher)
# ==========================

# Se TEACHER_PART (float entre 0 e 1) for informado, o teacher recebe um dataset balanceado
# com essa fração do total, retirado antes da distribuição Dirichlet dos demais nós.
python3 separate_dataset.py "$NUM_CLIENTS" ${TEACHER_PART:+"$TEACHER_PART"}

# TEACHER_REMOTE=1: the teacher node runs on another computer (./run_remote_teacher.sh there). This script then does not
# start the simulated node; it serves the teacher's slice and its KD flag over HTTP (only the datasets/ folder, read-only)
TEACHER_REMOTE="${TEACHER_REMOTE:-0}"
TEACHER_DATA_PORT="${TEACHER_DATA_PORT:-8000}"
DATA_SERVER_PID=""
if [ "$TEACHER_REMOTE" = "1" ]; then
    if [ "$KD" = "1" ]; then : > datasets/teacher_remote.args; else echo "--no-kd" > datasets/teacher_remote.args; fi
    python3 -m http.server "$TEACHER_DATA_PORT" --bind "${IP:-0.0.0.0}" --directory datasets > "$LOG_DIR/data_server.log" 2>&1 &
    DATA_SERVER_PID=$!
    echo "Professor remoto: dataset em http://${IP:-0.0.0.0}:$TEACHER_DATA_PORT/dataset_no_0.csv (PID $DATA_SERVER_PID)"
fi

# ==========================
# Gravar filesystem de cada nó
# ==========================

IDX=0
for tty in "${TTYS[@]}"; do

    # node id = position in the list of boards (not the ttyUSB number, which changes when a board is replugged)
    NODE_ID=$IDX; IDX=$((IDX + 1))

    echo "Preparando dataset para nó $NODE_ID ($tty)"

    cp \
      "datasets/dataset_no_$((NODE_ID + 1)).csv" \
      "$ZEPHYR_DIR/fs_data/dataset.csv"

    mklittlefs \
      -c "$ZEPHYR_DIR/fs_data" \
      -p 16 \
      -b 4096 \
      -s 2883584 \
      storage.bin

    esptool.py \
      --chip "${PORT_CHIP[$NODE_ID]}" \
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

trap 'kill $SERVER_PID $DATA_SERVER_PID 2>/dev/null || true' EXIT

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

if [ "$TEACHER_REMOTE" = "1" ]; then
    echo "Professor remoto: aguardando o cliente do outro computador em $IP:8888 (nao inicio o no simulado aqui)"
    TEACHER_CLIENT_PID=""
    # same first line the local client writes, so catalog_runs.py can read the teacher's rows (the client log itself stays on the other computer)
    echo "Teacher client: server $IP:8888/8080, dataset datasets/dataset_no_${TEACHER_NODE_ID}.csv ($(wc -l < "datasets/dataset_no_${TEACHER_NODE_ID}.csv") samples), role teacher, KD $([ "$KD" = "1" ] && echo on || echo off) (no remoto; o log completo esta no outro computador)" > "$LOG_DIR/teacher_client.log"
    trap 'kill $SERVER_PID $DATA_SERVER_PID 2>/dev/null || true' EXIT
else
    echo "Compilando cliente teacher..."

    make -C "$TEACHER_CLIENT_DIR" build > "$LOG_DIR/teacher_client_build.log" 2>&1

    # KD=0: o cliente simulado também treina a fase federada sem destilação
    TEACHER_CLIENT_ARGS=()
    if [ "$KD" != "1" ]; then TEACHER_CLIENT_ARGS+=(--no-kd); fi

    "$TEACHER_CLIENT_DIR/FederatedLearningTeacherClient" \
        --server "$IP" \
        --dataset "datasets/dataset_no_${TEACHER_NODE_ID}.csv" \
        "${TEACHER_CLIENT_ARGS[@]}" \
        > "$LOG_DIR/teacher_client.log" 2>&1 &

    TEACHER_CLIENT_PID=$!

    trap 'kill $SERVER_PID $TEACHER_CLIENT_PID $DATA_SERVER_PID 2>/dev/null || true' EXIT

    echo "Cliente teacher PID: $TEACHER_CLIENT_PID"
fi

# ==========================
# Build do firmware
# ==========================

echo "Compilando firmware Zephyr..."

cd "$ROOT_DIR"
cd ..

if [ "$KD" = "1" ]; then KD_CONFIG="y"; else KD_CONFIG="n"; fi

# um firmware por tipo de placa presente, cada um no seu diretório de build
declare -A BUILT
for k in "${!TTYS[@]}"; do
    b=${PORT_BUILD[$k]}
    [ -n "${BUILT[$b]}" ] && continue
    BUILT[$b]=1
    echo "Firmware para ${PORT_TYPE[$k]} (${PORT_BOARD[$k]}) em $b"
    west build \
        -b "${PORT_BOARD[$k]}" \
        "$ROOT_DIR/$ZEPHYR_DIR" \
        -d "$b" \
        --pristine \
        -- -DCONFIG_FL_KNOWLEDGE_DISTILLATION=$KD_CONFIG ${PORT_OVERLAY[$k]:+-DDTC_OVERLAY_FILE=${PORT_OVERLAY[$k]}}
done

# ==========================
# Flash dos nós
# ==========================

for k in "${!TTYS[@]}"; do
    tty=${TTYS[$k]}

    echo "Flashando $tty (${PORT_TYPE[$k]})"

    # a flash that stops half way ("Packet content transfer stopped") is usually the USB link: retry, slower
    flashed=0
    for try in 1 2 3; do
        baud=921600; [ "$try" -gt 1 ] && baud=460800
        if west flash -d "${PORT_BUILD[$k]}" --esp-device "$tty" --esp-baud-rate "$baud"; then flashed=1; break; fi
        echo "Falha ao flashar $tty (tentativa $try de 3)"; sleep 3
    done
    [ "$flashed" = 1 ] || { echo "Nao foi possivel flashar $tty"; exit 1; }

done

# ==========================
# Abrir monitores
# ==========================

MONITOR_PIDS=()

IDX=0
for tty in "${TTYS[@]}"; do

    NODE_ID=$IDX; IDX=$((IDX + 1))

    # the serial monitor must not reset a classic ESP32 when it opens the port (DTR/RTS are wired to EN/IO0)
    MONITOR_NO_RESET=$([ "${PORT_CHIP[$NODE_ID]}" = esp32 ] && echo 1 || echo 0) \
    python3 -u - "$tty" \
        > "$ROOT_DIR/$CLIENT_LOG_DIR/no_${NODE_ID}.log" 2>&1 <<'PYEOF' &
import serial, sys, os
s = serial.Serial()
s.port, s.baudrate, s.timeout = sys.argv[1], 115200, 1
if os.environ.get("MONITOR_NO_RESET") == "1":
    s.dtr = s.rts = False      # classic ESP32 boards: DTR/RTS wired to EN/IO0, opening the port would reset the chip
s.open()
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