"""Per-board table of a test_boards.sh session: Wi-Fi, global model received, local model sent, received by the server.
usage: python3 board_report.py <run log dir> <server ip> "<ports>" "<macs>" "<types>"
A board whose own log has no IP line (the serial monitor opened after it had connected) gets, when exactly one board
and one server-registered IP are left over, that IP (marked '*')."""
import os, re, sys

log, serverip = sys.argv[1], sys.argv[2]
ports, macs = sys.argv[3].split(), sys.argv[4].split()
types = sys.argv[5].split() if len(sys.argv) > 5 else [""] * len(ports)
srv = open(log + "/server.log", errors="replace").read() if os.path.exists(log + "/server.log") else ""
closed = re.findall(r"Calculating the new global model \((\d+) models\)", srv)
agg = {}
for ip, rd in re.findall(r"Aggregating Local Model (\S+) \(round (\d+)\)", srv):
    agg.setdefault(ip, set()).add(int(rd))
registered = set(re.findall(r"Client IP: (\d+\.\d+\.\d+\.\d+)", srv)) - {serverip}

text, ip = {}, {}
for i in range(len(ports)):
    f = f"{log}/clients/no_{i}.log"
    text[i] = re.sub(r"\x1b\[[0-9;]*m", "", open(f, "rb").read().decode("utf-8", "replace")) if os.path.exists(f) else ""
    found = re.findall(r"IP: (\d+\.\d+\.\d+\.\d+)", text[i])
    ip[i] = found[0] if found else None
free_boards = [i for i in ip if ip[i] is None]
free_ips = sorted(registered - set(v for v in ip.values() if v))
deduced = set()
if len(free_boards) == 1 and len(free_ips) == 1:
    ip[free_boards[0]] = free_ips[0]
    deduced.add(free_boards[0])

print(f"server: {len(closed)} round(s) closed (models per round: {closed or '-'}); registered nodes: {len(registered)} of {len(ports)} boards")
print(f"{'port':14s}{'type':7s}{'MAC':19s}{'IP':17s}{'wifi':6s}{'drops':6s}{'got model':10s}{'sent':6s}{'server got':11s}result")
ok = 0
for i, p in enumerate(ports):
    t = text[i]
    addr = ip[i]
    got = len(re.findall(r"getglobalmodel(?:flat)? feita com sucesso", t))
    sent = t.count("Enviando modelo local")
    drops = t.count("perdeu a conex")
    rec = len(agg.get(addr, ())) if addr else 0
    why = []
    if addr is None: why.append("never got an IP (Wi-Fi refused it or wrong password/network)")
    elif sent == 0 and got == 0 and rec == 0: why.append("registered but no model exchange")
    elif rec == 0: why.append("server did not receive its model")
    good = addr is not None and rec > 0 and (got > 0 or sent > 0 or i in deduced)
    ok += good
    shown = (addr + "*" if i in deduced else addr) if addr else "-"
    print(f"{p:14s}{(types[i] if i < len(types) else ''):7s}{(macs[i] if i < len(macs) else '?'):19s}{shown:17s}{'yes' if addr else 'NO':6s}{drops:<6d}{got:<10d}{sent:<6d}{rec:<11d}{'OK' if good else 'FAIL: ' + '; '.join(why)}")
if deduced:
    print("* IP deduced: its own log has no IP line (the serial monitor attached after it connected) and it is the only registered IP left")
print(f"summary: {ok} of {len(ports)} boards OK")
