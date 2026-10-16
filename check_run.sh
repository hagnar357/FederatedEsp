#!/bin/bash
# Checks that a finished run is usable for comparison: every round aggregated all the nodes, the run reached ITERATIONS
# rounds and (KD=1) no board trained without the teacher.
# usage: check_run.sh <run id> <KD 0|1>      exit 0 = valid, 1 = invalid (reasons on stdout)
ROOT=$(cd "$(dirname "$0")" && pwd)
RUN=$1; KD=${2:-0}
CFG="$ROOT/FederatedLearningServer/lib/configs.h"
LOG="$ROOT/log_run/$RUN/server.log"
[ -f "$LOG" ] || { echo "no server.log"; exit 1; }
ITER=$(awk '$1=="#define" && $2=="ITERATIONS" {print $3; exit}' "$CFG")
NODES=$(awk '$1=="#define" && $2=="CLIENTS_NUM" {print $3; exit}' "$CFG")
bad=0
rounds=$(grep -c "Calculating the new global model" "$LOG")
short=$(grep "Calculating the new global model" "$LOG" | grep -vc "($NODES models)")
timeouts=$(grep -c "timeout, missing nodes" "$LOG")
[ "$rounds" -ge "$ITER" ] || { echo "only $rounds of $ITER rounds"; bad=1; }
[ "$short" -eq 0 ] || { echo "$short round(s) aggregated fewer than $NODES models"; bad=1; }
[ "$timeouts" -eq 0 ] || { echo "$timeouts round(s) ended by timeout with a missing node"; bad=1; }
if [ "$KD" = 1 ]; then
    grep -q "starting federated training without teacher" "$LOG" && { echo "server ran without the teacher (timeout)"; bad=1; }
    notea=$(cat "$ROOT/log_run/$RUN"/clients/*.log 2>/dev/null | grep -cE "sem teacher: treino sem destila|Teacher indispon")
    [ "$notea" -eq 0 ] || { echo "a board trained without the teacher ($notea log line(s))"; bad=1; }
fi
[ "$bad" -eq 0 ] && echo "valid ($rounds rounds, all $NODES nodes)"
exit $bad
