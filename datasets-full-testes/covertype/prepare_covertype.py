"""Builds the project's CSVs from the UCI Covertype archive (covertype.zip).

Output format (same as the HAR files, no header): id,x1..x54,y0..y6
  - 54 features: 10 numeric + 4 wilderness-area one-hot + 40 soil-type one-hot, each min-max scaled to [-1, 1]
    using the statistics of the whole 581,012-row dataset
  - 7 classes (cover types 1..7), one-hot
  - dataset_full.csv: class-balanced training pool (POOL_ROWS rows, ~equal per class)
  - datasetevaluation.csv: class-balanced held-out rows that are NOT in the pool
usage: prepare_covertype.py <covertype.zip> [out_dir [pool_rows [eval_rows]]]   (defaults: here, 3000, 1000)"""
import gzip, io, os, sys, zipfile
import numpy as np

SEED = 42
NUM_CLASSES = 7

zip_path = sys.argv[1]
out_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(os.path.abspath(__file__))
POOL_ROWS = int(sys.argv[3]) if len(sys.argv) > 3 else 3000
EVAL_ROWS = int(sys.argv[4]) if len(sys.argv) > 4 else 1000

z = zipfile.ZipFile(zip_path)
raw = gzip.decompress(z.read("covtype.data.gz"))
data = np.loadtxt(io.BytesIO(raw), delimiter=",", dtype=np.float64)
X, y = data[:, :-1], data[:, -1].astype(int) - 1
assert X.shape == (581012, 54) and y.min() == 0 and y.max() == NUM_CLASSES - 1

lo, hi = X.min(axis=0), X.max(axis=0)
span = np.where(hi - lo == 0, 1, hi - lo)
Xn = 2 * (X - lo) / span - 1

rng = np.random.default_rng(SEED)


def balanced_pick(rows_per_class, exclude):
    """rows_per_class[c] rows of class c, never from `exclude`; shuffled."""
    chosen = []
    for c in range(NUM_CLASSES):
        candidates = np.setdiff1d(np.where(y == c)[0], exclude, assume_unique=False)
        chosen.append(rng.choice(candidates, size=rows_per_class[c], replace=False))
    chosen = np.concatenate(chosen)
    rng.shuffle(chosen)
    return chosen


def split_evenly(total):
    base, extra = divmod(total, NUM_CLASSES)
    return [base + (1 if c < extra else 0) for c in range(NUM_CLASSES)]


pool = balanced_pick(split_evenly(POOL_ROWS), exclude=np.array([], dtype=int))
held = balanced_pick(split_evenly(EVAL_ROWS), exclude=pool)
assert len(np.intersect1d(pool, held)) == 0

onehot = np.eye(NUM_CLASSES, dtype=int)


def write(path, idx):
    rows = np.hstack([np.arange(1, len(idx) + 1).reshape(-1, 1), Xn[idx], onehot[y[idx]]])
    np.savetxt(path, rows, fmt=["%d"] + ["%.6f"] * 54 + ["%d"] * NUM_CLASSES, delimiter=",")
    counts = np.bincount(y[idx], minlength=NUM_CLASSES)
    print(f"wrote {path}: {len(idx)} rows, per class {counts.tolist()}")


write(os.path.join(out_dir, "dataset_full.csv"), pool)
write(os.path.join(out_dir, "datasetevaluation.csv"), held)
