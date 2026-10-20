#!/bin/bash
# Tests every board connected by USB: flashes the firmware, lets the boards join the Wi-Fi, and checks with a short
# real federated session (the simulated teacher runs on this computer) that each one connects, receives the global
# model, sends its local model and that the server receives it. No internet is needed (hotspot of this computer is fine).
#
# usage: ./test_boards.sh [SSID PASSWORD] [ROUNDS]
#   SSID PASSWORD  Wi-Fi the boards must join (2.4 GHz). Without them the ones in espconfiguration.h are used.
#                  This computer must already be on that network (or be its hotspot).
#   ROUNDS         federated rounds to wait for (default 2)
# env: SERVER_IP=<ip>      IP of this computer on that network (default: the IPv4 of the Wi-Fi interface)
#      BOARD_TYPE=esp32|s3 treat every board as that kind (default: each board is detected with esptool, and ESP32-S3 and
#                          classic ESP32 boards can be plugged at the same time)
#      WAIT_S=1500         give up after this many seconds and report what happened
#      DRY_RUN=1           only list the boards, the IP and the network, change nothing
# The firmware header and the server configs are restored at the end. The run folder stays in log_run/ (it is a test run:
# delete it when you have read it); the table is also saved in log_run/<run>/teste_placas.txt.
cd "$(dirname "$0")" || exit 1
ROOT=$PWD
HDR=FederatedLearningZephyr/include/espconfiguration.h
SSID=$1; PASS=$2; ROUNDS=${3:-2}
[ -n "$SSID" ] && [ -z "$PASS" ] && { echo "usage: $0 [SSID PASSWORD] [ROUNDS]"; exit 1; }
WAIT_S=${WAIT_S:-1500}

CHECK_SKIP_WIFI=1 ./check_server_env.sh --quiet || { echo "this computer is not ready (see above); full report: ./check_server_env.sh"; exit 1; }

# the boards, in the order the run scripts use them (glob order = node index no_0, no_1, ...)
PORTS=(/dev/ttyUSB*)
[ -e "${PORTS[0]}" ] || { echo "no board in /dev/ttyUSB*"; exit 1; }
WS=${ZEPHYR_WORKSPACE:-$(dirname "$ROOT")}
source "$WS/.venv/bin/activate"
source "$WS/zephyr/zephyr-env.sh" > /dev/null 2>&1
source ./lib_boards.sh
declare -a MACS TYPES
for i in "${!PORTS[@]}"; do
    MACS[$i]=$(timeout 40 esptool --port "${PORTS[$i]}" flash-id 2>&1 | awk '/^MAC:/ {print $2; exit}')
    TYPES[$i]=${BOARD_TYPE:-$(board_detect_type "${PORTS[$i]}")}
    [ "${TYPES[$i]}" = auto ] && TYPES[$i]=$(board_detect_type "${PORTS[$i]}")
    case "${TYPES[$i]}" in s3|esp32) ;; *) echo "could not read the chip of ${PORTS[$i]} (${TYPES[$i]}); set BOARD_TYPE=s3 or esp32, and unplug broken boards"; exit 1 ;; esac
done
# S3 and classic ESP32 boards can be mixed: run_zephyr.sh builds one firmware per kind. A forced BOARD_TYPE applies to all.
[ -n "$BOARD_TYPE" ] && [ "$BOARD_TYPE" != auto ] && export BOARD_TYPE

IP=${SERVER_IP:-$(ip -4 -o addr show 2> /dev/null | awk '$2 ~ /^(wl|ap)/ {split($4, a, "/"); print a[1]; exit}')}
[ -n "$IP" ] || { echo "no IPv4 address on a Wi-Fi interface: join the network (or start the hotspot) first, or set SERVER_IP"; exit 1; }
CUR_SSID=$(awk '$2=="WIFI_SSID" {gsub(/"/, "", $3); print $3}' "$HDR")
echo "boards: ${#PORTS[@]} ($(printf '%s\n' "${TYPES[@]}" | sort | uniq -c | awk '{printf "%s x %s  ", $1, $2}'))"
for i in "${!PORTS[@]}"; do echo "  ${PORTS[$i]}  ${MACS[$i]:-?}  ${TYPES[$i]}"; done
echo "this computer: $IP   Wi-Fi of the boards: ${SSID:-$CUR_SSID}${SSID:+ (given)}   rounds to wait: $ROUNDS"
[ -n "$DRY_RUN" ] && exit 0

