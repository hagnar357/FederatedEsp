"""Plots the effect of the KD weight alpha (logit mode, T=2, teacher 3x12) against the no-KD baseline of the same
teacher fraction, for the covertype-17220 runs of catalogo_runs.csv (python3 catalog_runs.py first to refresh it).

Writes two PNGs to imgs/:
  efeito_alpha_delta.png      paired difference vs the baseline (rounds 8-31 mean): F1, argmax, precision, accuracy
  efeito_alpha_absoluto.png   mean of the last 8 rounds, with the baseline as a dashed line

Only complete runs (32 rounds, all nodes in every round) are used. usage: python3 plot_alpha.py"""
import csv, json, os, statistics as st
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

ROOT = os.path.dirname(os.path.abspath(__file__))
RES, MOD = (f"{ROOT}/FederatedLearningServer/{d}" for d in ("resultados", "modelos"))
POOL = "17220"
FRACTIONS = ["0.30", "0.50"]
COLORS = {"0.30": "#2a78d6", "0.50": "#eb6834"}          # categorical slots 1 and 2 of the reference palette
SURFACE, INK, INK2, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e3e2dd"

ev = np.loadtxt(f"{ROOT}/datasets-full-testes/covertype-{POOL}/datasetevaluation.csv", delimiter=",")
XEV, YEV = ev[:, 1:55], ev[:, 55:62].argmax(1)


def csv_col(run, key):
    return [float(r[key]) for r in csv.DictReader(open(f"{RES}/{run}/metrics.csv")) if int(r["interaction"]) >= 0]


def argmax_curve(run):
    out = []
    for f in sorted((f for f in os.listdir(f"{MOD}/{run}") if f.startswith("modelo_")), key=lambda f: int(f[7:-5])):
        a = XEV
        for L in json.load(open(f"{MOD}/{run}/{f}"))["neuralnetwork"]["layersArray"][1:]:
            W = np.array([n["weightsArray"] for n in L["neuronsArray"]]); b = np.array([n["bias"] for n in L["neuronsArray"]])
            z = a @ W.T + b
            a = np.maximum(z, 0) if L["activationfunctiontype"] == 2 else z
        out.append(float(np.mean(a.argmax(1) == YEV)))
    return out


def curves(run):
    return dict(f1=csv_col(run, "f1score"), am=argmax_curve(run), prec=csv_col(run, "precision"), acc=csv_col(run, "accuracy"))


# one complete run per (fraction, kd, alpha); the latest complete one wins
cat = [r for r in csv.DictReader(open(f"{ROOT}/catalogo_runs.csv")) if r["linhas_pool"] == POOL
       and r["rodadas_concluidas"] == "32" and r["rodadas_com_todos_os_nos"] == "32" and r["fracao_dataset_teacher"] in FRACTIONS
       and (r["kd"] == "não" or (r["kd_modo"] == "logit" and r["kd_temperatura"] == "2.00"))]
runs = {}
for r in sorted(cat, key=lambda r: r["run"]):
    runs[(r["fracao_dataset_teacher"], None if r["kd"] == "não" else float(r["kd_alpha"]))] = r["run"]

C = {k: curves(v) for k, v in runs.items()}
alphas = sorted({a for (_, a) in runs if a is not None})
last8 = lambda c: st.mean(c[-8:])
METRICS = [("f1", "F1"), ("am", "Acurácia argmax"), ("prec", "Precisão"), ("acc", "Acurácia (um-contra-todos)")]

plt.rcParams.update({"font.family": "DejaVu Sans", "text.color": INK, "axes.labelcolor": INK2, "xtick.color": INK2,
                     "ytick.color": INK2, "axes.edgecolor": GRID, "figure.facecolor": SURFACE, "axes.facecolor": SURFACE})


