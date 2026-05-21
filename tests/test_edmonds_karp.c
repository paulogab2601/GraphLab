/*
 * test_edmonds_karp.c -- Reference tests for the Edmonds-Karp implementation.
 *
 * Reference cases:
 *   1. Trivial single-edge network.
 *   2. CLRS 3rd ed Fig 26.6 (p. 727) -- expected max-flow = 23.
 *   3. Disconnected source/sink -- expected max-flow = 0.
 *   4. Property: after termination, no augmenting path exists in the residual
 *      graph (BFS from source cannot reach sink).
 */
#include "test_helpers.h"
#include "graph.h"
#include "edmonds_karp.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/*  Helper: build + run Edmonds-Karp end-to-end                               */
/* ------------------------------------------------------------------------- */
typedef struct {
    Graph            *g;
    FlowNetwork      *fn;
    SimulationResult  result;
} ek_case_t;

static int ek_case_init(ek_case_t *c,
                        vid_t n,
                        eid_t m_input,
                        const vid_t *src,
                        const vid_t *dst,
                        const cap_t *cap,
                        vid_t source,
                        vid_t sink) {
    c->g = graph_build_for_flow(n, m_input, src, dst, cap);
    if (!c->g) return 0;
    c->fn = flow_network_alloc(c->g, source, sink);
    if (!c->fn) { graph_free(c->g); c->g = NULL; return 0; }
    simulation_result_init(&c->result, NULL);
    return 1;
}

static void ek_case_free(ek_case_t *c) {
    simulation_result_free(&c->result);
    flow_network_free(c->fn);
    graph_free(c->g);
    c->fn = NULL;
    c->g = NULL;
}

/* BFS in residual from source; returns 1 if sink is reachable. */
static int residual_bfs_reaches_sink(const FlowNetwork *fn) {
    const Graph *g = fn->g;
    unsigned char *visited = (unsigned char *)calloc((size_t)g->n, 1);
    vid_t *queue = (vid_t *)malloc((size_t)g->n * sizeof(vid_t));
    if (!visited || !queue) { free(visited); free(queue); return -1; }

    vid_t qhead = 0, qtail = 0;
    queue[qtail++] = fn->source;
    visited[fn->source] = 1;

    int reached = 0;
    while (qhead < qtail && !reached) {
        vid_t u = queue[qhead++];
        for (eid_t i = g->row_ptr[u]; i < g->row_ptr[u + 1]; ++i) {
            vid_t v = g->col_idx[i];
            if (visited[v]) continue;
            if (flow_residual(fn, i) <= 0.0) continue;
            visited[v] = 1;
            if (v == fn->sink) { reached = 1; break; }
            queue[qtail++] = v;
        }
    }

    free(visited);
    free(queue);
    return reached;
}

/* ------------------------------------------------------------------------- */
/*  Case 1: single edge                                                       */
/* ------------------------------------------------------------------------- */
static void test_ek_single_edge(void) {
    TEST_BEGIN("ek_single_edge");
    vid_t src[] = {0};
    vid_t dst[] = {1};
    cap_t cap[] = {7.0};

    ek_case_t c;
    EXPECT_TRUE(ek_case_init(&c, 2, 1, src, dst, cap, 0, 1));

    graph_status_t st = edmonds_karp_run(c.fn, &c.result, NULL, NULL);
    EXPECT_EQ_INT(st, GRAPH_OK);
    EXPECT_EQ_DBL(c.result.max_flow, 7.0, 0.0);
    EXPECT_EQ_INT(c.result.total_iters, 1);
    EXPECT_EQ_INT(c.result.metrics.count, 1);
    EXPECT_EQ_INT(c.result.metrics.items[0].path_len, 1);
    EXPECT_EQ_DBL(c.result.metrics.items[0].bottleneck, 7.0, 0.0);
    EXPECT_EQ_DBL(c.result.metrics.items[0].flow_accum, 7.0, 0.0);

    ek_case_free(&c);
}

/* ------------------------------------------------------------------------- */
/*  Case 2: CLRS 3rd ed Fig 26.6, p. 727                                      */
/*                                                                            */
/*  Vertices: s=0, v1=1, v2=2, v3=3, v4=4, t=5                                */
/*  Edges:                                                                    */
/*    s  -> v1 : 16     v2 -> v1 :  4     v3 -> t  : 20                       */
/*    s  -> v2 : 13     v2 -> v4 : 14     v4 -> v3 :  7                       */
/*    v1 -> v3 : 12     v3 -> v2 :  9     v4 -> t  :  4                       */
/*                                                                            */
/*  Expected max-flow = 23.                                                   */
/* ------------------------------------------------------------------------- */
static void test_ek_clrs_p727(void) {
    TEST_BEGIN("ek_clrs_p727");
    vid_t src[] = {0, 0, 1, 2, 2, 3, 3, 4, 4};
    vid_t dst[] = {1, 2, 3, 1, 4, 2, 5, 3, 5};
    cap_t cap[] = {16, 13, 12, 4, 14, 9, 20, 7, 4};

    ek_case_t c;
    EXPECT_TRUE(ek_case_init(&c, 6, 9, src, dst, cap, 0, 5));

    graph_status_t st = edmonds_karp_run(c.fn, &c.result, NULL, NULL);
    EXPECT_EQ_INT(st, GRAPH_OK);
    EXPECT_EQ_DBL(c.result.max_flow, 23.0, 0.0);
    EXPECT_TRUE(c.result.total_iters >= 1);
    EXPECT_TRUE(c.result.total_iters <= 9);

    /* Every iteration must increase the accumulated flow by its bottleneck. */
    cap_t prev = 0.0;
    for (size_t i = 0; i < c.result.metrics.count; ++i) {
        const IterationMetric *m = &c.result.metrics.items[i];
        EXPECT_TRUE(m->bottleneck > 0.0);
        EXPECT_TRUE(m->path_len >= 1);
        EXPECT_EQ_DBL(m->flow_accum, prev + m->bottleneck, 1e-12);
        prev = m->flow_accum;
    }
    EXPECT_EQ_DBL(prev, 23.0, 0.0);

    /* Property: after EK terminates, sink is unreachable in residual. */
    EXPECT_EQ_INT(residual_bfs_reaches_sink(c.fn), 0);

    ek_case_free(&c);
}

