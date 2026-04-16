NetGameSim MPI Distributed Algorithms
Dependencies

First install these:

brew install open-mpi cmake python3
Setup

Here’s how to run the project step by step:

## Step 1 - Graph files
The generated graph DOT files are already in outputs/. 
To regenerate them, use NetGameSim (see netgamesim/README.md).

## Step 2 - Export graph to JSON
python3 tools/graph_export/graph_export.py outputs/MyFirstGraph.ngs.dot outputs/graph.json --seed 42

# 3. Split the graph across MPI ranks
python3 tools/partition/partition.py outputs/graph.json --ranks 4 --strategy contiguous --out outputs/part.json

# 4. Build the MPI program
cmake -S mpi_runtime -B build
cmake --build build

# 5. Run leader election
mpirun -n 4 ./build/ngs_mpi --graph outputs/graph.json --part outputs/part.json --algo leader --rounds 200

# 6. Run Dijkstra shortest paths
mpirun -n 4 ./build/ngs_mpi --graph outputs/graph.json --part outputs/part.json --algo dijkstra --source 0
Run Tests

To make sure everything works:

python3 tests/test_project.py
Project Layout
tools/graph_export/    Converts NetGameSim graph to JSON
tools/partition/       Splits the graph across ranks
mpi_runtime/src/       Main MPI code in C
tests/                 Test files
outputs/               Generated graphs and results
