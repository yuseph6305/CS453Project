#ifndef GRAPH_H
#define GRAPH_H

#define MAX_NODES 10000
#define MAX_EDGES 200000
#define INF 2147483647

/* A single edge in the adjacency list */
typedef struct {
    int dst;
    int weight;
} Edge;

/* Graph stored as adjacency list */
typedef struct {
    int num_nodes;
    int num_edges;
    int adj_start[MAX_NODES + 1];
    Edge adj[MAX_EDGES];
} Graph;

/* Partition - which rank owns which nodes */
typedef struct {
    int num_ranks;
    int owner[MAX_NODES];
    int my_nodes[MAX_NODES];
    int my_node_count;
} Partition;

#endif
