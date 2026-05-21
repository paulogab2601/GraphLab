/*
 * graph.c -- Implementation of CSR graph builders and metric buffers.
 */
#include "graph.h"

#include <stdlib.h>
#include <string.h>

/* ========================================================================= */
/*  Status strings                                                            */
/* ========================================================================= */
const char *graph_strerror(graph_status_t status) {
    switch (status) {
        case GRAPH_OK:        return "OK";
        case GRAPH_ENOMEM:    return "Out of memory";
        case GRAPH_EINVAL:    return "Invalid argument";
        case GRAPH_EIO:       return "I/O error";
        case GRAPH_EOVERFLOW: return "Arithmetic overflow";
        case GRAPH_ENOTFOUND: return "Not found";
    }
    return "Unknown error";
}

/* ========================================================================= */
/*  MetricBuffer                                                              */
/* ========================================================================= */
graph_status_t metric_buffer_init(MetricBuffer *buf, size_t initial_cap) {
    if (!buf) return GRAPH_EINVAL;
    buf->items = NULL;
    buf->count = 0;
    buf->capacity = 0;
    if (initial_cap == 0) return GRAPH_OK;
    buf->items = (IterationMetric *)malloc(initial_cap * sizeof(IterationMetric));
    if (!buf->items) return GRAPH_ENOMEM;
    buf->capacity = initial_cap;
    return GRAPH_OK;
}

graph_status_t metric_buffer_push(MetricBuffer *buf, const IterationMetric *m) {
    if (!buf || !m) return GRAPH_EINVAL;
    if (buf->count == buf->capacity) {
        size_t new_cap = (buf->capacity == 0) ? 16 : buf->capacity * 2;
        IterationMetric *p = (IterationMetric *)realloc(
            buf->items, new_cap * sizeof(IterationMetric));
        if (!p) return GRAPH_ENOMEM;
        buf->items = p;
        buf->capacity = new_cap;
    }
    buf->items[buf->count++] = *m;
    return GRAPH_OK;
}

void metric_buffer_clear(MetricBuffer *buf) {
    if (buf) buf->count = 0;
}

void metric_buffer_free(MetricBuffer *buf) {
    if (!buf) return;
    free(buf->items);
    buf->items = NULL;
    buf->count = 0;
    buf->capacity = 0;
}

/* ========================================================================= */
/*  SimulationResult                                                          */
/* ========================================================================= */
void simulation_result_init(SimulationResult *r, const char *algo) {
    if (!r) return;
    r->algorithm_name = algo;
    r->max_flow = 0.0;
    r->total_iters = 0;
    r->total_us = 0.0;
    r->metrics.items = NULL;
    r->metrics.count = 0;
    r->metrics.capacity = 0;
    r->seed_used = 0;
    r->status = GRAPH_OK;
}

void simulation_result_free(SimulationResult *r) {
    if (!r) return;
    metric_buffer_free(&r->metrics);
    r->algorithm_name = NULL;
}

/* ========================================================================= */
/*  Graph alloc / free                                                        */
/* ========================================================================= */
Graph *graph_alloc(vid_t n, eid_t m, uint32_t flags) {
    if (n <= 0 || m < 0) return NULL;

    Graph *g = (Graph *)calloc(1, sizeof(Graph));
    if (!g) return NULL;

    g->n = n;
    g->m = m;
    g->flags = flags;

    g->row_ptr = (eid_t *)calloc((size_t)n + 1, sizeof(eid_t));
    if (!g->row_ptr) {
        graph_free(g);
        return NULL;
    }

    if (m > 0) {
        g->col_idx = (vid_t *)malloc((size_t)m * sizeof(vid_t));
        if (!g->col_idx) {
            graph_free(g);
            return NULL;
        }
        if (flags & (GRAPH_WEIGHTED | GRAPH_FLOW_NETWORK)) {
            g->cap = (cap_t *)malloc((size_t)m * sizeof(cap_t));
            if (!g->cap) {
                graph_free(g);
                return NULL;
            }
        }
        if (flags & GRAPH_FLOW_NETWORK) {
            g->rev = (eid_t *)malloc((size_t)m * sizeof(eid_t));
            if (!g->rev) {
                graph_free(g);
                return NULL;
            }
        }
    }

    return g;
}

void graph_free(Graph *g) {
    if (!g) return;
    free(g->row_ptr);
    free(g->col_idx);
    free(g->cap);
    free(g->rev);
    free(g);
}

