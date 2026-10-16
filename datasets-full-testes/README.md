# datasets-full-testes

Datasets used in the federated-learning tests, one folder each. CSV format (no header):
`id, x1..xN, y0..yC-1` — features scaled to [-1, 1], one-hot labels. `dataset_full.csv` is the training pool that
`separate_dataset.py` splits between the teacher client and the boards; `datasetevaluation.csv` is held out and
used by the server for the metrics.

| Folder | Source | Features | Classes | Pool / evaluation rows | `EVAL_SAMPLES` |
|---|---|---|---|---|---|
| `har/` | UCI HAR (smartphone sensors), StandardScaler + PCA to 32 components, min-max [-1, 1] | 32 | 6 | 9299 / 1000 | 300 |
| `covertype/` | UCI Covertype, class-balanced sample (`prepare_covertype.py`) | 54 | 7 | 3000 / 1000 | 1000 |
| `covertype-7500/` | Same as `covertype/` with a larger class-balanced pool (`prepare_covertype.py <zip> <out> 7500 2002`) | 54 | 7 | 7500 / 2002 | 2002 |
| `covertype-17220/` | Largest class-balanced pool the covertype allows (smallest class has 2747 rows: 2460 per class in the pool + 286 per class held out) (`prepare_covertype.py <zip> <out> 17220 2002`) | 54 | 7 | 17220 / 2002 | 2002 |
| `iris/` | Original dataset of the project, kept only as an archive (not wired into `use_dataset.sh`) | 4 | 3 | 120 / 30 | — |

`pamap2_data/` holds a PAMAP2 download that was not turned into a dataset.

## Switching the active dataset

```bash
datasets-full-testes/use_dataset.sh covertype   # or: har, covertype-7500, covertype-17220
```

Copies the CSVs to `FederatedLearningZephyr/data/dataset_full.csv` and `FederatedLearningServer/data/datasetevaluation.csv`
and sets `INPUT_FEATURES`, `OUTPUT_CLASSES`, `EVAL_SAMPLES` (`configs.h`) and `NUM_FEATURES`, `NUM_CLASSES`
(`separate_dataset.py`). The firmware must be rebuilt afterwards (`run_zephyr.sh` already does it).

## Covertype

`covertype/prepare_covertype.py <covertype.zip>` rebuilds both CSVs (seed 42). 54 features = 10 numeric +
4 wilderness-area one-hot + 40 soil-type one-hot, scaled with the statistics of the full 581,012 rows. Pool and
evaluation sets are class-balanced (~429 / ~143 rows per class) and disjoint. The balanced sampling is on purpose:
the natural distribution has two classes with 85% of the rows and a class with 0.5%, which would leave the teacher
client with ~100 balanced rows.