# change the firmware header for this test and put it back at the end, whatever happens
cp "$HDR" /tmp/espconfiguration.h.test_boards.$$
cleanup() {
    for p in $(ps -eo pid,args | grep -E "^ *[0-9]+ +/bin/bash \./run_sweep" | awk '{print $1}'); do kill "$p" 2> /dev/null; done
    sleep 4
    for p in $(ps -eo pid,args | grep -E "^ *[0-9]+ +(bash run_zephyr|\./FederatedLearningServer|FederatedLearningTeacherClient/Fed)" | awk '{print $1}'); do kill "$p" 2> /dev/null; done
    for p in $(lsof -t /dev/ttyUSB* 2> /dev/null); do kill "$p" 2> /dev/null; done
    cp /tmp/espconfiguration.h.test_boards.$$ "$HDR" && rm -f /tmp/espconfiguration.h.test_boards.$$
    sed -i 's/^#define CLIENTS_NUM .*/#define CLIENTS_NUM 3/' FederatedLearningServer/lib/configs.h
}
trap cleanup EXIT
if [ -n "$SSID" ]; then
    python3 -I - "$HDR" "$SSID" "$PASS" <<'PY'
import re, sys
p, ssid, pw = sys.argv[1:4]
t = open(p).read()
esc = lambda s: s.replace("\\", "\\\\").replace('"', '\\"')
t = re.sub(r'^#define WIFI_SSID .*$', lambda m: '#define WIFI_SSID       "%s"' % esc(ssid), t, flags=re.M)
t = re.sub(r'^#define WIFI_PASSWORD .*$', lambda m: '#define WIFI_PASSWORD   "%s"' % esc(pw), t, flags=re.M)
open(p, "w").write(t)
PY
fi

OUT=$(mktemp)
echo "[$(date +%T)] flashing and starting the session (the boards are flashed one by one, it takes a few minutes)..."
DATASET=covertype-17220 TEACHER_FRAC=0.7 STUDENT_NEURONS=4 SEED=42 KD_TEMPERATURE=2.0f TEACHER_EPOCHS=5 \
TEACHER_ALPHA=0.001 TEACHER_HIDDEN_LAYERS=3 TEACHER_HIDDEN_NEURONS=12 TEACHER_LAMBDA=0.0 SWEEP_REDO=0 \
RUN_TIMEOUT_S=$((WAIT_S + 600)) IP=$IP \
    bash -c 'printf "teste-placas:0:TEACHER_ARCH_CLASSIFIER:0.2:1\n" | ./run_sweep.sh' > "$OUT" 2>&1 &
for _ in $(seq 90); do grep -q "run=run_" "$OUT" && break; sleep 2; done
# the sweep retries a run that fails in the first minutes (new run folder each time): always use the newest one
newest_run() { ls -t log_run | grep '^run_' | head -1; }
RUN=$(newest_run)
[ -n "$RUN" ] || { echo "the run did not start; output:"; tail -20 "$OUT"; exit 1; }
echo "[$(date +%T)] run $RUN (waiting up to $((WAIT_S / 60)) min for $ROUNDS round(s))"

start=$(date +%s)
while [ $(( $(date +%s) - start )) -lt "$WAIT_S" ]; do
    RUN=$(newest_run); LOG=log_run/$RUN
    closed=$(grep -c 'Calculating the new global model' "$LOG/server.log" 2> /dev/null); [ "${closed:-0}" -ge "$ROUNDS" ] && break
    grep -q "script_failed" "$OUT" && break
    sleep 10
done
RUN=$(newest_run); LOG=log_run/$RUN

REPORT=$LOG/teste_placas.txt
python3 -I ./board_report.py "$LOG" "$IP" "${PORTS[*]}" "${MACS[*]}" "${TYPES[*]}" | tee "$REPORT"
echo "(table saved in $REPORT; logs of each board in $LOG/clients/)"