/* ========================================================================= */
/*  Validation                                                                */
/* ========================================================================= */
graph_status_t graph_validate(const Graph *g) {
    if (!g) return GRAPH_EINVAL;
    if (g->n <= 0 || g->m < 0) return GRAPH_EINVAL;
    if (!g->row_ptr) return GRAPH_EINVAL;
    if (g->m > 0 && !g->col_idx) return GRAPH_EINVAL;

    if (g->row_ptr[0] != 0) return GRAPH_EINVAL;
    if (g->row_ptr[g->n] != g->m) return GRAPH_EINVAL;

    for (vid_t v = 0; v < g->n; ++v) {
        if (g->row_ptr[v] > g->row_ptr[v + 1]) return GRAPH_EINVAL;
    }
    for (eid_t i = 0; i < g->m; ++i) {
        if (g->col_idx[i] < 0 || g->col_idx[i] >= g->n) return GRAPH_EINVAL;
    }

    if (g->flags & GRAPH_FLOW_NETWORK) {
        if (!g->rev || !g->cap) return GRAPH_EINVAL;
        vid_t v = 0;
        for (eid_t i = 0; i < g->m; ++i) {
            while (v < g->n - 1 && g->row_ptr[v + 1] <= i) ++v;
            eid_t r = g->rev[i];
            if (r < 0 || r >= g->m) return GRAPH_EINVAL;
            if (g->rev[r] != i) return GRAPH_EINVAL;
            if (g->col_idx[r] != v) return GRAPH_EINVAL;
        }
    }
    return GRAPH_OK;
}

/* ========================================================================= */
/*  Edge-list ingestion shared between general and flow builders              */
/* ========================================================================= */

/* Arc tuple used during sort. */
typedef struct {
    vid_t src;
    vid_t dst;
    cap_t cap;
    eid_t input_idx;   /* index in the user-provided edge list */
    int   is_reverse;  /* 0 = forward, 1 = reverse (flow networks only) */
} arc_tuple_t;

static int arc_tuple_cmp(const void *a, const void *b) {
    const arc_tuple_t *x = (const arc_tuple_t *)a;
    const arc_tuple_t *y = (const arc_tuple_t *)b;
    if (x->src != y->src) return (x->src < y->src) ? -1 : 1;
    if (x->dst != y->dst) return (x->dst < y->dst) ? -1 : 1;
    if (x->input_idx != y->input_idx)
        return (x->input_idx < y->input_idx) ? -1 : 1;
    if (x->is_reverse != y->is_reverse)
        return (x->is_reverse < y->is_reverse) ? -1 : 1;
    return 0;
}

/*
 * Sort the arc tuples, then materialise the CSR. If track_rev is set, rev[]
 * is populated by pairing forward/reverse arcs that share input_idx.
 *
 * Takes ownership of `arcs` only on success path; caller must free on error.
 * (Actually neither owns; caller always frees after the call.)
 */
static Graph *build_from_tuples(vid_t n,
                                arc_tuple_t *arcs,
                                eid_t m_total,
                                uint32_t flags,
                                int track_rev,
                                eid_t m_input) {
    Graph *g = graph_alloc(n, m_total, flags);
    if (!g) return NULL;

    if (m_total > 0) {
        qsort(arcs, (size_t)m_total, sizeof(arc_tuple_t), arc_tuple_cmp);
    }

    /* row_ptr from source field (arcs are sorted by src, so we can scan). */
    for (eid_t i = 0; i < m_total; ++i) {
        g->row_ptr[arcs[i].src + 1] += 1;
    }
    for (vid_t v = 0; v < n; ++v) {
        g->row_ptr[v + 1] += g->row_ptr[v];
    }

    eid_t *input_to_slot = NULL;
    if (track_rev && m_input > 0) {
        input_to_slot = (eid_t *)malloc((size_t)(2 * m_input) * sizeof(eid_t));
        if (!input_to_slot) {
            graph_free(g);
            return NULL;
        }
    }

    for (eid_t i = 0; i < m_total; ++i) {
        g->col_idx[i] = arcs[i].dst;
        if (g->cap) g->cap[i] = arcs[i].cap;
        if (track_rev && input_to_slot) {
            input_to_slot[2 * arcs[i].input_idx + arcs[i].is_reverse] = i;
        }
    }

    if (track_rev && input_to_slot) {
        for (eid_t k = 0; k < m_input; ++k) {
            eid_t s_fwd = input_to_slot[2 * k];
            eid_t s_rev = input_to_slot[2 * k + 1];
            g->rev[s_fwd] = s_rev;
            g->rev[s_rev] = s_fwd;
        }
        free(input_to_slot);
    }

    return g;
}

