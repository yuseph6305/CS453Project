import json
import argparse
from collections import defaultdict

def partition_contiguous(nodes, num_ranks):
    chunk = (len(nodes) + num_ranks - 1) // num_ranks
    owner = {}
    for i, node in enumerate(nodes):
        owner[node] = min(i // chunk, num_ranks - 1)
    return owner

def partition_roundrobin(nodes, num_ranks):
    owner = {}
    for i, node in enumerate(nodes):
        owner[node] = i % num_ranks
    return owner

def compute_edge_cuts(edges, owner):
    cuts = 0
    for e in edges:
        if owner[e["src"]] != owner[e["dst"]]:
            cuts += 1
    return cuts

def main():
    parser = argparse.ArgumentParser(description="Partition graph nodes into ranks")
    parser.add_argument("graph_json", help="Input graph JSON file")
    parser.add_argument("--ranks", type=int, required=True, help="Number of MPI ranks")
    parser.add_argument("--strategy", choices=["contiguous", "roundrobin"], default="contiguous")
    parser.add_argument("--out", required=True, help="Output partition JSON file")
    args = parser.parse_args()
    # load the graph
    with open(args.graph_json) as f:
        graph = json.load(f)

    nodes = graph["nodes"]
    edges = graph["edges"]

    # pick strategy
    if args.strategy == "contiguous":
        owner = partition_contiguous(nodes, args.ranks)
    else:
        owner = partition_roundrobin(nodes, args.ranks)

    # compute edge cuts
    cuts = compute_edge_cuts(edges, owner)

    # build ghost nodes - remote neighbors for each rank
    ghost_nodes = defaultdict(set)
    for e in edges:
        if owner[e["src"]] != owner[e["dst"]]:
            ghost_nodes[owner[e["src"]]].add(e["dst"])

    # build rank node lists
    rank_nodes = defaultdict(list)
    for n in nodes:
        rank_nodes[owner[n]].append(n)
    # save partition
    partition = {
        "meta": {
            "strategy": args.strategy,
            "ranks": args.ranks,
            "edge_cuts": cuts,
            "num_nodes": len(nodes),
            "num_edges": len(edges)
        },
        "owner": {str(k): v for k, v in owner.items()},
        "rank_nodes": {r: rank_nodes[r] for r in range(args.ranks)},
        "ghost_nodes": {r: list(ghost_nodes[r]) for r in range(args.ranks)}
    }

    with open(args.out, 'w') as f:
        json.dump(partition, f, indent=2)

    print(f"Strategy={args.strategy}, Ranks={args.ranks}, Edge cuts={cuts}/{len(edges)}")
    for r in range(args.ranks):
        print(f"  Rank {r}: {len(rank_nodes[r])} nodes, {len(ghost_nodes[r])} ghosts")

if __name__ == "__main__":
    main()