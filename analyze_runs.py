"""Compares a group of runs from catalogo_runs.csv against the no-KD baseline of the group.
usage: python3 analyze_runs.py <column=value> ...        (python3 catalog_runs.py first to refresh the catalog)
  e.g. python3 analyze_runs.py linhas_pool=7500 fracao_dataset_teacher=0.50 aluno=54-4-4-7 teacher_arquitetura=54-16-16-7
The group needs exactly one run with kd=não (the baseline), or pass base=<run id>. Metrics are the server's (accuracy, precision, f1score,
recall: one-vs-rest, threshold 0.5) plus the threshold-free argmax accuracy recomputed from the saved global models.
Prints: last-8-rounds means, paired differences vs the baseline (rounds 8-31), and convergence speed."""
import csv, json, os, statistics as st, sys
import numpy as np

ROOT = os.path.dirname(os.path.abspath(__file__))
RES, MOD = (f"{ROOT}/FederatedLearningServer/{d}" for d in ("resultados", "modelos"))
EVAL_BY_POOL = {"3000": "covertype", "7500": "covertype-7500", "17220": "covertype-17220"}
cat = {r["run"]: r for r in csv.DictReader(open(f"{ROOT}/catalogo_runs.csv"))}
flt = dict(a.split("=", 1) for a in sys.argv[1:])
BASE = flt.pop("base", None)   # optional: baseline run id when the baseline is not part of the filtered group
runs = sorted(r for r, c in cat.items() if all(c.get(k) == v for k, v in flt.items()) and c["rodadas_concluidas"] == "32")
bases = [BASE] if BASE else [r for r in runs if cat[r]["kd"] == "não"]
if BASE:
    runs = sorted(set(runs) | {BASE})
if len(bases) != 1:
    sys.exit(f"the filter must leave exactly one no-KD baseline, found {len(bases)} among {len(runs)} runs")
base = bases[0]
pool = cat[base]["linhas_pool"]
ev = np.loadtxt(f"{ROOT}/datasets-full-testes/{EVAL_BY_POOL[pool]}/datasetevaluation.csv", delimiter=",")
Xev, yev = ev[:, 1:55], ev[:, 55:62].argmax(1)


def name(c):
    if c["kd"] == "não":
        return "sem KD"
    extra = f" T{c['kd_temperatura']}" if c["kd_modo"] == "logit" and c["kd_temperatura"] else ""
    cam = f" cam{c['kd_camada_aluno']}" if c["kd_modo"] == "latent" else ""
    return f"{c['kd_modo']}{cam} a{c['kd_alpha']}{extra}"


col = lambda r, k: [float(x[k]) for x in csv.DictReader(open(f"{RES}/{r}/metrics.csv")) if int(x["interaction"]) >= 0]


def argmax_curve(r):
    out = []
    for f in sorted((f for f in os.listdir(f"{MOD}/{r}") if f.startswith("modelo_")), key=lambda f: int(f[7:-5])):
        a = Xev
        for L in json.load(open(f"{MOD}/{r}/{f}"))["neuralnetwork"]["layersArray"][1:]:
            W = np.array([n["weightsArray"] for n in L["neuronsArray"]]); b = np.array([n["bias"] for n in L["neuronsArray"]])
            z = a @ W.T + b
            a = np.maximum(z, 0) if L["activationfunctiontype"] == 2 else z
        out.append(float(np.mean(a.argmax(1) == yev)))
    return out


def rt(c, target, hold=3):
    for i in range(len(c) - hold + 1):
        if all(x >= target for x in c[i:i + hold]):
            return i


order = [base] + sorted((r for r in runs if r != base), key=lambda r: (cat[r]["kd_modo"], float(cat[r]["kd_alpha"] or 0)))
C = {r: dict(acc=col(r, "accuracy"), prec=col(r, "precision"), f1=col(r, "f1score"), rec=col(r, "recall"), am=argmax_curve(r)) for r in order}
last = lambda c: st.mean(c[-8:])
b = cat[base]
print(f"grupo: {flt} | {len(order)} runs | pool {pool}, professor {b['linhas_professor']} linhas, placas {b['linhas_por_placa']}, aluno {b['aluno']}")
t = [(name(cat[r]), cat[r]["teacher_acc"] or cat[r]["teacher_loss_reconstrucao"], cat[r]["professor_epocas"], cat[r]["professor_lr"]) for r in order if cat[r]["kd"] == "sim"]
if t:
    print(f"professor: {cat[order[1]]['teacher_arquitetura']}, {t[0][2]} épocas, taxa {t[0][3]}, acerto do servidor/erro de reconstrução {t[0][1]}")

print(f"\n=== últimas 8 rodadas (média) ===\n{'':20s}| accuracy | precision | f1score | recall | argmax")
for r in order:
    c = C[r]
    print(f"{name(cat[r]):20s}| {last(c['acc']):.3f}    | {last(c['prec']):.3f}     | {last(c['f1']):.3f}   | {last(c['rec']):.3f}  | {last(c['am']):.3f}")

print(f"\n=== diferença pareada vs baseline, rodadas 8-31 (média; melhor/pior em 24) ===\n{'':20s}| f1score          | argmax           | precision        | accuracy")
for r in order[1:]:
    row = []
    for k in ("f1", "am", "prec", "acc"):
        d = [x - y for x, y in zip(C[r][k][8:], C[base][k][8:])]
        row.append(f"{st.mean(d):+.3f} ({sum(x > 0 for x in d)}/{sum(x < 0 for x in d)})")
    print(f"{name(cat[r]):20s}| " + " | ".join(f"{x:16s}" for x in row))

print("\n=== velocidade de convergência: rodadas até o nível final da baseline | média rodadas 0-4 | rodada 0 | patamar ===")
for key, lab, tol in (("f1", "F1", None), ("am", "argmax", None), ("acc", "accuracy (CSV)", 0.005), ("prec", "precision (CSV)", 0.005)):
    pl = last(C[base][key]); tgt = 0.98 * pl if tol is None else pl - tol
    print(f"-- {lab}: nível final da baseline {pl:.3f}")
    for r in order:
        c = C[r][key]; x = rt(c, tgt)
        print(f"   {name(cat[r]):20s} até o nível: {('nunca' if x is None else x):>5} | média 0-4 {st.mean(c[:5]):.3f} | rodada 0 {c[0]:.3f} | patamar {last(c):.3f}")
