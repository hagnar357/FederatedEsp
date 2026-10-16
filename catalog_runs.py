"""Regenerates catalogo_runs.csv: one row per run with its folders, the training configuration (read from the
run's own artifacts, not from the current configs.h) and caveats (missing nodes per round and the evidence of why,
unfinished runs, runs without KD...).
usage: python3 catalog_runs.py [project_root]"""
import csv, json, os, re, statistics as st, subprocess, sys
from datetime import datetime

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(__file__))
OUT = os.path.join(ROOT, "catalogo_runs.csv")
LOG, MOD, RES = (os.path.join(ROOT, p) for p in ("log_run", "FederatedLearningServer/modelos", "FederatedLearningServer/resultados"))
ANSI = re.compile(r"\x1b\[[0-9;]*m")
NETWORKS = {"192.168.3.": "HUAWEI-CAECOMPS-2.4", "192.168.1.": "Isabel"}
# (student input width, classes) -> (dataset, pool rows used only when the logs do not give the split)
DATASETS = {(32, 6): ("har", 9299), (54, 7): ("covertype", 3000), (4, 3): ("iris", 120)}


def read(p):
    try:
        return ANSI.sub("", open(p, errors="replace").read())
    except OSError:
        return ""


def load_json(p):
    try:
        return json.load(open(p))
    except (OSError, ValueError):
        return None


def shape(nn):
    return "-".join(str(l["neurons"]) for l in nn["layersArray"])


def board_ip(text):
    m = re.search(r"(?:nos deu o IP|Received): (\d+\.\d+\.\d+\.\d+)", text)
    return m.group(1) if m else None


def failure_evidence(text):
    """Evidence in a node's serial log of why it failed to deliver its model (every non-zero signature)."""
    counts = [
        ("queda/reconexão do Wi-Fi", len(re.findall(r"perdeu a conex|Falha ao conectar no WiFi", text))),
        ("sem memória no envio (-ENOMEM)", len(re.findall(r"enviar os dados: -12", text))),
        ("provável falta de memória ao serializar o modelo",
         max(0, len(re.findall(r"WEBSOCKET_EVENT_CONNECTED", text))
             - len(re.findall(r"Dados enviados com sucesso|Falha ao enviar os dados", text)))),
        ("modelo global inválido (sem memória no parse)", len(re.findall(r"inválida no JSON", text))),
        ("timeouts de rede (TCP)", len(re.findall(r"via TCP: 116|ret -116|handshake WebSocket: -113", text))),
    ]
    found = [f"{name} {n}x" for name, n in counts if n]
    return "evidências no log: " + ", ".join(found) if found else "nenhuma evidência no log da placa"


acc = lambda r: int(r["TP"]) / (int(r["TP"]) + int(r["FN"]))
runs_on_disk = sorted(r for r in os.listdir(LOG) if r.startswith("run_"))
server_up = subprocess.run(["pgrep", "-f", r"^\./FederatedLearningServer"], capture_output=True).returncode == 0
rows = []

