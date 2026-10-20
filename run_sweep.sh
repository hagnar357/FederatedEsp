#!/bin/bash
# Runs several hardware tests back to back (2 boards + the simulated teacher node) with one configuration, then
# restores configs.h and the active dataset. After every run it refreshes catalogo_runs.csv.
#
# usage (settings via environment, the runs on stdin):
#   DATASET=covertype-7500 TEACHER_FRAC=0.5 STUDENT_NEURONS=4 SEED=42 KD_TEMPERATURE=2.0f \
#     ./run_sweep.sh <<'EOF'
#   semKD:0:TEACHER_ARCH_CLASSIFIER:0.5:1
#   logit0.1:1:TEACHER_ARCH_CLASSIFIER:0.1:1
#   EOF
# each run line is  label:KD(0|1):teacher architecture:alpha:student hint layer
#
# settings (defaults): DATASET=har  TEACHER_FRAC=0.3  STUDENT_NEURONS=16  STUDENT_LAYERS=2  SEED=0 (0 = random)
#   KD_TEMPERATURE / TEACHER_TIMEOUT_S / TEACHER_EPOCHS / TEACHER_ALPHA / TEACHER_HIDDEN_LAYERS / TEACHER_HIDDEN_NEURONS / TEACHER_ENCODER_LAYERS / TEACHER_DECODER_LAYERS / TEACHER_LAMBDA
#   (unset = leave configs.h as it is; the teacher ones only apply to the classifier teacher)
#   IP (default: this machine's address)  RESTORE_DATASET=har  RUN_TIMEOUT_S=7200
#   SWEEP_REDO=2: extra attempts for a run that finishes but fails check_run.sh (node missing in a round, board
#   without the teacher, fewer than ITERATIONS rounds); the invalid attempt is deleted before repeating
# Early failures (e.g. an esptool serial error while writing the dataset) are retried up to 3 times.
ROOT=$(cd "$(dirname "$0")" && pwd)
CFG="$ROOT/FederatedLearningServer/lib/configs.h"
DATASET=${DATASET:-har}
TEACHER_FRAC=${TEACHER_FRAC:-0.3}
STUDENT_NEURONS=${STUDENT_NEURONS:-16}
STUDENT_LAYERS=${STUDENT_LAYERS:-2}
SEED=${SEED:-0}
RESTORE_DATASET=${RESTORE_DATASET:-har}
RUN_TIMEOUT_S=${RUN_TIMEOUT_S:-7200}
SWEEP_REDO=${SWEEP_REDO:-2}
source "$ROOT/lib_boards.sh"
IP=${IP:-$(server_ip)}   # IP=<ip> forces it
RUNS=$(cat)
[ -n "$RUNS" ] || { echo "no runs on stdin"; exit 1; }
OUT="$ROOT/log_run/_sweep_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUT"

# the Zephyr workspace (west, esptool, Python modules): the folder that contains this repository unless told otherwise
WS=${ZEPHYR_WORKSPACE:-$(dirname "$ROOT")}
if [ ! -f "$WS/.venv/bin/activate" ] || [ ! -f "$WS/zephyr/zephyr-env.sh" ]; then
    echo "Zephyr workspace not found in $WS (needs .venv and zephyr/): put the repository inside it or set ZEPHYR_WORKSPACE; see ./check_server_env.sh"
    exit 1
fi
source "$WS/.venv/bin/activate"
source "$WS/zephyr/zephyr-env.sh"
cd "$ROOT"

KEYS="HIDDEN_NEURONS HIDDEN_LAYERS WEIGHT_INIT_SEED KD_ALPHA KD_TEMPERATURE KD_STUDENT_HINT_LAYER TEACHER_ARCHITECTURE TEACHER_TIMEOUT_S TEACHER_EPOCHS TEACHER_ALPHA TEACHER_HIDDEN_LAYERS TEACHER_HIDDEN_NEURONS TEACHER_ENCODER_LAYERS TEACHER_DECODER_LAYERS TEACHER_LAMBDA CLIENTS_NUM"
declare -A ORIGINAL
for k in $KEYS; do ORIGINAL[$k]=$(grep -m1 "^#define $k " "$CFG"); done

