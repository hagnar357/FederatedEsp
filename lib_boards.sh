# Shared by run_zephyr.sh, run_all_remote.sh and test_boards.sh: which kind of board is on a serial port and which
# firmware/chip/overlay it needs. Several kinds can be plugged at the same time (ESP32-S3 and classic ESP32).
#   s3     Heltec Wireless Stick Lite V3 (ESP32-S3)        build dir build_s3
#   esp32  classic ESP32 (esp32_devkitc, esp32.overlay)     build dir build_esp32
#   custom whatever BOARD / CHIP / OVERLAY say             build dir build_custom

# prints s3 | esp32 | unknown for one serial port (esptool reads the chip, it only resets the board)
board_detect_type() {
    local out
    out=$( { command -v esptool > /dev/null && esptool --port "$1" flash-id || esptool.py --port "$1" flash_id; } 2>&1 \
           | grep -m1 -E "Chip type|Chip is")
    case "$out" in
        *ESP32-S3*) echo s3 ;;
        *ESP32*)    echo esp32 ;;
        *)          echo unknown ;;
    esac
}

# prints "<west board> <esptool chip> <overlay or -> <build dir>" for a type
board_params() {
    case "$1" in
        s3)     echo "heltec_wireless_stick_lite_v3/esp32s3/procpu esp32s3 - build_s3" ;;
        esp32)  echo "esp32_devkitc/esp32/procpu esp32 esp32.overlay build_esp32" ;;
        custom) echo "${BOARD:-heltec_wireless_stick_lite_v3/esp32s3/procpu} ${CHIP:-esp32s3} ${OVERLAY:--} build_custom" ;;
    esac
}

# IPv4 of this computer on the network of the boards: the Wi-Fi interface (wl*/ap*, also the hotspot) first, because the
# default route may go through Ethernet, a VPN or virbr0/Hamachi; then the default route; then the first address.
server_ip() {
    local ip
    ip=$(ip -4 -o addr show 2> /dev/null | awk '$2 ~ /^(wl|ap)/ {split($4, a, "/"); print a[1]; exit}')
    [ -n "$ip" ] || ip=$(ip route get 1.1.1.1 2> /dev/null | awk '{for (i = 1; i <= NF; i++) if ($i == "src") print $(i + 1)}')
    [ -n "$ip" ] || ip=$(hostname -I | awk '{print $1}')
    echo "$ip"
}
