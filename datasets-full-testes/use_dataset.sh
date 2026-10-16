#!/bin/bash
# Makes one of the datasets in this folder the active one for the whole pipeline:
#   - copies its CSVs to where the project reads them (Zephyr pool + server evaluation file)
#   - sets INPUT_FEATURES / OUTPUT_CLASSES / EVAL_SAMPLES in FederatedLearningServer/lib/configs.h
#   - sets NUM_FEATURES / NUM_CLASSES in separate_dataset.py
# usage: datasets-full-testes/use_dataset.sh <har|covertype|covertype-7500|covertype-17220>
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
CFG="$ROOT/FederatedLearningServer/lib/configs.h"

case "$1" in
    har)       FEATURES=32; CLASSES=6; EVAL=300 ;;
    covertype) FEATURES=54; CLASSES=7; EVAL=1000 ;;
    covertype-7500) FEATURES=54; CLASSES=7; EVAL=2002 ;;
    covertype-17220) FEATURES=54; CLASSES=7; EVAL=2002 ;;
    *) echo "usage: $0 <har|covertype|covertype-7500|covertype-17220>"; exit 1 ;;
esac

cp "$HERE/$1/dataset_full.csv" "$ROOT/FederatedLearningZephyr/data/dataset_full.csv"
cp "$HERE/$1/datasetevaluation.csv" "$ROOT/FederatedLearningServer/data/datasetevaluation.csv"

sed -i "s/^#define INPUT_FEATURES [0-9]*/#define INPUT_FEATURES $FEATURES/" "$CFG"
sed -i "s/^#define OUTPUT_CLASSES [0-9]*/#define OUTPUT_CLASSES $CLASSES/" "$CFG"
sed -i "s/^#define EVAL_SAMPLES [0-9]*/#define EVAL_SAMPLES $EVAL/" "$CFG"
sed -i "s/^NUM_FEATURES = [0-9]*/NUM_FEATURES = $FEATURES/" "$ROOT/separate_dataset.py"
sed -i "s/^NUM_CLASSES = [0-9]*/NUM_CLASSES = $CLASSES/" "$ROOT/separate_dataset.py"

echo "active dataset: $1 ($FEATURES features, $CLASSES classes, EVAL_SAMPLES $EVAL)"