# change only the value of a #define, keeping the trailing comment
setdef() {
    python3 -I - "$CFG" "$1" "$2" <<'PY'
import re, sys
path, key, value = sys.argv[1:4]
text = open(path).read()
new, n = re.subn(rf"^(#define {key} )\S+", lambda m: m.group(1) + value, text, flags=re.M)
assert n == 1, key
open(path, "w").write(new)
PY
}

# put a whole saved #define line back
restoreline() {
    python3 -I - "$CFG" "$1" "$2" <<'PY'
import re, sys
path, key, line = sys.argv[1:4]
text = open(path).read()
new, n = re.subn(rf"^#define {key} .*$", lambda m: line, text, flags=re.M)
assert n == 1, key
open(path, "w").write(new)
PY
}

restore() {
    "$ROOT/datasets-full-testes/use_dataset.sh" "$RESTORE_DATASET"
    for k in $KEYS; do restoreline "$k" "${ORIGINAL[$k]}"; done
    echo "[$(date +%T)] restored: dataset $RESTORE_DATASET and $(for k in $KEYS; do printf '%s; ' "$(grep -m1 "^#define $k " "$CFG" | sed 's/ *\/\/.*//')"; done)"
}
trap restore EXIT

# patterns anchored to the exact command lines run_zephyr.sh starts
cleanup() {
    pkill -f "^bash run_zephyr\.sh"
    pkill -f "^\./FederatedLearningServer"
    pkill -f "^FederatedLearningTeacherClient/FederatedLearningTeacherClient"
    for p in $(lsof -t /dev/ttyUSB* 2>/dev/null); do kill "$p"; done
    wait_ports_free
}

# the server binds without SO_REUSEADDR: wait until no socket on its ports is left, TIME_WAIT included
wait_ports_free() {
    for _ in $(seq 180); do
        ss -tan | awk '{print $4}' | grep -qE ":(8888|8080)$" || return 0
        sleep 1
    done
    echo "[$(date +%T)] warning: ports 8888/8080 still busy"
}

"$ROOT/datasets-full-testes/use_dataset.sh" "$DATASET" || exit 1
setdef HIDDEN_NEURONS "$STUDENT_NEURONS"
setdef HIDDEN_LAYERS "$STUDENT_LAYERS"
setdef WEIGHT_INIT_SEED "$SEED"
[ -n "$KD_TEMPERATURE" ] && setdef KD_TEMPERATURE "$KD_TEMPERATURE"
[ -n "$TEACHER_TIMEOUT_S" ] && setdef TEACHER_TIMEOUT_S "$TEACHER_TIMEOUT_S"
[ -n "$TEACHER_EPOCHS" ] && setdef TEACHER_EPOCHS "$TEACHER_EPOCHS"
[ -n "$TEACHER_ALPHA" ] && setdef TEACHER_ALPHA "$TEACHER_ALPHA"
[ -n "$TEACHER_HIDDEN_LAYERS" ] && setdef TEACHER_HIDDEN_LAYERS "$TEACHER_HIDDEN_LAYERS"
[ -n "$TEACHER_HIDDEN_NEURONS" ] && setdef TEACHER_HIDDEN_NEURONS "$TEACHER_HIDDEN_NEURONS"
[ -n "$TEACHER_LAMBDA" ] && setdef TEACHER_LAMBDA "$TEACHER_LAMBDA"
[ -n "$TEACHER_ENCODER_LAYERS" ] && setdef TEACHER_ENCODER_LAYERS "$TEACHER_ENCODER_LAYERS"
[ -n "$TEACHER_DECODER_LAYERS" ] && setdef TEACHER_DECODER_LAYERS "$TEACHER_DECODER_LAYERS"
echo "[$(date +%T)] sweep: dataset=$DATASET teacher=$TEACHER_FRAC student=${STUDENT_LAYERS}x${STUDENT_NEURONS} seed=$SEED ip=$IP out=$OUT"
echo "[$(date +%T)] $(grep -E '^#define (INPUT_FEATURES|OUTPUT_CLASSES|EVAL_SAMPLES|HIDDEN_LAYERS|HIDDEN_NEURONS|WEIGHT_INIT_SEED|KD_TEMPERATURE|TEACHER_EPOCHS|TEACHER_ALPHA|TEACHER_HIDDEN_LAYERS|TEACHER_HIDDEN_NEURONS|TEACHER_TIMEOUT_S)' "$CFG" | sed 's/ *\/\/.*//' | tr '\n' ' ')"
: > "$OUT/runs.txt"

