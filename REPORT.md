# REPORT.md — NetGameSim MPI Distributed Algorithms

## Approach and Overall Idea

In this project, I built a system that can handle large graphs by splitting the work across multiple processes. NetGameSim was used to generate the graph and MPI to distribute the computation. Each process (rank) is responsible for a portion of the nodes. I implemented two main algorithms. The first is FloodMax, which finds the node with the highest ID in the entire graph. The second is a distributed version of Dijkstra's algorithm, which computes the shortest paths from a given starting node.

One key idea I learned is that nodes aren't tied to just one process in a simple way. Each process handles multiple nodes, and when nodes are connected across different processes, they have to communicate by sending messages between processes.

---

## Implementation Details

**graph_export.py:** Tool that takes the DOT file from NetGameSim and converts it into JSON so it's easier to use in my program. I also had to add reverse edges because the original graph didn't always include both directions, and my algorithms needed that to work properly.

**partition.py:** This tool splits the graph's nodes across different MPI ranks. Contiguous partitioning assigns a block of nodes to each rank in order, while round-robin distributes nodes one by one across ranks. I compared both to see how the distribution affects performance and workload balance.

**main.c:** This is the main MPI program that loads the graph and partition data from JSON and runs the algorithm I choose. I used `MPI_Allreduce` instead of sending messages manually because it made it easier to combine results across all ranks and keep everything in sync.

Since C doesn't have a built-in JSON library, I wrote a simple parser myself that reads the JSON files and extracts the data I need, like nodes and edges, so the program can use them.

---

## Experimental Hypothesis

I thought contiguous partitioning would have fewer edge cuts because it keeps nodes with similar IDs together, so I expected less communication between ranks. I also expected both partitioning methods to still give correct results since the algorithms should work no matter how the graph is split.

I expected Dijkstra to send more messages than leader election because it has to keep updating distances and communicating between ranks, while leader election just passes IDs around until everyone agrees.

After running the experiments, I saw that this was only partially correct. For Graph 2, contiguous did have fewer edge cuts like I expected. But for Graph 1, round-robin actually had fewer edge cuts, which I didn't expect. I think this happened because NetGameSim assigns node IDs randomly, so grouping nodes by ID doesn't actually group connected nodes together.

---

## Experiment 1 — Graph 1 (501 nodes), Contiguous vs Round-Robin

| Metric | Contiguous | Round-Robin |
|--------|-----------|-------------|
| Edge cuts | 1654 / 2148 (77%) | 1624 / 2148 (75.6%) |
| Leader rounds | 8 | 7 |
| Leader agreement | YES | YES |
| Leader runtime | 0.0039s | 0.0002s |
| Dijkstra reachable | 501 / 501 | 501 / 501 |
| Dijkstra runtime | 0.0049s | 0.0039s |

For Graph 1, round-robin performed slightly better because it had fewer edge cuts than contiguous. This also helped it run faster, since fewer edge cuts means less communication between ranks. That's probably why round-robin had a faster runtime for both leader election and Dijkstra in this case.

---

## Experiment 2 — Graph 2 (301 nodes), Contiguous vs Round-Robin

| Metric | Contiguous | Round-Robin |
|--------|-----------|-------------|
| Edge cuts | 890 / 1198 (74.3%) | 906 / 1198 (75.6%) |
| Leader rounds | 8 | 8 |
| Leader agreement | YES | YES |
| Dijkstra reachable | 301 / 301 | 301 / 301 |
| Dijkstra runtime | 0.0030s | 0.0039s |

For Graph 2, contiguous performed better because it had fewer edge cuts. This led to slightly less communication between ranks, so it ended up being faster than round-robin for Dijkstra. Unlike Graph 1, contiguous worked better here, which shows that performance depends on how the graph is structured.

---

## Overall Analysis

Round-robin converged faster on Graph 1 but not on Graph 2 because the number of edge cuts changed between the graphs. In Graph 1, round-robin had fewer edge cuts, so it required less communication and ran faster. In Graph 2, contiguous had fewer edge cuts, so it performed better there instead.

Dijkstra sends more bytes than leader election because it has to send distance updates across ranks many times during the algorithm. Leader election only sends node IDs, which is much smaller and happens fewer times.

When the results say "Leader agreement: YES," it means all nodes ended up with the same leader ID, so the algorithm worked correctly across all ranks.

---

## Correctness Verification

I know leader election is correct because all nodes ended up agreeing on the same leader, which is shown by "Agreement: YES." Also, the leader that was chosen matches the node with the maximum ID in the graph, which is exactly what the algorithm is supposed to find.

I know Dijkstra is correct because the distance from the source node to itself is 0, and all nodes were marked as reachable. This means the algorithm was able to compute valid shortest paths from the source to every node in the graph.

---

## Design Decisions

I made the graph undirected because it makes both algorithms simpler and easier to reason about. For leader election, messages need to flow in all directions so information can spread across the whole graph. For Dijkstra, having edges go both ways makes sure paths exist between nodes and avoids issues with unreachable nodes.

I used `MPI_Allreduce` instead of point-to-point messages because it was much easier to implement and manage. With `MPI_Allreduce`, all ranks can combine their results at once and stay in sync, especially when finding the global minimum in Dijkstra.

---

## Limitations and Future Work

If I had more time, I would try to improve the partitioning strategy so it reduces edge cuts more consistently. I would also try a more optimized version of Dijkstra that doesn't rely as much on global communication, since that can slow things down as the number of ranks increases.