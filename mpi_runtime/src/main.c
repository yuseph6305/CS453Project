#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Maximum graph size we support */
#define MAX_NODES 10000
#define MAX_EDGES 200000
#define INF INT_MAX

typedef struct {
    int dst;
    int weight;
} Edge;

/* Stores the full graph as an adjacency list */
typedef struct {
    int num_nodes;
    int num_edges;
    int adj_start[MAX_NODES + 1]; /* adj_start[i] to adj_start[i+1] are edges of node i */
    Edge adj[MAX_EDGES];
} Graph;

/* Stores which nodes this rank owns */
typedef struct {
    int num_ranks;
    int owner[MAX_NODES];      /* owner[node] = which rank owns it */
    int my_nodes[MAX_NODES];   /* list of nodes this rank owns */
    int my_node_count;         /* how many nodes this rank owns */
} Partition;

/* Global rank - set in main() */
static int g_rank;

/* Logging helpers - only rank 0 prints global results */
#define LOG(fmt, ...) \
    fprintf(stdout, "[rank %d] " fmt "\n", g_rank, ##__VA_ARGS__)

#define LOG0(fmt, ...) \
    do { if (g_rank == 0) fprintf(stdout, "[rank 0] " fmt "\n", ##__VA_ARGS__); } while(0)

/* Read entire file into a string */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "ERROR: Cannot open %s\n", path);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = (char *)malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

/* Find a key in JSON and return pointer to its value */
static const char *find_key(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return NULL;
    p += strlen(search);
    while (*p == ' ' || *p == ':' || *p == '\t') p++;
    return p;
}

/* Read an integer value for a key from JSON */
static int read_int(const char *json, const char *key) {
    const char *p = find_key(json, key);
    if (!p) return -1;
    return atoi(p);
}

/* Parse the owner map - "owner": {"0": 1, "1": 0, ...} */
static void parse_owner(const char *json, int *owner, int max_nodes) {
    const char *p = find_key(json, "owner");
    if (!p) return;
    while (*p && *p != '{') p++;
    if (!*p) return;
    p++;
    while (*p && *p != '}') {
        while (*p && *p != '"' && *p != '}') p++;
        if (*p != '"') break;
        p++;
        int node = atoi(p);
        while (*p && *p != '"') p++;
        if (*p == '"') p++;
        while (*p == ':' || *p == ' ') p++;
        int rank = atoi(p);
        if (node >= 0 && node < max_nodes) owner[node] = rank;
        while (*p && *p != ',' && *p != '}') p++;
        if (*p == ',') p++;
    }
}

/* Parse edges array from JSON */
static int parse_edges(const char *json, int *srcs, int *dsts, 
                        int *weights, int max_edges) {
    const char *p = find_key(json, "edges");
    if (!p) return 0;
    while (*p && *p != '[') p++;
    if (!*p) return 0;
    p++;

    int count = 0;
    while (*p && *p != ']' && count < max_edges) {
        while (*p && *p != '{' && *p != ']') p++;
        if (*p != '{') break;
        p++;

        int src = -1, dst = -1, weight = 1;
        for (int f = 0; f < 3; f++) {
            while (*p && *p != '"' && *p != '}') p++;
            if (*p != '"') break;
            p++;
            char key[32]; int ki = 0;
            while (*p && *p != '"' && ki < 31) key[ki++] = *p++;
            key[ki] = '\0';
            if (*p == '"') p++;
            while (*p == ':' || *p == ' ') p++;
            int val = atoi(p);
            while (*p && *p != ',' && *p != '}') p++;
            if (!strcmp(key, "src"))    src = val;
            else if (!strcmp(key, "dst"))    dst = val;
            else if (!strcmp(key, "weight")) weight = val;
        }
        if (src >= 0 && dst >= 0) {
            srcs[count] = src;
            dsts[count] = dst;
            weights[count] = weight;
            count++;
        }
        while (*p && *p != '}') p++;
        if (*p == '}') p++;
    }
    return count;
}

/* Load graph from JSON file */
static void load_graph(const char *path, Graph *g) {
    char *json = read_file(path);
    g->num_nodes = read_int(json, "num_nodes");
    if (g->num_nodes <= 0 || g->num_nodes > MAX_NODES) {
        fprintf(stderr, "ERROR: Invalid num_nodes: %d\n", g->num_nodes);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int *srcs    = malloc(MAX_EDGES * sizeof(int));
    int *dsts    = malloc(MAX_EDGES * sizeof(int));
    int *weights = malloc(MAX_EDGES * sizeof(int));

    int nedges = parse_edges(json, srcs, dsts, weights, MAX_EDGES);
    g->num_edges = nedges;

    /* count edges per node */
    int deg[MAX_NODES] = {0};
    for (int i = 0; i < nedges; i++)
        if (srcs[i] >= 0 && srcs[i] < g->num_nodes) deg[srcs[i]]++;

    /* build adj_start */
    g->adj_start[0] = 0;
    for (int i = 0; i < g->num_nodes; i++)
        g->adj_start[i+1] = g->adj_start[i] + deg[i];

    /* fill adjacency list */
    int pos[MAX_NODES] = {0};
    for (int i = 0; i < nedges; i++) {
        int s = srcs[i];
        if (s < 0 || s >= g->num_nodes) continue;
        int idx = g->adj_start[s] + pos[s]++;
        g->adj[idx].dst    = dsts[i];
        g->adj[idx].weight = (weights[i] > 0) ? weights[i] : 1;
    }

    free(srcs); free(dsts); free(weights);
    free(json);
}

/* Load partition from JSON file */
static void load_partition(const char *path, Partition *part, 
                            int my_rank, int num_nodes) {
    char *json = read_file(path);
    part->num_ranks = read_int(json, "ranks");
    memset(part->owner, 0, sizeof(part->owner));
    parse_owner(json, part->owner, MAX_NODES);

    /* build list of nodes this rank owns */
    part->my_node_count = 0;
    for (int i = 0; i < num_nodes; i++)
        if (part->owner[i] == my_rank)
            part->my_nodes[part->my_node_count++] = i;

    free(json);
}

static void run_leader_election(const Graph *g, const Partition *part,
                                 int rounds, long *total_msgs, 
                                 long *total_bytes) {
    int N = g->num_nodes;
    int *candidate = malloc(N * sizeof(int));
    int *global_candidate = malloc(N * sizeof(int));

    /* every node starts with its own id as best candidate */
    for (int i = 0; i < part->my_node_count; i++)
        candidate[part->my_nodes[i]] = part->my_nodes[i];

    *total_msgs = 0;
    *total_bytes = 0;
    double start = MPI_Wtime();

    for (int round = 0; round < rounds; round++) {
        int changed = 0;

        /* local propagation within this rank */
        for (int i = 0; i < part->my_node_count; i++) {
            int n = part->my_nodes[i];
            for (int e = g->adj_start[n]; e < g->adj_start[n+1]; e++) {
                int nb = g->adj[e].dst;
                if (part->owner[nb] == g_rank && 
                    candidate[nb] > candidate[n]) {
                    candidate[n] = candidate[nb];
                    changed = 1;
                }
            }
        }

        /* share candidates across all ranks */
        for (int i = 0; i < N; i++) global_candidate[i] = -1;
        for (int i = 0; i < part->my_node_count; i++)
            global_candidate[part->my_nodes[i]] = candidate[part->my_nodes[i]];

        MPI_Allreduce(MPI_IN_PLACE, global_candidate, N, 
                      MPI_INT, MPI_MAX, MPI_COMM_WORLD);
        (*total_msgs)++;
        (*total_bytes) += N * sizeof(int);

        /* update from cross-rank neighbors */
        for (int i = 0; i < part->my_node_count; i++) {
            int n = part->my_nodes[i];
            for (int e = g->adj_start[n]; e < g->adj_start[n+1]; e++) {
                int nb = g->adj[e].dst;
                if (global_candidate[nb] > candidate[n]) {
                    candidate[n] = global_candidate[nb];
                    changed = 1;
                }
            }
        }

        /* check if anyone changed */
        int gc = 0;
        MPI_Allreduce(&changed, &gc, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        (*total_msgs)++;
        (*total_bytes) += sizeof(int);

        if (gc == 0) {
            LOG0("Leader election converged at round %d", round + 1);
            break;
        }
    }

    double elapsed = MPI_Wtime() - start;

    /* collect final candidates */
    for (int i = 0; i < N; i++) global_candidate[i] = -1;
    for (int i = 0; i < part->my_node_count; i++)
        global_candidate[part->my_nodes[i]] = candidate[part->my_nodes[i]];
    MPI_Allreduce(MPI_IN_PLACE, global_candidate, N, 
                  MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    /* find leader and check agreement */
    int leader = 0;
    for (int i = 0; i < N; i++)
        if (global_candidate[i] > leader) leader = global_candidate[i];

    int agree = 1;
    for (int i = 0; i < N; i++)
        if (global_candidate[i] >= 0 && global_candidate[i] != leader) {
            agree = 0;
            break;
        }

    LOG0("=== Leader Election Results ===");
    LOG0("Leader id  : %d", leader);
    LOG0("Agreement  : %s", agree ? "YES" : "NO");
    LOG0("Runtime    : %.4f s", elapsed);
    LOG0("MPI calls  : %ld", *total_msgs);
    LOG0("Bytes sent : %ld", *total_bytes);

    free(candidate);
    free(global_candidate);
}

static void run_dijkstra(const Graph *g, const Partition *part,
                          int source, long *total_msgs, long *total_bytes) {
    int N = g->num_nodes;
    int *dist    = malloc(N * sizeof(int));
    int *settled = calloc(N, sizeof(int));

    /* initialize all distances to infinity */
    for (int i = 0; i < N; i++) dist[i] = INF;

    /* source node gets distance 0 */
    if (part->owner[source] == g_rank) dist[source] = 0;

    *total_msgs = 0;
    *total_bytes = 0;
    int iterations = 0;
    double start = MPI_Wtime();

    for (int iter = 0; iter < N; iter++) {
        iterations++;

        /* each rank finds its best unsettled node */
        int local_dist = INF, local_node = -1;
        for (int i = 0; i < part->my_node_count; i++) {
            int n = part->my_nodes[i];
            if (!settled[n] && dist[n] < local_dist) {
                local_dist = dist[n];
                local_node = n;
            }
        }

        /* global minimum - find best node across all ranks */
        int send[2] = {local_dist, local_node};
        int recv[2];
        MPI_Allreduce(send, recv, 1, MPI_2INT, MPI_MINLOC, MPI_COMM_WORLD);
        (*total_msgs)++;
        (*total_bytes) += sizeof(int) * 2;

        /* if no reachable unsettled node, we are done */
        if (recv[0] == INF || recv[1] == -1) break;

        int u = recv[1];
        settled[u] = 1;

        /* owning rank relaxes edges */
        if (part->owner[u] == g_rank) {
            for (int e = g->adj_start[u]; e < g->adj_start[u+1]; e++) {
                int v = g->adj[e].dst;
                int w = g->adj[e].weight;
                if (!settled[v] && dist[u] != INF && dist[u] + w < dist[v])
                    dist[v] = dist[u] + w;
            }
        }

        /* share updated distances */
        MPI_Allreduce(MPI_IN_PLACE, dist, N, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
        (*total_msgs)++;
        (*total_bytes) += N * sizeof(int);
    }

    double elapsed = MPI_Wtime() - start;

    if (g_rank == 0) {
        LOG0("=== Dijkstra Results ===");
        LOG0("Source     : %d", source);
        LOG0("Iterations : %d", iterations);
        LOG0("Runtime    : %.4f s", elapsed);
        LOG0("MPI calls  : %ld", *total_msgs);
        LOG0("Bytes sent : %ld", *total_bytes);

        int reachable = 0;
        for (int i = 0; i < N; i++)
            if (dist[i] != INF) reachable++;

        LOG0("Reachable  : %d / %d", reachable, N);
        LOG0("Correctness: dist[source] = %d (expect 0)", dist[source]);

        LOG0("--- First 10 distances ---");
        for (int i = 0; i < 10 && i < N; i++) {
            if (dist[i] == INF) printf("  dist[%d] = INF\n", i);
            else printf("  dist[%d] = %d\n", i, dist[i]);
        }
    }

    free(dist);
    free(settled);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int nranks;
    MPI_Comm_rank(MPI_COMM_WORLD, &g_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    /* parse command line arguments */
    char graph_path[512] = "";
    char part_path[512]  = "";
    char algo[64]        = "leader";
    int source = 0, rounds = -1;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--graph") && i+1 < argc)
            strncpy(graph_path, argv[++i], 511);
        else if (!strcmp(argv[i], "--part") && i+1 < argc)
            strncpy(part_path, argv[++i], 511);
        else if (!strcmp(argv[i], "--algo") && i+1 < argc)
            strncpy(algo, argv[++i], 63);
        else if (!strcmp(argv[i], "--source") && i+1 < argc)
            source = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rounds") && i+1 < argc)
            rounds = atoi(argv[++i]);
    }

    /* validate input */
    if (!strlen(graph_path) || !strlen(part_path)) {
        if (g_rank == 0)
            fprintf(stderr, "Usage: mpirun -n N ./ngs_mpi "
                    "--graph G --part P --algo <leader|dijkstra> "
                    "[--source S] [--rounds R]\n");
        MPI_Finalize();
        return 1;
    }

    /* load graph and partition */
    Graph g;
    memset(&g, 0, sizeof(g));
    load_graph(graph_path, &g);
    LOG0("Graph: %d nodes, %d edges", g.num_nodes, g.num_edges);

    Partition part;
    memset(&part, 0, sizeof(part));
    load_partition(part_path, &part, g_rank, g.num_nodes);
    LOG("owns %d nodes", part.my_node_count);

    if (rounds < 0) rounds = g.num_nodes;

    long msgs = 0, bytes = 0;
    MPI_Barrier(MPI_COMM_WORLD);

    /* run chosen algorithm */
    if (!strcmp(algo, "leader"))
        run_leader_election(&g, &part, rounds, &msgs, &bytes);
    else if (!strcmp(algo, "dijkstra"))
        run_dijkstra(&g, &part, source, &msgs, &bytes);
    else {
        if (g_rank == 0)
            fprintf(stderr, "Unknown algorithm: %s\n", algo);
        MPI_Finalize();
        return 1;
    }

    /* collect total metrics across all ranks */
    long total_msgs = 0, total_bytes = 0;
    MPI_Reduce(&msgs, &total_msgs, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&bytes, &total_bytes, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    LOG0("=== Metrics Summary ===");
    LOG0("Algorithm  : %s", algo);
    LOG0("Ranks      : %d", nranks);
    LOG0("Total msgs : %ld", total_msgs);
    LOG0("Total bytes: %ld (%.2f KB)", total_bytes, total_bytes/1024.0);

    MPI_Finalize();
    return 0;
}