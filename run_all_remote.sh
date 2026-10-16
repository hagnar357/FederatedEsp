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
# usage: ./run_all_remote.sh [fraction ...]     e.g. ./run_all_remote.sh 0.7      (default: 0.3 0.5 0.4 0.6 0.7)
#        DRY_RUN=1 ./run_all_remote.sh          only prints the plan
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

total=0
for f in "${FRACTIONS[@]}"; do total=$((total + $(runs_for "$f" | wc -l))); done
IP=$(ip route get 1.1.1.1 2>/dev/null | awk '{for (i = 1; i <= NF; i++) if ($i == "src") print $(i + 1)}')
echo "plan: $total runs, fractions: ${FRACTIONS[*]}, teacher on another computer, this computer: ${IP:-?}"
echo "(the teacher computer must be running: ./run_remote_teacher.sh ${IP:-<this IP>})"
if [ -n "$DRY_RUN" ]; then
    for f in "${FRACTIONS[@]}"; do echo "== teacher $f"; runs_for "$f"; done
    exit 0
fi

boards=$(ls /dev/ttyUSB* 2> /dev/null | wc -l)
[ "$boards" -ge 2 ] || { echo "only $boards board(s) in /dev/ttyUSB*: plug both boards in first"; exit 1; }

for f in "${FRACTIONS[@]}"; do
    echo "[$(date +%T)] === teacher with $f of the dataset ==="
    runs_for "$f" | TEACHER_FRAC=$f ./run_sweep.sh
done
echo "[$(date +%T)] all done"
