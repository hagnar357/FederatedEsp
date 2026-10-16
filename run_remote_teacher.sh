#!/bin/bash
# Runs the teacher node on THIS computer for a server/boards running on another one (the server side starts its runs
# with TEACHER_REMOTE=1, e.g.  TEACHER_REMOTE=1 ./run_sweep.sh  or  TEACHER_REMOTE=1 ./run_zephyr.sh 0.5).
#
# usage:  ./run_remote_teacher.sh <server ip>
# Loop, one pass per run of the server side:
#   1. waits until the server (HTTP port) and its dataset server answer;
#   2. downloads this run's teacher slice (dataset_no_0.csv) and the KD flag (teacher_remote.args: empty or --no-kd);
#   3. runs FederatedLearningTeacherClient until the server goes away (run finished), then goes back to 1.
# Start it BEFORE the server side starts the runs (it only joins a run whose server it has seen start).
# Needs: bash, curl, gcc, make, libwebsockets-dev, this repository at the same version as the server side,
# and network access to the server's ports 8888 (HTTP), 8080 (WebSocket) and 8000 (dataset server).
# Optional env: API_PORT=8888 WS_PORT=8080 DATA_PORT=8000 DOWN_SECONDS=30 BIN=<client binary>
SERVER="$1"
[ -n "$SERVER" ] || { echo "usage: $0 <server ip>"; exit 1; }
API_PORT=${API_PORT:-8888}
WS_PORT=${WS_PORT:-8080}
DATA_PORT=${DATA_PORT:-8000}
DOWN_SECONDS=${DOWN_SECONDS:-30}
ROOT=$(cd "$(dirname "$0")" && pwd)
BIN=${BIN:-$ROOT/FederatedLearningTeacherClient/FederatedLearningTeacherClient}
WORK=$(mktemp -d)
LOGDIR="$ROOT/log_teacher_remoto"
mkdir -p "$LOGDIR"
CLIENT_PID=""
trap '[ -n "$CLIENT_PID" ] && kill "$CLIENT_PID" 2>/dev/null; rm -rf "${WORK:?}"; exit 0' INT TERM EXIT

if [ ! -x "$BIN" ] || [ -n "$REBUILD" ]; then
    echo "[$(date +%T)] building the teacher client..."
    mkdir -p "$ROOT/FederatedLearningTeacherClient/.build"
    make -C "$ROOT/FederatedLearningTeacherClient" build > "$WORK/build.log" 2>&1 || { cat "$WORK/build.log"; exit 1; }
fi

port_open() { timeout 2 bash -c "exec 3<>/dev/tcp/$SERVER/$1" 2>/dev/null; }

n=0
while true; do
    echo "[$(date +%T)] waiting for the server $SERVER:$API_PORT and its dataset server :$DATA_PORT ..."
    until port_open "$API_PORT" && port_open "$DATA_PORT"; do sleep 3; done

    # the datasets are regenerated for every run, before the server starts; fetch them now
    if ! curl -fsS --max-time 60 -o "$WORK/dataset_no_0.csv" "http://$SERVER:$DATA_PORT/dataset_no_0.csv" ||
       ! curl -fsS --max-time 10 -o "$WORK/teacher_remote.args" "http://$SERVER:$DATA_PORT/teacher_remote.args"; then
        echo "[$(date +%T)] could not download the dataset, trying again"
        sleep 5
        continue
    fi
    n=$((n + 1))
    ARGS=$(cat "$WORK/teacher_remote.args")
    echo "[$(date +%T)] run $n: $(wc -l < "$WORK/dataset_no_0.csv") teacher rows, args: '${ARGS}'"

    LOGFILE="$LOGDIR/run${n}_$(date +%Y%m%d_%H%M%S).log"
    # shellcheck disable=SC2086
    "$BIN" --server "$SERVER" --port "$API_PORT" --wsport "$WS_PORT" --dataset "$WORK/dataset_no_0.csv" $ARGS \
        > "$LOGFILE" 2>&1 &
    CLIENT_PID=$!
    echo "[$(date +%T)] teacher client started (pid $CLIENT_PID), log: $LOGFILE"

    # the run is over when the server's HTTP port stays closed for DOWN_SECONDS (it is only up during a run)
    down=0
    while kill -0 "$CLIENT_PID" 2>/dev/null; do
        if port_open "$API_PORT"; then down=0; else down=$((down + 3)); fi
        [ "$down" -ge "$DOWN_SECONDS" ] && break
        sleep 3
    done
    kill "$CLIENT_PID" 2>/dev/null
    wait "$CLIENT_PID" 2>/dev/null
    CLIENT_PID=""
    echo "[$(date +%T)] run $n finished (server gone), teacher client stopped"
    sleep 5
done
