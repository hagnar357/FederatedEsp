#!/bin/bash
# Runs the whole test matrix (23 runs) on the boards, with the teacher node on ANOTHER computer.
#
#   on the teacher computer (first, and keep it open):  ./run_remote_teacher.sh <IP of this computer>
#   on this computer:                                   ./run_all_remote.sh
#
# Matrix (covertype 17220-row pool, logit KD, T=2, teacher 3x12 lr 0.001 / 500 epochs, student 54-4-4-7, seed 42 by default, SEED=<n> to change it):
#   30% and 50% of the dataset in the teacher: no-KD baseline + alpha 0.1 .. 0.6   (2 x 7 = 14 runs)
#   40%, 60% and 70%:                          no-KD baseline + alpha 0.2 and 0.4  (3 x 3 =  9 runs)
# Every run is checked (check_run.sh) and repeated automatically up to SWEEP_REDO=2 times if it is incomplete.
#
# Boards: the chip of each board on /dev/ttyUSB* is detected with esptool (BOARD_TYPE=auto, the default), and ESP32-S3
# (Heltec V3) and classic ESP32 boards can be plugged AT THE SAME TIME: run_zephyr.sh builds one firmware per kind and
# flashes each board with its own (the dataset image is the same for both). Minimum: 2 boards if all are S3, else 1.
# BOARD_TYPE=s3|esp32 skips the detection and treats every board as that kind.
# A single board receives all the data that is not in the teacher's slice (a pair splits it in two), so results with
# different numbers of boards are not comparable. The classic ESP32 has 80 KB of heap (170 KB on the S3): it runs the
# student 54-4-4-7 and the teacher 3x12 only because the model transfers use the flat format.
#
# usage: ./run_all_remote.sh [fraction ...]     e.g. ./run_all_remote.sh 0.7      (default: 0.3 0.5 0.4 0.6 0.7)
#        DRY_RUN=1 ./run_all_remote.sh          only prints the plan (it still reads the chip of the boards)
#        BOARD_TYPE=esp32 ./run_all_remote.sh 0.7        (force one kind for all boards)
cd "$(dirname "$0")" || exit 1

# keep the notebook awake while the runs are going (it is re-executed once under systemd-inhibit)
if [ -z "$FL_INHIBITED" ] && [ -z "$DRY_RUN" ] && command -v systemd-inhibit > /dev/null; then
    FL_INHIBITED=1 exec systemd-inhibit --what=sleep:idle:handle-lid-switch --who="FL tests" \
        --why="queue of hardware tests running" "$0" "$@"
fi

FRACTIONS=("$@")
[ ${#FRACTIONS[@]} -gt 0 ] || FRACTIONS=(0.3 0.5 0.4 0.6 0.7)

# SEED seeds the weight init AND the split of the data between the nodes (DATA_SEED); SEED=23 ./run_all_remote.sh repeats the matrix
SEED=${SEED:-42}
export DATASET=covertype-17220 STUDENT_NEURONS=4 SEED KD_TEMPERATURE=2.0f DATA_SEED=$SEED
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

# stop at once, with the list of what is missing, if this computer cannot run the boards side (Zephyr workspace,
# west, esptool, Python modules, serial access...): otherwise every run fails right at the start
./check_server_env.sh --quiet || { echo "this computer is not ready for the runs (see above); full report: ./check_server_env.sh"; exit 1; }

# which boards: the kind of each one (S3 and classic ESP32 can be plugged at the same time; run_zephyr.sh builds one
# firmware per kind and flashes each board with its own). BOARD_TYPE=s3|esp32 forces one kind for all of them.
source "${ZEPHYR_WORKSPACE:-$(dirname "$PWD")}/.venv/bin/activate"
source ./lib_boards.sh
BOARD_TYPE=${BOARD_TYPE:-auto}
ports=(/dev/ttyUSB*)
[ -e "${ports[0]}" ] || { echo "no board in /dev/ttyUSB*: plug the boards in first"; exit 1; }
boards=${#ports[@]}
n_s3=0; n_esp32=0
if [ "$BOARD_TYPE" = auto ]; then
    for p in "${ports[@]}"; do
        case "$(board_detect_type "$p")" in
            s3)    n_s3=$((n_s3 + 1)) ;;
            esp32) n_esp32=$((n_esp32 + 1)) ;;
            *)     echo "could not read the chip of $p; set BOARD_TYPE=s3 or esp32, or unplug that board"; exit 1 ;;
        esac
    done
else
    case "$BOARD_TYPE" in
        s3)    n_s3=$boards ;;
        esp32) n_esp32=$boards ;;
        *)     echo "BOARD_TYPE must be auto, s3 or esp32 (got $BOARD_TYPE)"; exit 1 ;;
    esac
fi
export BOARD_TYPE
# two boards at least when all of them are S3 (the pair splits the data); one is enough as soon as there is a classic ESP32
if [ "$n_esp32" -eq 0 ]; then MIN_BOARDS=${MIN_BOARDS:-2}; else MIN_BOARDS=${MIN_BOARDS:-1}; fi
composition="$n_s3 x s3, $n_esp32 x esp32"

total=0
for f in "${FRACTIONS[@]}"; do total=$((total + $(runs_for "$f" | wc -l))); done
IP=${IP:-$(server_ip)}   # IP=<ip> ./run_all_remote.sh forces it
export IP
echo "plan: $total runs, fractions: ${FRACTIONS[*]}, teacher on another computer, this computer: ${IP:-?} (the boards, the server and the teacher must all be on this IP's network)"
echo "boards: $boards ($composition; one firmware per kind), minimum $MIN_BOARDS"
echo "(the teacher computer must be running: ./run_remote_teacher.sh ${IP:-<this IP>})"
if [ -n "$DRY_RUN" ]; then
    for f in "${FRACTIONS[@]}"; do echo "== teacher $f"; runs_for "$f"; done
    exit 0
fi

[ "$boards" -ge "$MIN_BOARDS" ] || { echo "only $boards board(s) in /dev/ttyUSB*, $MIN_BOARDS needed ($composition): plug them in first"; exit 1; }

for f in "${FRACTIONS[@]}"; do
    echo "[$(date +%T)] === teacher with $f of the dataset ==="
    runs_for "$f" | TEACHER_FRAC=$f ./run_sweep.sh
done
echo "[$(date +%T)] all done"