def panel(ax, key, title, delta):
    for f in FRACTIONS:
        base = C.get((f, None))
        pts = [(a, C[(f, a)][key]) for a in alphas if (f, a) in C]
        if base is None or not pts:
            continue
        xs = [a for a, _ in pts]
        if delta:
            ys = [st.mean([x - y for x, y in zip(c[8:], base[key][8:])]) * 100 for _, c in pts]
        else:
            ys = [last8(c) * 100 for _, c in pts]
            ax.axhline(last8(base[key]) * 100, color=COLORS[f], lw=1.2, ls=(0, (4, 3)), alpha=0.9)
        ax.plot(xs, ys, color=COLORS[f], lw=2, marker="o", ms=8, mfc=COLORS[f], mec=SURFACE, mew=2, zorder=3)
        ax.annotate(f"{int(float(f) * 100)}%", (xs[-1], ys[-1]), xytext=(8, 0 if f == FRACTIONS[0] else -9),
                    textcoords="offset points", color=INK2, fontsize=10, va="center")
    if delta:
        ax.axhline(0, color=INK2, lw=1.2)
    ax.set_title(title, loc="left", fontsize=12, color=INK, fontweight="bold")
    ax.set_xticks(alphas)
    ax.set_xlim(min(alphas) - 0.03, max(alphas) + 0.07)
    ax.grid(axis="y", color=GRID, lw=0.8)
    ax.set_axisbelow(True)
    for s in ("top", "right", "left"):
        ax.spines[s].set_visible(False)
    ax.tick_params(length=0)
    ax.set_ylabel("diferença vs baseline (pontos percentuais)" if delta else "últimas 8 rodadas (%)", fontsize=9)
    ax.set_xlabel("α (peso do KD)", fontsize=9)


def figure(delta, path, suptitle, note):
    fig, axes = plt.subplots(2, 2, figsize=(11, 7.4), dpi=160)
    for ax, (key, title) in zip(axes.flat, METRICS):
        panel(ax, key, title, delta)
    handles = [Line2D([0], [0], color=COLORS[f], lw=2, marker="o", ms=8, mec=SURFACE, mew=2,
                      label=f"professor com {int(float(f) * 100)}% do dataset") for f in FRACTIONS]
    if not delta:
        handles.append(Line2D([0], [0], color=INK2, lw=1.2, ls=(0, (4, 3)), label="baseline sem KD (mesma cor = mesma fração)"))
    fig.legend(handles=handles, loc="lower center", ncol=len(handles), frameon=False, fontsize=10, labelcolor=INK2)
    fig.text(0.06, 0.965, suptitle, ha="left", va="top", fontsize=14, fontweight="bold", color=INK)
    fig.text(0.06, 0.925, note, ha="left", va="top", fontsize=9.5, color=INK2, linespacing=1.5)
    fig.tight_layout(rect=(0, 0.06, 1, 0.86))
    fig.savefig(path, facecolor=SURFACE)
    plt.close(fig)


os.makedirs(f"{ROOT}/imgs", exist_ok=True)
pending = [f"{int(float(f) * 100)}% com α {a}" for f in FRACTIONS for a in (0.1, 0.2, 0.3, 0.4, 0.5, 0.6) if (f, a) not in runs]
note = ("covertype (pool de 17.220 linhas), KD por logits, T=2, professor 3×12; aluno 54-4-4-7; 3 nós, 32 rodadas; "
        "uma run por ponto, uma semente")
missing = ("\nainda sem run completa: " + "; ".join(pending)) if pending else ""
figure(True, f"{ROOT}/imgs/efeito_alpha_delta.png", "Efeito do α do KD em relação à baseline sem KD",
       note + "\nmédia da diferença pareada nas rodadas 8 a 31 (acima de 0 = KD melhor)" + missing)
figure(False, f"{ROOT}/imgs/efeito_alpha_absoluto.png", "Desempenho com KD por α, com a baseline de cada fração",
       note + "\nmédia das últimas 8 rodadas" + missing)
print("runs usadas:")
for (f, a), r in sorted(runs.items(), key=lambda kv: (kv[0][0], kv[0][1] or 0)):
    print(f"  {f} {'baseline' if a is None else 'alpha ' + str(a):10s} {r}")
