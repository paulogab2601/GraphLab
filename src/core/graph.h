/*
 * graph.h -- Core graph data structures for GraphLab.
 *
 * Compressed Sparse Row (CSR) representation, deterministic builders and the
 * IterationMetric/SimulationResult contracts shared by every algorithm in
 * src/core/. No globals, no exceptions, no allocations in hot loops.
 *
 * Conventions:
 *   - All public functions either succeed (GRAPH_OK) and write outputs, or
 *     fail with a graph_status_t code and leave outputs untouched.
 *   - Builders sort adjacency lists by destination vertex for determinism.
 *   - Flow networks store every input arc (u, v, c) along with its reverse
 *     arc (v, u, 0). rev[i] is the index of the reverse of arc i.
 */
#ifndef GRAPHLAB_GRAPH_H
#define GRAPHLAB_GRAPH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/*  Primitive types                                                           */
/* ------------------------------------------------------------------------- */
typedef int32_t  vid_t;   /* vertex id; supports up to ~2.1 * 10^9 vertices  */
typedef int64_t  eid_t;   /* edge   id; supports up to ~9.2 * 10^18 arcs     */
typedef double   cap_t;   /* capacity / weight                                */

#define GRAPHLAB_INVALID_VID ((vid_t)-1)
#define GRAPHLAB_INVALID_EID ((eid_t)-1)

/* ------------------------------------------------------------------------- */
/*  Status codes                                                              */
/* ------------------------------------------------------------------------- */
typedef enum {
    GRAPH_OK         = 0,
    GRAPH_ENOMEM     = 1,
    GRAPH_EINVAL     = 2,
    GRAPH_EIO        = 3,
    GRAPH_EOVERFLOW  = 4,
    GRAPH_ENOTFOUND  = 5
} graph_status_t;

const char *graph_strerror(graph_status_t status);

/* ------------------------------------------------------------------------- */
/*  Graph flags                                                               */
/* ------------------------------------------------------------------------- */
#define GRAPH_DIRECTED      0x0001u  /* arcs only go src -> dst (else also dst -> src) */
#define GRAPH_WEIGHTED      0x0002u  /* cap[] is populated                              */
#define GRAPH_FLOW_NETWORK  0x0004u  /* rev[] populated; pairs forward/reverse arcs     */
#define GRAPH_TREE          0x0008u  /* structurally a tree (advisory, not validated)   */

/* ------------------------------------------------------------------------- */
/*  Graph (CSR)                                                               */
/* ------------------------------------------------------------------------- */
typedef struct {
    vid_t     n;        /* number of vertices                                  */
    eid_t     m;        /* number of directed arcs actually stored             */
    eid_t    *row_ptr;  /* size n+1; outgoing arcs of u are in [row_ptr[u], row_ptr[u+1]) */
    vid_t    *col_idx;  /* size m;   destination of arc i is col_idx[i]        */
    cap_t    *cap;      /* size m;   NULL when !GRAPH_WEIGHTED                 */
    eid_t    *rev;      /* size m;   reverse arc index; NULL unless flow net   */
    uint32_t  flags;
} Graph;

/* ------------------------------------------------------------------------- */
/*  Flow network view                                                         */
/* ------------------------------------------------------------------------- */
typedef struct {
    Graph *g;        /* non-owning reference                                    */
    cap_t *flow;     /* size g->m; flow[rev[i]] == -flow[i] is maintained      */
    vid_t  source;
    vid_t  sink;
} FlowNetwork;

/* ------------------------------------------------------------------------- */
/*  Iteration metrics                                                         */
/* ------------------------------------------------------------------------- */
typedef struct {
    int32_t iter;          /* 0-based iteration index                          */
    int32_t path_len;      /* number of edges in the augmenting path           */
    cap_t   bottleneck;    /* min residual capacity along the path             */
    cap_t   flow_accum;    /* total flow after this iteration                  */
    double  elapsed_us;    /* wall-clock time spent on this iteration          */
    int64_t edges_visited; /* arcs touched during the search                   */
} IterationMetric;

