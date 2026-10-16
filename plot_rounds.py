"""Per-round metrics of the covertype-17220 runs (logit KD, T=2, teacher 3x12), one figure per fixed setting:
  imgs/rodadas_prof<NN>.png     fixed teacher fraction  -> one line per alpha tested (+ the no-KD baseline, dashed)
  imgs/rodadas_alpha<A>.png     fixed alpha             -> one line per teacher fraction tested (+ each fraction's baseline, dashed)
Every figure has four panels: F1, argmax accuracy, precision and accuracy (one-vs-rest) at each of the 32 rounds.
Only complete runs (32 rounds, all nodes in every round) are used; re-run after python3 catalog_runs.py to pick up new runs.
usage: python3 plot_rounds.py"""
import csv, json, os
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

ROOT = os.path.dirname(os.path.abspath(__file__))
RES, MOD = (f"{ROOT}/FederatedLearningServer/{d}" for d in ("resultados", "modelos"))
POOL = "17220"
SURFACE, INK, INK2, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e3e2dd"
# a teacher fraction always has the same color (categorical slots 1-5 of the reference palette, 30% and 50% as before)
FRACTION_COLOR = {"0.30": "#2a78d6", "0.50": "#eb6834", "0.40": "#1baf7a", "0.60": "#eda100", "0.70": "#e87ba4"}
# alpha is ordered: one blue ramp, light (small alpha) to dark (large alpha)
ALPHA_RAMP = ["#8fbcf0", "#5f9fe6", "#2a78d6", "#1d5cac", "#144282", "#0b2a56"]
BASELINE_COLOR = "#52514e"
METRICS = [("f1", "F1"), ("am", "Acurácia argmax"), ("prec", "Precisão"), ("acc", "Acurácia (um-contra-todos)")]

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


cat = [r for r in csv.DictReader(open(f"{ROOT}/catalogo_runs.csv")) if r["linhas_pool"] == POOL
       and r["rodadas_concluidas"] == "32" and r["rodadas_com_todos_os_nos"] == "32"
       and (r["kd"] == "não" or (r["kd_modo"] == "logit" and r["kd_temperatura"] == "2.00"))]
runs = {}
for r in sorted(cat, key=lambda r: r["run"]):           # the latest complete run wins
    runs[(r["fracao_dataset_teacher"], None if r["kd"] == "não" else float(r["kd_alpha"]))] = r["run"]
fractions = sorted({f for (f, a) in runs if a is None})
C = {k: dict(f1=csv_col(v, "f1score"), am=argmax_curve(v), prec=csv_col(v, "precision"), acc=csv_col(v, "accuracy"))
     for k, v in runs.items()}

plt.rcParams.update({"font.family": "DejaVu Sans", "text.color": INK, "axes.labelcolor": INK2, "xtick.color": INK2,
                     "ytick.color": INK2, "axes.edgecolor": GRID, "figure.facecolor": SURFACE, "axes.facecolor": SURFACE})


def style(ax, title):
    ax.set_title(title, loc="left", fontsize=12, color=INK, fontweight="bold")
    ax.grid(axis="y", color=GRID, lw=0.8)
    ax.set_axisbelow(True)
    for s in ("top", "right", "left"):
        ax.spines[s].set_visible(False)
    ax.tick_params(length=0)
    ax.set_xlabel("rodada", fontsize=9)
    ax.set_ylabel("%", fontsize=9)
    ax.set_xlim(0, 31)
    ax.set_xticks([0, 5, 10, 15, 20, 25, 31])


def save(fig, path, title, note, handles):
    fig.legend(handles=handles, loc="lower center", ncol=min(len(handles), 4), frameon=False, fontsize=10, labelcolor=INK2)
    fig.text(0.06, 0.965, title, ha="left", va="top", fontsize=14, fontweight="bold", color=INK)
    fig.text(0.06, 0.925, note, ha="left", va="top", fontsize=9.5, color=INK2, linespacing=1.5)
    fig.tight_layout(rect=(0, 0.09 if len(handles) > 4 else 0.06, 1, 0.88))
    fig.savefig(path, facecolor=SURFACE)
    plt.close(fig)


NOTE = "covertype (pool de 17.220 linhas), KD por logits, T=2, professor 3×12; aluno 54-4-4-7; 3 nós; uma run por linha, uma semente"
os.makedirs(f"{ROOT}/imgs", exist_ok=True)
written = []

# fixed teacher fraction: one line per alpha
for f in fractions:
    alphas = sorted(a for (g, a) in runs if g == f and a is not None)
    if not alphas:
        continue          # only the baseline exists so far
    color_of = {a: ALPHA_RAMP[min(i, len(ALPHA_RAMP) - 1)] for i, a in enumerate(alphas)}
    fig, axes = plt.subplots(2, 2, figsize=(11, 7.8), dpi=160)
    for ax, (key, title) in zip(axes.flat, METRICS):
        ax.plot(range(32), np.array(C[(f, None)][key]) * 100, color=BASELINE_COLOR, lw=1.8, ls=(0, (4, 3)), zorder=2)
        for a in alphas:
            ax.plot(range(32), np.array(C[(f, a)][key]) * 100, color=color_of[a], lw=1.8, zorder=3)
        style(ax, title)
    handles = [Line2D([0], [0], color=BASELINE_COLOR, lw=1.8, ls=(0, (4, 3)), label="baseline sem KD")] + \
              [Line2D([0], [0], color=color_of[a], lw=2, label=f"α = {a:g}") for a in alphas]
    path = f"{ROOT}/imgs/rodadas_prof{int(float(f) * 100)}.png"
    save(fig, path, f"Métricas por rodada, professor com {int(float(f) * 100)}% do dataset",
         NOTE + "\numa linha por α testado; tracejado = baseline sem KD", handles)
    written.append(path)

# fixed alpha: one line per teacher fraction (dashed = that fraction's own baseline)
for a in sorted({a for (_, a) in runs if a is not None}):
    fr = [f for f in fractions if (f, a) in runs]
    if len(fr) < 2:
        continue
    fig, axes = plt.subplots(2, 2, figsize=(11, 7.8), dpi=160)
    for ax, (key, title) in zip(axes.flat, METRICS):
        for f in fr:
            ax.plot(range(32), np.array(C[(f, None)][key]) * 100, color=FRACTION_COLOR[f], lw=1.4, ls=(0, (4, 3)), alpha=0.9, zorder=2)
            ax.plot(range(32), np.array(C[(f, a)][key]) * 100, color=FRACTION_COLOR[f], lw=2, zorder=3)
        style(ax, title)
    handles = [Line2D([0], [0], color=FRACTION_COLOR[f], lw=2, label=f"professor com {int(float(f) * 100)}%") for f in fr] + \
              [Line2D([0], [0], color=BASELINE_COLOR, lw=1.4, ls=(0, (4, 3)), label="baseline sem KD (mesma cor = mesma fração)")]
    path = f"{ROOT}/imgs/rodadas_alpha{a:g}.png"
    save(fig, path, f"Métricas por rodada com α = {a:g}",
         NOTE + "\numa linha por fração do dataset no professor; tracejado = baseline sem KD daquela fração", handles)
    written.append(path)

print("figuras:", *[os.path.relpath(p, ROOT) for p in written], sep="\n  ")
print("runs usadas:", len(runs))
