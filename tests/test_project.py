import sys
import unittest
sys.path.insert(0, "tools/graph_export")
sys.path.insert(0, "tools/partition")

from graph_export import parse_dot_file, remap_node_ids, make_undirected, ensure_connected
from partition import partition_contiguous, partition_roundrobin, compute_edge_cuts

class TestPartition(unittest.TestCase):

    def test_contiguous_chunk_size(self):
        nodes = list(range(12))
        owner = partition_contiguous(nodes, 4)
        # first 3 nodes should go to rank 0
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

if __name__ == "__main__":
    unittest.main(verbosity=2)