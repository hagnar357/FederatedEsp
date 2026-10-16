#!/bin/bash
# Runs the whole test matrix (23 runs) on the boards, with the teacher node on ANOTHER computer.
#
#   on the teacher computer (first, and keep it open):  ./run_remote_teacher.sh <IP of this computer>
#   on this computer:                                   ./run_all_remote.sh
#
# Matrix (covertype 17220-row pool, logit KD, T=2, teacher 3x12 lr 0.001 / 500 epochs, student 54-4-4-7, seed 42):
#   30% and 50% of the dataset in the teacher: no-KD baseline + alpha 0.1 .. 0.6   (2 x 7 = 14 runs)
#   40%, 60% and 70%:                          no-KD baseline + alpha 0.2 and 0.4  (3 x 3 =  9 runs)
# Every run is checked (check_run.sh) and repeated automatically up to SWEEP_REDO=2 times if it is incomplete.
#
# Boards: the chip of the boards on /dev/ttyUSB* is detected with esptool (BOARD_TYPE=auto, the default):
#   ESP32-S3 (Heltec V3)  -> firmware for heltec_wireless_stick_lite_v3, at least 2 boards (MIN_BOARDS=2)
#   ESP32 (classic)       -> firmware for esp32_devkitc with esp32.overlay, at least 1 board (MIN_BOARDS=1)
# BOARD_TYPE=s3|esp32 skips the detection. All the boards must be of the same type (one firmware per run).
# With a single board it receives all the data that is not in the teacher's slice (the S3 pair splits it in two), so
# those results are not comparable with the 2-board runs. The classic ESP32 has 80 KB of heap (170 KB on the S3):
# it runs the student 54-4-4-7 and the teacher 3x12 only because the model transfers use the flat format.
#
# usage: ./run_all_remote.sh [fraction ...]     e.g. ./run_all_remote.sh 0.7      (default: 0.3 0.5 0.4 0.6 0.7)
#        DRY_RUN=1 ./run_all_remote.sh          only prints the plan (it still reads the chip of the boards)
#        BOARD_TYPE=esp32 MIN_BOARDS=1 ./run_all_remote.sh 0.7
cd "$(dirname "$0")" || exit 1

# keep the notebook awake while the runs are going (it is re-executed once under systemd-inhibit)
if [ -z "$FL_INHIBITED" ] && [ -z "$DRY_RUN" ] && command -v systemd-inhibit > /dev/null; then
    FL_INHIBITED=1 exec systemd-inhibit --what=sleep:idle:handle-lid-switch --who="FL tests" \
        --why="queue of hardware tests running" "$0" "$@"
fi

FRACTIONS=("$@")
[ ${#FRACTIONS[@]} -gt 0 ] || FRACTIONS=(0.3 0.5 0.4 0.6 0.7)

export DATASET=covertype-17220 STUDENT_NEURONS=4 SEED=42 KD_TEMPERATURE=2.0f
export TEACHER_TIMEOUT_S=3600 TEACHER_EPOCHS=500 TEACHER_ALPHA=0.001 TEACHER_LAMBDA=0.0
export TEACHER_HIDDEN_LAYERS=3 TEACHER_HIDDEN_NEURONS=12 RUN_TIMEOUT_S=9000
export TEACHER_REMOTE=1

runs_for() {   # runs of one fraction, one "label:KD:arch:alpha:hint layer" per line
    local f=$1 alphas
    case "$f" in
        0.3|0.5) alphas="0.2 0.4 0.1 0.3 0.5 0.6" ;;
        *)       alphas="0.2 0.4" ;;
    esac
    echo "c$f-semKD:0:TEACHER_ARCH_CLASSIFIER:0.5:1"
    for a in $alphas; do echo "c$f-logit$a:1:TEACHER_ARCH_CLASSIFIER:$a:1"; done
}

# which firmware: detect the chip of every board with esptool unless BOARD_TYPE says it
detect_chip() {   # prints esp32s3 | esp32 | unknown for one serial port
    local out
    out=$(source /home/flips/zephyrproject/.venv/bin/activate && esptool --port "$1" flash-id 2>&1 | grep -m1 "Chip type")
    case "$out" in
        *ESP32-S3*) echo esp32s3 ;;
        *ESP32*)    echo esp32 ;;
        *)          echo unknown ;;
    esac
}

BOARD_TYPE=${BOARD_TYPE:-auto}
ports=$(ls /dev/ttyUSB* 2> /dev/null)
boards=$(echo "$ports" | grep -c .)
if [ "$BOARD_TYPE" = auto ]; then
    types=""
    for p in $ports; do types="$types $(detect_chip "$p")"; done
    uniq_types=$(echo $types | tr ' ' '\n' | sort -u | tr '\n' ' ')
    case "$uniq_types" in
        "esp32s3 ") BOARD_TYPE=s3 ;;
        "esp32 ")   BOARD_TYPE=esp32 ;;
        "")         echo "no board in /dev/ttyUSB*: plug the boards in first"; exit 1 ;;
        *)          echo "could not tell the board type (detected: ${types:-none}); set BOARD_TYPE=s3 or esp32"; exit 1 ;;
    esac
fi
case "$BOARD_TYPE" in
    s3)    MIN_BOARDS=${MIN_BOARDS:-2} ;;
    esp32) MIN_BOARDS=${MIN_BOARDS:-1}
           export BOARD=esp32_devkitc/esp32/procpu CHIP=esp32 OVERLAY=esp32.overlay ;;
    *)     echo "BOARD_TYPE must be auto, s3 or esp32 (got $BOARD_TYPE)"; exit 1 ;;
esac

total=0
for f in "${FRACTIONS[@]}"; do total=$((total + $(runs_for "$f" | wc -l))); done
IP=$(ip route get 1.1.1.1 2>/dev/null | awk '{for (i = 1; i <= NF; i++) if ($i == "src") print $(i + 1)}')
echo "plan: $total runs, fractions: ${FRACTIONS[*]}, teacher on another computer, this computer: ${IP:-?}"
echo "boards: $boards x $BOARD_TYPE (minimum $MIN_BOARDS)${BOARD:+, firmware $BOARD with $OVERLAY}"
echo "(the teacher computer must be running: ./run_remote_teacher.sh ${IP:-<this IP>})"
if [ -n "$DRY_RUN" ]; then
    for f in "${FRACTIONS[@]}"; do echo "== teacher $f"; runs_for "$f"; done
    exit 0
fi

[ "$boards" -ge "$MIN_BOARDS" ] || { echo "only $boards board(s) in /dev/ttyUSB*, $MIN_BOARDS needed for $BOARD_TYPE: plug them in first"; exit 1; }

for f in "${FRACTIONS[@]}"; do
    echo "[$(date +%T)] === teacher with $f of the dataset ==="
    runs_for "$f" | TEACHER_FRAC=$f ./run_sweep.sh
done
echo "[$(date +%T)] all done"
