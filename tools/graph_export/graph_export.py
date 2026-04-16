import json
import re
import argparse
import random
from collections import defaultdict

def parse_dot_file(filepath):
    nodes = set()
    edges = []
    edge_pattern = re.compile(r'"(\d+)"\s*->\s*"(\d+)"\s*\["weight"="([\d.]+)"\]')
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            match = edge_pattern.search(line)
            if match:
                src = int(match.group(1))
                dst = int(match.group(2))
                weight = max(1, int(float(match.group(3))))
                nodes.add(src)
                nodes.add(dst)
                edges.append({"src": src, "dst": dst, "weight": weight})
    return sorted(nodes), edges

def remap_node_ids(nodes, edges):
    id_map = {old: new for new, old in enumerate(nodes)}
    new_edges = []
    for e in edges:
        new_edges.append({"src": id_map[e["src"]], "dst": id_map[e["dst"]], "weight": e["weight"]})
    return list(range(len(nodes))), new_edges

def ensure_connected(nodes, edges, seed):
    rng = random.Random(seed)
    parent = list(range(len(nodes)))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a, b):
        parent[find(a)] = find(b)

    for e in edges:
        union(e["src"], e["dst"])

    extra = []
    for n in nodes[1:]:
        if find(n) != find(0):
            w = rng.randint(1, 20)
            extra.append({"src": 0, "dst": n, "weight": w})
            extra.append({"src": n, "dst": 0, "weight": w})
            union(0, n)

    return edges + extra

def make_undirected(edges):
    existing = {(e["src"], e["dst"]) for e in edges}
    extra = []
    for e in edges:
        if (e["dst"], e["src"]) not in existing:
            extra.append({"src": e["dst"], "dst": e["src"], "weight": e["weight"]})
    return edges + extra

def main():
    parser = argparse.ArgumentParser(description='Convert NetGameSim DOT file to JSON')
    parser.add_argument('input_dot', help='Input .dot file path')
    parser.add_argument('output_json', help='Output .json file path')
    parser.add_argument('--seed', type=int, default=42, help='Random seed')
    args = parser.parse_args()

    print(f"Parsing {args.input_dot}...")
    nodes, edges = parse_dot_file(args.input_dot)
    print(f"Found {len(nodes)} nodes, {len(edges)} edges")

    nodes, edges = remap_node_ids(nodes, edges)
    edges = ensure_connected(nodes, edges, args.seed)
    edges = make_undirected(edges)

    graph = {
        "meta": {
            "num_nodes": len(nodes),
            "num_edges": len(edges),
            "seed": args.seed
        },
        "nodes": nodes,
        "edges": edges
    }

    with open(args.output_json, 'w') as f:
        json.dump(graph, f, indent=2)

    print(f"Done! Wrote {len(nodes)} nodes and {len(edges)} edges to {args.output_json}")

if __name__ == "__main__":
    main()