for run in runs_on_disk:
    server = read(os.path.join(LOG, run, "server.log"))
    tclient = read(os.path.join(LOG, run, "teacher_client.log"))
    cdir = os.path.join(LOG, run, "clients")
    boards = {f: read(os.path.join(cdir, f)) for f in sorted(os.listdir(cdir))} if os.path.isdir(cdir) else {}
    allnodes = "\n".join(boards.values()) + "\n" + tclient

    student = load_json(os.path.join(MOD, run, "modelo_1.json"))
    teacher = load_json(os.path.join(MOD, run, "teacher.json"))
    snn = student["neuralnetwork"] if student else None
    tnn = teacher["neuralnetwork"] if teacher else None
    dataset, pool_rows = DATASETS.get((snn["layersArray"][0]["neurons"], snn["layersArray"][-1]["neurons"]), ("", 0)) if snn else ("", 0)

    teacher_kind = "" if tnn is None else ("classificador" if tnn["layersArray"][-1]["activationfunctiontype"] == 4 else "encoder-decoder")

    kd_lines = re.findall(r"KD on \(mode=(\w+)(?:, T=([\d.]+))?, alpha=([\d.]+)(?:, student layer (\d+))?\)", allnodes)
    kd = bool(kd_lines)
    mode, temp, alpha, layer = kd_lines[0] if kd else ("", "", "", "")
    if kd and mode == "latent" and not layer:
        layer = "2"  # before the hint layer became configurable it was always the last hidden layer

    seed = re.search(r"Weight init seed: (\d+)", server)
    ips = re.findall(r"Client IP: (\d+\.\d+\.\d+\.\d+)", server)
    network = next((v for k, v in NETWORKS.items() if any(ip.startswith(k) for ip in ips)), "")
    tsamples = re.search(r"dataset \S+ \((\d+) samples\)", tclient)
    # pool = teacher rows + the rows of each board, all read from the run's own logs
    board_rows = [re.search(r"Instâncias no Dataset: (\d+)", t) for t in boards.values()]
    if tsamples and board_rows and all(board_rows):
        pool_rows = int(tsamples.group(1)) + sum(int(m.group(1)) for m in board_rows)

    models = [int(x) for x in re.findall(r"Calculating the new global model \((\d+) models\)", server)]
    nodes = int(re.search(r"client nodes registered \d+ client nodes (\d+)", server).group(1)) if "client nodes registered" in server else 3
    finished = "max interation" in server

    metrics = list(csv.DictReader(open(os.path.join(RES, run, "metrics.csv")))) if os.path.exists(os.path.join(RES, run, "metrics.csv")) else []
    rounds = [r for r in metrics if int(r["interaction"]) >= 0]
    teval = [r for r in metrics if int(r["interaction"]) < 0]
    recon = re.search(r"Teacher reconstruction loss: ([\d.]+)", server)

    obs = []
    if not finished:
        state = "em andamento" if run == runs_on_disk[-1] and server_up else "interrompido"
        obs.append(f"run {state} ({len(models)} rodadas concluídas)")
    if not kd and teacher_kind:
        obs.append("KD desligado: o teacher foi treinado na fase 1 mas não foi usado no treino federado")
    missing = {}
    for rnd, ms in re.findall(r"Round (\d+) timeout, missing nodes: ([\d. ]+)", server):
        for ip in ms.split():
            missing.setdefault(ip, []).append(int(rnd))
    ip_to_log = {board_ip(t): t for t in boards.values() if board_ip(t)}
    board_ips = sorted(set(re.findall(r"Client IP: (\d+\.\d+\.\d+\.\d+)\n", server)))
    unmapped = [t for t in boards.values() if not board_ip(t)]
    free_ips = [ip for ip in board_ips if ip not in ip_to_log]
    if len(unmapped) == 1 and len(free_ips) == 1:  # the only board without an IP line gets the only unclaimed IP
        ip_to_log[free_ips[0]] = unmapped[0]
    for ip, rs in sorted(missing.items()):
        rs = sorted(rs)
        span = f"{rs[0]}" if len(rs) == 1 else (f"{rs[0]}-{rs[-1]}" if rs == list(range(rs[0], rs[-1] + 1)) else ",".join(map(str, rs)))
        cause = failure_evidence(ip_to_log[ip]) if ip in ip_to_log else "não foi possível associar o IP a um log de placa"
        obs.append(f"{len(rs)} rodada(s) sem o nó {ip} (rodadas {span}): {cause}")
    if any(m < nodes for m in models) and not missing:
        obs.append("rodadas com menos modelos que nós (ver server.log)")
    if kd and re.search(r"Falha na requisição /api/getteachermodel", allnodes):
        obs.append("algum nó treinou ao menos uma rodada sem o teacher (download falhou)")

    last8 = [r for i, r in enumerate(rounds[-8:], start=max(0, len(rounds) - 8)) if i < len(models) and models[i] >= nodes]
    rows.append({
        "run": run,
        "inicio": datetime.strptime(run[4:], "%Y%m%d_%H%M%S").strftime("%Y-%m-%d %H:%M"),
        "pasta_logs": f"log_run/{run}",
        "pasta_modelos": f"FederatedLearningServer/modelos/{run}",
        "pasta_resultados": f"FederatedLearningServer/resultados/{run}",
        "dataset": dataset,
        "rede_wifi": network,
        "semente_pesos": seed.group(1) if seed and seed.group(1) != "0" else "aleatória",
        "nos": nodes,
        "linhas_pool": pool_rows or "",
        "linhas_professor": tsamples.group(1) if tsamples else "",
        "linhas_por_placa": "+".join(m.group(1) for m in board_rows if m),
        "fracao_dataset_teacher": f"{int(tsamples.group(1)) / pool_rows:.2f}" if tsamples and pool_rows else "",
        "aluno": shape(snn) if snn else "",
        "epocas_por_rodada": snn["epoch"] if snn else "",
        "learning_rate": f"{snn['alpha']:.3g}" if snn else "",
        "teacher": teacher_kind,
        "teacher_arquitetura": shape(tnn) if tnn else "",
        "professor_epocas": tnn["epoch"] if tnn else "",
        "professor_lr": f"{tnn['alpha']:.3g}" if tnn else "",
        "kd": "sim" if kd else "não",
        "kd_modo": mode,
        "kd_alpha": alpha,
        "kd_temperatura": temp if mode == "logit" else "",
        "kd_camada_aluno": layer if mode == "latent" else "",
        "rodadas_concluidas": len(models),
        "rodadas_com_todos_os_nos": sum(1 for m in models if m >= nodes),
        "acc_final": f"{acc(rounds[-1]):.3f}" if rounds else "",
        "acc_media_ultimas8_todos_nos": f"{st.mean(map(acc, last8)):.3f}" if last8 else "",
        "teacher_acc": f"{acc(teval[0]):.3f}" if teval else "",
        "teacher_loss_reconstrucao": recon.group(1) if recon else "",
        "obs": "; ".join(obs),
    })

with open(OUT, "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
    w.writeheader()
    w.writerows(rows)
print(f"{len(rows)} runs -> {OUT}")