while IFS=: read -r label kd arch alpha layer; do
    [ -n "$label" ] || continue
    setdef KD_ALPHA "${alpha}f"
    setdef TEACHER_ARCHITECTURE "$arch"
    setdef KD_STUDENT_HINT_LAYER "$layer"

    # a run that finishes but is not usable (a node missing in some round, a board without the teacher, fewer rounds
    # than ITERATIONS: see check_run.sh) is deleted and repeated, up to SWEEP_REDO extra times
    for redo in $(seq 0 "$SWEEP_REDO"); do
        echo "[$(date +%T)] $label: starting (KD=$kd $arch alpha=$alpha hint layer=$layer)${redo:+ attempt $((redo + 1))}"

        for attempt in 1 2 3; do
            wait_ports_free
            before=$(ls log_run | grep '^run_')
            echo "$IP" | KD=$kd bash run_zephyr.sh "$TEACHER_FRAC" > "$OUT/$label.try$attempt.out" 2>&1 &
            script_pid=$!
            ln -sf "$label.try$attempt.out" "$OUT/$label.out"   # $label.out always points to the latest attempt

            run=""
            for _ in $(seq 900); do
                run=$(comm -13 <(echo "$before" | sort) <(ls log_run | grep '^run_' | sort) | head -1)
                [ -n "$run" ] && break
                sleep 1
            done
            echo "[$(date +%T)] $label: run=$run"

            status=timeout
            start=$(date +%s)
            while [ $(( $(date +%s) - start )) -lt "$RUN_TIMEOUT_S" ]; do
                if [ -n "$run" ] && grep -q "max interation" "log_run/$run/server.log" 2>/dev/null; then
                    status=ok; break
                fi
                if ! kill -0 "$script_pid" 2>/dev/null; then
                    status=script_failed; break
                fi
                sleep 15
            done
            # a run that dies within 5 minutes is a transient setup failure: retry it
            if [ "$status" = script_failed ] && [ $(( $(date +%s) - start )) -lt 300 ]; then
                echo "[$(date +%T)] $label: attempt $attempt failed early, retrying"
                cleanup
                continue
            fi
            break
        done

        echo "$label $run $status" >> "$OUT/runs.txt"
        echo "[$(date +%T)] $label: $status"
        cleanup
        python3 -I "$ROOT/catalog_runs.py" "$ROOT"

        if [ "$status" = ok ]; then
            verdict=$("$ROOT/check_run.sh" "$run" "$kd"); valid=$?
        else
            verdict="run did not finish ($status)"; valid=1
        fi
        echo "[$(date +%T)] $label: check: $(echo "$verdict" | tr '\n' ';')"
        echo "$label $run check: $verdict" | tr '\n' ' ' >> "$OUT/runs.txt"; echo >> "$OUT/runs.txt"
        [ "$valid" -eq 0 ] && break

        if [ "$redo" -lt "$SWEEP_REDO" ]; then
            echo "[$(date +%T)] $label: invalid, deleting $run and repeating ($((redo + 1))/$SWEEP_REDO)"
            if [ -n "$run" ]; then
                for d in "log_run/${run:?}" "FederatedLearningServer/modelos/${run:?}" "FederatedLearningServer/resultados/${run:?}"; do
                    [ -d "$d" ] && rm -rf "${PWD:?}/$d"
                done
                python3 -I "$ROOT/catalog_runs.py" "$ROOT"
            fi
        else
            echo "[$(date +%T)] $label: still invalid after $SWEEP_REDO repeats, keeping the last attempt ($run); see obs in the catalog"
        fi
    done
done <<< "$RUNS"
