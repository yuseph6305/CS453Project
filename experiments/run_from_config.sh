#!/bin/bash
# Usage: bash experiments/run_from_config.sh configs/small.conf

CONFIG=$1
if [ -z "$CONFIG" ]; then
    echo "Usage: bash experiments/run_from_config.sh <config_file>"
    exit 1
fi

# read config values
INPUT_DOT=$(grep input_dot $CONFIG | cut -d'"' -f2)
SEED=$(grep export_seed $CONFIG | cut -d'=' -f2 | tr -d ' ')
RANKS=$(grep ranks $CONFIG | cut -d'=' -f2 | tr -d ' ')
STRATEGY=$(grep strategy $CONFIG | cut -d'"' -f2)
SOURCE=$(grep source_node $CONFIG | cut -d'=' -f2 | tr -d ' ')
ROUNDS=$(grep rounds $CONFIG | cut -d'=' -f2 | tr -d ' ')

echo "=== Running with config: $CONFIG ==="
echo "Input: $INPUT_DOT, Seed: $SEED, Ranks: $RANKS, Strategy: $STRATEGY"

python3 tools/graph_export/graph_export.py "$INPUT_DOT" outputs/graph.json --seed $SEED
python3 tools/partition/partition.py outputs/graph.json --ranks $RANKS --strategy $STRATEGY --out outputs/part.json

cmake -S mpi_runtime -B build > /dev/null
cmake --build build > /dev/null

mpirun -n $RANKS ./build/ngs_mpi --graph outputs/graph.json --part outputs/part.json --algo leader --rounds $ROUNDS
mpirun -n $RANKS ./build/ngs_mpi --graph outputs/graph.json --part outputs/part.json --algo dijkstra --source $SOURCE