/* ------------------------------------------------------------------------- */
/*  Case 3: disconnected source/sink                                          */
/* ------------------------------------------------------------------------- */
static void test_ek_disconnected(void) {
    TEST_BEGIN("ek_disconnected");
    /* 0 -> 1 and 2 -> 3, but nothing bridges. Source=0, sink=3. */
    vid_t src[] = {0, 2};
    vid_t dst[] = {1, 3};
    cap_t cap[] = {5.0, 5.0};

    ek_case_t c;
    EXPECT_TRUE(ek_case_init(&c, 4, 2, src, dst, cap, 0, 3));

    graph_status_t st = edmonds_karp_run(c.fn, &c.result, NULL, NULL);
    EXPECT_EQ_INT(st, GRAPH_OK);
    EXPECT_EQ_DBL(c.result.max_flow, 0.0, 0.0);
    EXPECT_EQ_INT(c.result.total_iters, 0);
    EXPECT_EQ_INT(c.result.metrics.count, 0);

    ek_case_free(&c);
}

/* ------------------------------------------------------------------------- */
/*  Case 4: triangle with bottleneck downstream                               */
/* ------------------------------------------------------------------------- */
static void test_ek_path_bottleneck(void) {
    TEST_BEGIN("ek_path_bottleneck");
    /* s=0 -> a=1 (10), a -> b=2 (3), b -> t=3 (10) -- max flow = 3. */
    vid_t src[] = {0, 1, 2};
    vid_t dst[] = {1, 2, 3};
    cap_t cap[] = {10.0, 3.0, 10.0};

    ek_case_t c;
    EXPECT_TRUE(ek_case_init(&c, 4, 3, src, dst, cap, 0, 3));

    graph_status_t st = edmonds_karp_run(c.fn, &c.result, NULL, NULL);
    EXPECT_EQ_INT(st, GRAPH_OK);
    EXPECT_EQ_DBL(c.result.max_flow, 3.0, 0.0);
    EXPECT_EQ_INT(c.result.total_iters, 1);
    EXPECT_EQ_INT(c.result.metrics.items[0].path_len, 3);
    EXPECT_EQ_DBL(c.result.metrics.items[0].bottleneck, 3.0, 0.0);

    ek_case_free(&c);
}

/* ------------------------------------------------------------------------- */
/*  Case 5: parallel edges (multi-graph)                                      */
/* ------------------------------------------------------------------------- */
static void test_ek_parallel_edges(void) {
    TEST_BEGIN("ek_parallel_edges");
    /* Two parallel arcs from 0 -> 1 with caps 3 and 5. Then 1 -> 2 with 100. */
    vid_t src[] = {0, 0, 1};
    vid_t dst[] = {1, 1, 2};
    cap_t cap[] = {3.0, 5.0, 100.0};

    ek_case_t c;
    EXPECT_TRUE(ek_case_init(&c, 3, 3, src, dst, cap, 0, 2));

    graph_status_t st = edmonds_karp_run(c.fn, &c.result, NULL, NULL);
    EXPECT_EQ_INT(st, GRAPH_OK);
    EXPECT_EQ_DBL(c.result.max_flow, 8.0, 0.0);

    ek_case_free(&c);
}

/* ------------------------------------------------------------------------- */
/*  Case 6: callback fires once per iteration                                 */
/* ------------------------------------------------------------------------- */
static void count_cb(const IterationMetric *m, void *user_data) {
    (void)m;
    int *count = (int *)user_data;
    *count += 1;
}

static void test_ek_callback(void) {
    TEST_BEGIN("ek_callback");
    vid_t src[] = {0, 0, 1, 2, 2, 3, 3, 4, 4};
    vid_t dst[] = {1, 2, 3, 1, 4, 2, 5, 3, 5};
    cap_t cap[] = {16, 13, 12, 4, 14, 9, 20, 7, 4};

    ek_case_t c;
    EXPECT_TRUE(ek_case_init(&c, 6, 9, src, dst, cap, 0, 5));

    int callback_calls = 0;
    graph_status_t st = edmonds_karp_run(
        c.fn, &c.result, count_cb, &callback_calls);
    EXPECT_EQ_INT(st, GRAPH_OK);
    EXPECT_EQ_INT(callback_calls, c.result.total_iters);
    EXPECT_EQ_INT(callback_calls, (int)c.result.metrics.count);

    ek_case_free(&c);
}

/* ------------------------------------------------------------------------- */
int main(void) {
    test_ek_single_edge();
    test_ek_clrs_p727();
    test_ek_disconnected();
    test_ek_path_bottleneck();
    test_ek_parallel_edges();
    test_ek_callback();
    TEST_REPORT();
}
