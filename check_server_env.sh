#!/bin/bash
# Checks that THIS computer can be the server/boards side of the tests (run_all_remote.sh, run_sweep.sh, run_zephyr.sh):
# the Zephyr workspace around the repository, the toolchain, the Python modules, the serial access and the ports.
# The teacher computer (run_remote_teacher.sh) only needs gcc, make, libwebsockets-dev and curl.
#
# usage: ./check_server_env.sh [--quiet]     exit 0 = nothing missing, 1 = something that stops the runs is missing
#   --quiet: print only the problems
#   ZEPHYR_WORKSPACE=<dir>: the west workspace (default: the folder that contains this repository)
ROOT=$(cd "$(dirname "$0")" && pwd)
WS=${ZEPHYR_WORKSPACE:-$(dirname "$ROOT")}
QUIET=0; [ "$1" = "--quiet" ] && QUIET=1
fail=0
ok()   { [ "$QUIET" = 1 ] || echo "  ok    $1"; }
warn() { echo "  AVISO $1"; }
bad()  { echo "  FALTA $1"; fail=1; }

[ "$QUIET" = 1 ] || echo "workspace Zephyr: $WS (repositório em $ROOT)"
if [ -d "$WS/.west" ]; then ok "$WS é um workspace west (.west)"; else
    bad "$WS não tem .west: o run_zephyr.sh faz 'cd ..' e 'west build' a partir da pasta acima do repositório; clone o repositório DENTRO do workspace Zephyr (ex.: ~/zephyrproject/FederatedEdgeComputing) ou defina ZEPHYR_WORKSPACE"
fi
if [ -f "$WS/.venv/bin/activate" ]; then ok "ambiente Python $WS/.venv"; else bad "$WS/.venv/bin/activate (ambiente Python do Zephyr: west, esptool, pyserial, polars, numpy)"; fi
if [ -f "$WS/zephyr/zephyr-env.sh" ]; then ok "$WS/zephyr/zephyr-env.sh"; else bad "$WS/zephyr/zephyr-env.sh (o Zephyr não está em $WS/zephyr)"; fi

# the tools and modules are looked up the way run_sweep.sh does: with the environment of the workspace active
if [ -f "$WS/.venv/bin/activate" ]; then source "$WS/.venv/bin/activate"; fi
if [ -f "$WS/zephyr/zephyr-env.sh" ]; then source "$WS/zephyr/zephyr-env.sh" > /dev/null 2>&1; fi

for tool in west mklittlefs make gcc python3 ss lsof curl awk sed; do
    if command -v "$tool" > /dev/null; then ok "$tool"; else bad "$tool (não está no PATH)"; fi
done
if command -v esptool.py > /dev/null; then ok "esptool.py"; else bad "esptool.py (pip install esptool no ambiente $WS/.venv)"; fi
if command -v esptool > /dev/null; then ok "esptool"; else bad "esptool (pip install esptool; o run_all_remote.sh usa 'esptool flash-id' para detectar o chip)"; fi
for mod in polars numpy serial; do
    if python3 -c "import $mod" 2> /dev/null; then ok "python: $mod"; else bad "python: $mod não importa (pip install $( [ $mod = serial ] && echo pyserial || echo $mod ))"; fi
done
sdk=""
for d in "$ZEPHYR_SDK_INSTALL_DIR" "$HOME"/zephyr-sdk-* "$HOME"/.local/zephyr-sdk-* /opt/zephyr-sdk-*; do
    [ -n "$d" ] && [ -d "$d" ] && sdk=$d && break
done
if [ -n "$sdk" ]; then ok "Zephyr SDK ($sdk)"; else
    bad "Zephyr SDK (nem ZEPHYR_SDK_INSTALL_DIR nem ~/zephyr-sdk-*); o xtensa-espressif_esp32s3_zephyr-elf (S3) ou xtensa-espressif_esp32_zephyr-elf (ESP32) precisa estar instalado"
fi
if [ -f /usr/include/libwebsockets.h ] || pkg-config --exists libwebsockets 2> /dev/null; then ok "libwebsockets-dev (o servidor e o cliente professor compilam com ele)"; else bad "libwebsockets-dev (sudo apt install libwebsockets-dev)"; fi
command -v systemd-inhibit > /dev/null && ok "systemd-inhibit (bloqueia a suspensão)" || warn "systemd-inhibit ausente: o notebook pode suspender durante as runs"

# serial access
if id -nG | tr ' ' '\n' | grep -qx dialout; then ok "usuário no grupo dialout"; else warn "usuário fora do grupo dialout: sem acesso às placas (sudo usermod -aG dialout \$USER e abrir nova sessão)"; fi
ports=$(ls /dev/ttyUSB* 2> /dev/null)
if [ -z "$ports" ]; then
    acm=$(ls /dev/ttyACM* 2> /dev/null)
    if [ -n "$acm" ]; then bad "placas em $(echo $acm) mas o run_zephyr.sh só procura /dev/ttyUSB*: ele falha com \"Nenhuma placa encontrada\" antes de qualquer build"
    else warn "nenhuma placa em /dev/ttyUSB* (o run_zephyr.sh falha com \"Nenhuma placa encontrada\" antes de qualquer build)"; fi
else
    for p in $ports; do [ -r "$p" ] && [ -w "$p" ] && ok "$p legível e gravável" || bad "$p sem permissão de leitura/escrita"; done
fi

# network: the boards connect to the SSID of espconfiguration.h, the server must be on the same network
ssid=$(awk '$2=="WIFI_SSID" {gsub(/"/,"",$3); print $3}' "$ROOT/FederatedLearningZephyr/include/espconfiguration.h" 2> /dev/null)
now=$(iwgetid -r 2> /dev/null)
if [ -n "$ssid" ] && [ "$ssid" = "$now" ]; then ok "este computador está na rede \"$now\" (a do firmware)"; else warn "firmware usa a rede \"$ssid\" e este computador está em \"${now:-sem Wi-Fi}\": as placas não alcançam o servidor"; fi
for port in 8888 8080 8000; do
    if ss -tln 2> /dev/null | awk '{print $4}' | grep -qE ":$port\$"; then warn "porta $port já está em uso (sobra de outra run?)"; else ok "porta $port livre"; fi
done
command -v ufw > /dev/null && ufw status 2> /dev/null | grep -q "Status: active" && warn "ufw ativo: libere as portas 8888, 8080 e 8000 para o computador do professor"

[ "$QUIET" = 1 ] || { [ $fail = 0 ] && echo "=> pronto para rodar" || echo "=> falta algo acima (FALTA) que faz as runs falharem no início"; }
exit $fail