/* ========================================================================= */
/*  Public builders                                                           */
/* ========================================================================= */
Graph *graph_build_from_edge_list(vid_t n,
                                  eid_t m_input,
                                  const vid_t *edges_src,
                                  const vid_t *edges_dst,
                                  const cap_t *edges_cap,
                                  uint32_t flags) {
    if (n <= 0 || m_input < 0) return NULL;
    if (m_input > 0 && (!edges_src || !edges_dst)) return NULL;
    if ((flags & GRAPH_WEIGHTED) && m_input > 0 && !edges_cap) return NULL;
    if (flags & GRAPH_FLOW_NETWORK) return NULL; /* use graph_build_for_flow */

    int directed = (flags & GRAPH_DIRECTED) != 0;
    int weighted = (flags & GRAPH_WEIGHTED) != 0;
    eid_t m_total = directed ? m_input : 2 * m_input;

    arc_tuple_t *arcs = NULL;
    if (m_total > 0) {
        arcs = (arc_tuple_t *)malloc((size_t)m_total * sizeof(arc_tuple_t));
        if (!arcs) return NULL;
    }

    eid_t pos = 0;
    for (eid_t k = 0; k < m_input; ++k) {
        vid_t u = edges_src[k];
        vid_t v = edges_dst[k];
        cap_t c = weighted ? edges_cap[k] : (cap_t)1.0;
        if (u < 0 || u >= n || v < 0 || v >= n) {
            free(arcs);
            return NULL;
        }
        arcs[pos].src = u;
        arcs[pos].dst = v;
        arcs[pos].cap = c;
        arcs[pos].input_idx = k;
        arcs[pos].is_reverse = 0;
        ++pos;
        if (!directed) {
            arcs[pos].src = v;
            arcs[pos].dst = u;
            arcs[pos].cap = c;
            arcs[pos].input_idx = k;
            arcs[pos].is_reverse = 1;
            ++pos;
        }
    }

    Graph *g = build_from_tuples(n, arcs, m_total, flags, 0, m_input);
    free(arcs);
    return g;
}

Graph *graph_build_for_flow(vid_t n,
                            eid_t m_input,
                            const vid_t *edges_src,
                            const vid_t *edges_dst,
                            const cap_t *edges_cap) {
    if (n <= 0 || m_input < 0) return NULL;
    if (m_input > 0 && (!edges_src || !edges_dst || !edges_cap)) return NULL;

    uint32_t flags = GRAPH_DIRECTED | GRAPH_WEIGHTED | GRAPH_FLOW_NETWORK;
    eid_t m_total = 2 * m_input;

    arc_tuple_t *arcs = NULL;
    if (m_total > 0) {
        arcs = (arc_tuple_t *)malloc((size_t)m_total * sizeof(arc_tuple_t));
        if (!arcs) return NULL;
    }

    eid_t pos = 0;
    for (eid_t k = 0; k < m_input; ++k) {
        vid_t u = edges_src[k];
        vid_t v = edges_dst[k];
        cap_t c = edges_cap[k];
        if (u < 0 || u >= n || v < 0 || v >= n || u == v || c < 0.0) {
            free(arcs);
            return NULL;
        }
        arcs[pos].src = u;
        arcs[pos].dst = v;
        arcs[pos].cap = c;
        arcs[pos].input_idx = k;
        arcs[pos].is_reverse = 0;
        ++pos;
        arcs[pos].src = v;
        arcs[pos].dst = u;
        arcs[pos].cap = (cap_t)0.0;
        arcs[pos].input_idx = k;
        arcs[pos].is_reverse = 1;
        ++pos;
    }

    Graph *g = build_from_tuples(n, arcs, m_total, flags, 1, m_input);
    free(arcs);
    return g;
}

/* ========================================================================= */
/*  FlowNetwork                                                               */
/* ========================================================================= */
FlowNetwork *flow_network_alloc(Graph *g, vid_t source, vid_t sink) {
    if (!g) return NULL;
    if (source < 0 || source >= g->n) return NULL;
    if (sink   < 0 || sink   >= g->n) return NULL;
    if (source == sink) return NULL;
    if (!(g->flags & GRAPH_FLOW_NETWORK)) return NULL;

    FlowNetwork *fn = (FlowNetwork *)calloc(1, sizeof(FlowNetwork));
    if (!fn) return NULL;
    fn->g = g;
    fn->source = source;
    fn->sink = sink;
    fn->flow = (cap_t *)calloc((size_t)g->m, sizeof(cap_t));
    if (!fn->flow) {
        free(fn);
        return NULL;
    }
    return fn;
}

void flow_network_free(FlowNetwork *fn) {
    if (!fn) return;
    free(fn->flow);
    free(fn);
}

void flow_network_reset(FlowNetwork *fn) {
    if (!fn) return;
    memset(fn->flow, 0, (size_t)fn->g->m * sizeof(cap_t));
}
