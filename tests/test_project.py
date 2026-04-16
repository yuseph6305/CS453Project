import sys
import unittest
from collections import defaultdict

sys.path.insert(0, "tools/graph_export")
sys.path.insert(0, "tools/partition")

from graph_export import parse_dot_file, remap_node_ids, make_undirected, ensure_connected
from partition import partition_contiguous, partition_roundrobin, compute_edge_cuts


class TestPartition(unittest.TestCase):

    def test_contiguous_chunk_size(self):
        nodes = list(range(12))
        owner = partition_contiguous(nodes, 4)
        self.assertEqual(owner[0], 0)
        self.assertEqual(owner[2], 0)
        self.assertEqual(owner[3], 1)

    def test_roundrobin_cycles(self):
        nodes = list(range(8))
        owner = partition_roundrobin(nodes, 4)
        self.assertEqual(owner[0], 0)
        self.assertEqual(owner[1], 1)
        self.assertEqual(owner[4], 0)
        self.assertEqual(owner[5], 1)

    def test_edge_cuts_none(self):
        edges = [{"src": 0, "dst": 1, "weight": 1}]
        owner = {0: 0, 1: 0}
        self.assertEqual(compute_edge_cuts(edges, owner), 0)

    def test_edge_cuts_all(self):
        edges = [{"src": 0, "dst": 1, "weight": 1}]
        owner = {0: 0, 1: 1}
        self.assertEqual(compute_edge_cuts(edges, owner), 1)


class TestGraphExport(unittest.TestCase):

    def test_remap_ids(self):
        nodes = [5, 42, 99]
        edges = [{"src": 5, "dst": 42, "weight": 1}]
        new_nodes, new_edges = remap_node_ids(nodes, edges)
        self.assertEqual(new_nodes, [0, 1, 2])
        self.assertEqual(new_edges[0]["src"], 0)
        self.assertEqual(new_edges[0]["dst"], 1)

    def test_make_undirected(self):
        edges = [{"src": 0, "dst": 1, "weight": 2}]
        result = make_undirected(edges)
        self.assertEqual(len(result), 2)
        self.assertEqual(result[1]["src"], 1)
        self.assertEqual(result[1]["dst"], 0)

    def test_ensure_connected(self):
        nodes = [0, 1, 2, 3]
        edges = [{"src": 0, "dst": 1, "weight": 1}]
        result = ensure_connected(nodes, edges, 42)
        self.assertGreater(len(result), len(edges))

    def test_leader_agreement(self):
        # FloodMax on a line graph - all nodes must agree on max id
        n = 5
        adj = defaultdict(list)
        for i in range(n - 1):
            adj[i].append(i + 1)
            adj[i + 1].append(i)
        candidate = list(range(n))
        for _ in range(n):
            new = candidate[:]
            for u in range(n):
                for v in adj[u]:
                    if candidate[v] > new[u]:
                        new[u] = candidate[v]
            candidate = new
        self.assertTrue(all(c == 4 for c in candidate))

    def test_dijkstra_source_distance_zero(self):
        # dist[source] must always be 0
        edges = [(0, 1, 2), (1, 2, 3), (0, 2, 10)]
        adj = defaultdict(list)
        for u, v, w in edges:
            adj[u].append((v, w))
            adj[v].append((u, w))
        dist = [float('inf')] * 3
        dist[0] = 0
        settled = [False] * 3
        for _ in range(3):
            u = min((dist[i], i) for i in range(3) if not settled[i])[1]
            settled[u] = True
            for v, w in adj[u]:
                if dist[u] + w < dist[v]:
                    dist[v] = dist[u] + w
        self.assertEqual(dist[0], 0)
        self.assertEqual(dist[1], 2)
        self.assertEqual(dist[2], 5)


if __name__ == "__main__":
    unittest.main(verbosity=2)