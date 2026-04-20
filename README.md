# CS453 MPI Distributed Algorithms

This project implements two distributed graph algorithms using MPI:

* FloodMax Leader Election
* Distributed Dijkstra Shortest Paths

The graph is partitioned across multiple MPI ranks, where each rank owns a subset of nodes. Communication between ranks is handled using MPI collective operations.

---

## Quick Start

The repository already includes pre-generated graph files in the `outputs/` directory, so NetGameSim is not required to run the project.

### 1. Export graph to JSON

```bash
python3 tools/graph_export/graph_export.py outputs/MyFirstGraph.ngs.dot outputs/graph.json --seed 42
```

### 2. Partition graph across ranks

```bash
python3 tools/partition/partition.py outputs/graph.json --ranks 4 --strategy contiguous --out outputs/part.json
```

### 3. Build the MPI program

```bash
cmake -S mpi_runtime -B build
cmake --build build
```

### 4. Run leader election

```bash
mpirun -n 4 ./build/ngs_mpi --graph outputs/graph.json --part outputs/part.json --algo leader --rounds 200
```

### 5. Run Dijkstra

```bash
mpirun -n 4 ./build/ngs_mpi --graph outputs/graph.json --part outputs/part.json --algo dijkstra --source 0
```

---

## Running Tests

```bash
python3 tests/test_project.py
```

---

## Running Experiments

Run all experiments:

```bash
bash experiments/run_all.sh
```

Run using a configuration file:

```bash
bash experiments/run_from_config.sh configs/small.conf
```

---

## Dependencies

### macOS

```bash
brew install open-mpi cmake python3
```

### Linux (Ubuntu/Debian)

```bash
sudo apt-get install openmpi-bin libopenmpi-dev cmake python3
```

---

## Project Structure

```
tools/graph_export/    Converts DOT files to JSON
tools/partition/       Partitions graph nodes across MPI ranks
mpi_runtime/           C implementation of MPI algorithms
tests/                 Unit tests
outputs/               Graph files and generated outputs
configs/               Example configuration files
experiments/           Scripts to run experiments
REPORT.md              Project report
```

---

## Data Formats

### graph.json

* nodes: list of node IDs
* edges: list of objects with src, dst, and weight

### part.json

* owner: mapping from node ID to rank
* rank_nodes: nodes owned by each rank
* ghost_nodes: neighbors owned by other ranks

---

## Reproducibility

* Graph generation uses a fixed seed (`--seed`)
* Partitioning is deterministic
* Running the same commands will produce the same results

---

## Correctness Assumptions

* All edge weights are positive (required for Dijkstra)
* The graph is connected (ensured during export)
* Node IDs are unique in the range 0 to N-1

---

## Implementation Notes

* Each rank processes its local nodes
* MPI_Allreduce is used for global synchronization
* Leader election propagates the maximum node ID
* Dijkstra selects the global minimum node each iteration

---

## Optional: Regenerating Graphs

This project includes graph files in `outputs/`, so this step is not required.

If you want to regenerate graphs, you need to download and build NetGameSim separately and place the generated `.dot` files into the `outputs/` directory.

---

## Notes

The project is designed so that a grader can clone the repository and run all commands directly from the README without additional setup.
