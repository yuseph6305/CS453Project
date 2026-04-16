#!/bin/bash
# Runs all experiments for both graphs and both partitioning strategies
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "=== Building MPI runtime ==="
cmake -S mpi_runtime -B build > /dev/null
cmake --build build > /dev/null
echo "Build complete."

echo ""
echo "=== Exporting graphs ==="
python3 tools/graph_export/graph_export.py outputs/MyFirstGraph.ngs.dot outputs/graph1.json --seed 42
python3 tools/graph_export/graph_export.py outputs/SecondGraph.ngs.dot outputs/graph2.json --seed 123

echo ""
echo "=== Partitioning ==="
python3 tools/partition/partition.py outputs/graph1.json --ranks 4 --strategy contiguous --out outputs/part1_contiguous.json
python3 tools/partition/partition.py outputs/graph1.json --ranks 4 --strategy roundrobin --out outputs/part1_roundrobin.json
python3 tools/partition/partition.py outputs/graph2.json --ranks 4 --strategy contiguous --out outputs/part2_contiguous.json
python3 tools/partition/partition.py outputs/graph2.json --ranks 4 --strategy roundrobin --out outputs/part2_roundrobin.json

echo ""
echo "=== Experiment 1: Graph 1 contiguous ==="
mpirun -n 4 ./build/ngs_mpi --graph outputs/graph1.json --part outputs/part1_contiguous.json --algo leader --rounds 200
mpirun -n 4 ./build/ngs_mpi --graph outputs/graph1.json --part outputs/part1_contiguous.json --algo dijkstra --source 0

echo ""
echo "=== Experiment 2: Graph 1 roundrobin ==="
mpirun -n 4 ./build/ngs_mpi --graph outputs/graph1.json --part outputs/part1_roundrobin.json --algo leader --rounds 200
mpirun -n 4 ./build/ngs_mpi --graph outputs/graph1.json --part outputs/part1_roundrobin.json --algo dijkstra --source 0

echo ""
echo "=== Experiment 3: Graph 2 contiguous ==="
mpirun -n 4 ./build/ngs_mpi --graph outputs/graph2.json --part outputs/part2_contiguous.json --algo leader --rounds 200
mpirun -n 4 ./build/ngs_mpi --graph outputs/graph2.json --part outputs/part2_contiguous.json --algo dijkstra --source 0

echo ""
echo "=== Experiment 4: Graph 2 roundrobin ==="
mpirun -n 4 ./build/ngs_mpi --graph outputs/graph2.json --part outputs/part2_roundrobin.json --algo leader --rounds 200
mpirun -n 4 ./build/ngs_mpi --graph outputs/graph2.json --part outputs/part2_roundrobin.json --algo dijkstra --source 0

echo ""
echo "=== Running tests ==="
python3 tests/test_project.py

echo ""
echo "=== All experiments complete ==="