typedef struct {
    IterationMetric *items;
    size_t           count;
    size_t           capacity;
} MetricBuffer;

graph_status_t metric_buffer_init(MetricBuffer *buf, size_t initial_cap);
graph_status_t metric_buffer_push(MetricBuffer *buf, const IterationMetric *m);
void           metric_buffer_clear(MetricBuffer *buf);
void           metric_buffer_free(MetricBuffer *buf);

/* ------------------------------------------------------------------------- */
/*  Simulation result                                                         */
/* ------------------------------------------------------------------------- */
typedef struct {
    const char    *algorithm_name; /* non-owning; typically a string literal   */
    cap_t          max_flow;       /* meaningful for max-flow algorithms       */
    int32_t        total_iters;
    double         total_us;
    MetricBuffer   metrics;        /* owned                                     */
    uint64_t       seed_used;
    graph_status_t status;
} SimulationResult;

void simulation_result_init(SimulationResult *r, const char *algo);
void simulation_result_free(SimulationResult *r);

/* ------------------------------------------------------------------------- */
/*  Graph construction                                                        */
/* ------------------------------------------------------------------------- */

/*
 * Allocate an empty Graph shell. row_ptr is zero-filled; col_idx/cap/rev
 * are allocated to size m but left uninitialized. Callers normally use
 * graph_build_* instead of touching this directly.
 *
 * Returns NULL on allocation failure or invalid arguments.
 */
Graph *graph_alloc(vid_t n, eid_t m, uint32_t flags);
void   graph_free(Graph *g);

/*
 * Build a general (non-flow) graph from an edge list.
 *
 *   GRAPH_DIRECTED present  -> m_input arcs stored as given (final m = m_input).
 *   GRAPH_DIRECTED absent   -> each input edge stored in both directions
 *                              (final m = 2 * m_input).
 *   GRAPH_WEIGHTED present  -> edges_cap must be non-NULL; cap[] populated.
 *   GRAPH_FLOW_NETWORK      -> rejected; use graph_build_for_flow instead.
 *
 * Adjacency lists are sorted by destination vertex for determinism.
 * Returns NULL on failure.
 */
Graph *graph_build_from_edge_list(
    vid_t        n,
    eid_t        m_input,
    const vid_t *edges_src,
    const vid_t *edges_dst,
    const cap_t *edges_cap,  /* NULL allowed iff !GRAPH_WEIGHTED */
    uint32_t     flags);

/*
 * Build a flow network. Always directed and weighted. For each input arc
 * (u, v, c) a reverse arc (v, u, 0) is also stored, and rev[] pairs them.
 * Final m = 2 * m_input.
 *
 * Self-loops and negative capacities are rejected. Returns NULL on failure.
 */
Graph *graph_build_for_flow(
    vid_t        n,
    eid_t        m_input,
    const vid_t *edges_src,
    const vid_t *edges_dst,
    const cap_t *edges_cap);

/*
 * Sanity check the CSR. Confirms row_ptr monotone, col_idx in range, and -
 * for flow networks - that rev[] is an involution that pairs reciprocal arcs.
 */
graph_status_t graph_validate(const Graph *g);

/* ------------------------------------------------------------------------- */
/*  Flow network helpers                                                      */
/* ------------------------------------------------------------------------- */
FlowNetwork *flow_network_alloc(Graph *g, vid_t source, vid_t sink);
void         flow_network_free(FlowNetwork *fn);
void         flow_network_reset(FlowNetwork *fn);

/* Residual capacity of arc i in this flow network. */
static inline cap_t flow_residual(const FlowNetwork *fn, eid_t i) {
    return fn->g->cap[i] - fn->flow[i];
}

#ifdef __cplusplus
}
#endif

#endif /* GRAPHLAB_GRAPH_H */
