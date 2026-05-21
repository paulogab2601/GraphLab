/*
 * test_graph.c -- Tests for CSR builders, validation, and metric buffer.
 */
#include "test_helpers.h"
#include "graph.h"

#include <string.h>

/* ------------------------------------------------------------------------- */
/*  Allocation / free                                                         */
/* ------------------------------------------------------------------------- */
static void test_alloc_free_empty(void) {
    TEST_BEGIN("alloc_free_empty");
    Graph *g = graph_alloc(5, 0, GRAPH_DIRECTED);
    EXPECT_TRUE(g != NULL);
    EXPECT_EQ_INT(g->n, 5);
    EXPECT_EQ_INT(g->m, 0);
    EXPECT_TRUE(g->row_ptr != NULL);
    for (vid_t v = 0; v <= g->n; ++v) {
        EXPECT_EQ_INT(g->row_ptr[v], 0);
    }
    graph_free(g);
}

static void test_alloc_invalid_args(void) {
    TEST_BEGIN("alloc_invalid_args");
    EXPECT_TRUE(graph_alloc(0, 0, 0) == NULL);
    EXPECT_TRUE(graph_alloc(-3, 0, 0) == NULL);
    EXPECT_TRUE(graph_alloc(5, -1, 0) == NULL);
}

/* ------------------------------------------------------------------------- */
/*  General builder                                                           */
/* ------------------------------------------------------------------------- */
static void test_build_directed_weighted(void) {
    TEST_BEGIN("build_directed_weighted");
    /* 0 -> 1 (10), 0 -> 2 (20), 1 -> 2 (30), 2 -> 0 (40) */
    vid_t src[] = {0, 0, 1, 2};
    vid_t dst[] = {1, 2, 2, 0};
    cap_t cap[] = {10.0, 20.0, 30.0, 40.0};

    Graph *g = graph_build_from_edge_list(
        3, 4, src, dst, cap, GRAPH_DIRECTED | GRAPH_WEIGHTED);
    EXPECT_TRUE(g != NULL);
    EXPECT_EQ_INT(graph_validate(g), GRAPH_OK);
    EXPECT_EQ_INT(g->n, 3);
    EXPECT_EQ_INT(g->m, 4);
    EXPECT_EQ_INT(g->row_ptr[0], 0);
    EXPECT_EQ_INT(g->row_ptr[1], 2);
    EXPECT_EQ_INT(g->row_ptr[2], 3);
    EXPECT_EQ_INT(g->row_ptr[3], 4);
    EXPECT_EQ_INT(g->col_idx[0], 1);
    EXPECT_EQ_INT(g->col_idx[1], 2);
    EXPECT_EQ_INT(g->col_idx[2], 2);
    EXPECT_EQ_INT(g->col_idx[3], 0);
    EXPECT_EQ_DBL(g->cap[0], 10.0, 0.0);
    EXPECT_EQ_DBL(g->cap[1], 20.0, 0.0);
    EXPECT_EQ_DBL(g->cap[2], 30.0, 0.0);
    EXPECT_EQ_DBL(g->cap[3], 40.0, 0.0);
    EXPECT_TRUE(g->rev == NULL);
    graph_free(g);
}

static void test_build_undirected_unweighted(void) {
    TEST_BEGIN("build_undirected_unweighted");
    /* edges 0-1, 1-2 stored in both directions (final m = 4). */
    vid_t src[] = {0, 1};
    vid_t dst[] = {1, 2};
    Graph *g = graph_build_from_edge_list(3, 2, src, dst, NULL, 0);
    EXPECT_TRUE(g != NULL);
    EXPECT_EQ_INT(graph_validate(g), GRAPH_OK);
    EXPECT_EQ_INT(g->n, 3);
    EXPECT_EQ_INT(g->m, 4);
    EXPECT_EQ_INT(g->row_ptr[0], 0);
    EXPECT_EQ_INT(g->row_ptr[1], 1);
    EXPECT_EQ_INT(g->row_ptr[2], 3);
    EXPECT_EQ_INT(g->row_ptr[3], 4);
    /* vertex 0 -> {1};  vertex 1 -> {0, 2};  vertex 2 -> {1} */
    EXPECT_EQ_INT(g->col_idx[0], 1);
    EXPECT_EQ_INT(g->col_idx[1], 0);
    EXPECT_EQ_INT(g->col_idx[2], 2);
    EXPECT_EQ_INT(g->col_idx[3], 1);
    EXPECT_TRUE(g->cap == NULL);
    graph_free(g);
}

static void test_build_rejects_flow_flag(void) {
    TEST_BEGIN("build_rejects_flow_flag");
    vid_t src[] = {0};
    vid_t dst[] = {1};
    cap_t cap[] = {1.0};
    Graph *g = graph_build_from_edge_list(
        2, 1, src, dst, cap,
        GRAPH_DIRECTED | GRAPH_WEIGHTED | GRAPH_FLOW_NETWORK);
    EXPECT_TRUE(g == NULL);
}

static void test_build_out_of_range(void) {
    TEST_BEGIN("build_out_of_range");
    vid_t src[] = {0};
    vid_t dst[] = {5};  /* n = 2, but dst = 5 */
    cap_t cap[] = {1.0};
    Graph *g = graph_build_from_edge_list(
        2, 1, src, dst, cap, GRAPH_DIRECTED | GRAPH_WEIGHTED);
    EXPECT_TRUE(g == NULL);
}

/* ------------------------------------------------------------------------- */
/*  Flow network builder                                                      */
/* ------------------------------------------------------------------------- */
static void test_build_flow_simple(void) {
    TEST_BEGIN("build_flow_simple");
    /* Two parallel paths: 0 -> 1 -> 3, 0 -> 2 -> 3. */
    vid_t src[] = {0, 0, 1, 2};
    vid_t dst[] = {1, 2, 3, 3};
    cap_t cap[] = {5.0, 7.0, 3.0, 4.0};

    Graph *g = graph_build_for_flow(4, 4, src, dst, cap);
    EXPECT_TRUE(g != NULL);
    EXPECT_EQ_INT(graph_validate(g), GRAPH_OK);
    EXPECT_EQ_INT(g->n, 4);
    EXPECT_EQ_INT(g->m, 8);  /* 4 forward + 4 reverse */
    EXPECT_TRUE(g->rev != NULL);
    EXPECT_TRUE(g->cap != NULL);

    /* Involution: rev[rev[i]] == i */
    for (eid_t i = 0; i < g->m; ++i) {
        EXPECT_EQ_INT(g->rev[g->rev[i]], i);
    }

    /* For every forward arc with cap > 0, the reverse should have cap == 0. */
    for (eid_t i = 0; i < g->m; ++i) {
        if (g->cap[i] > 0.0) {
            EXPECT_EQ_DBL(g->cap[g->rev[i]], 0.0, 0.0);
        }
    }
    graph_free(g);
}

static void test_build_flow_rejects_self_loop(void) {
    TEST_BEGIN("build_flow_rejects_self_loop");
    vid_t src[] = {0};
    vid_t dst[] = {0};
    cap_t cap[] = {1.0};
    Graph *g = graph_build_for_flow(2, 1, src, dst, cap);
    EXPECT_TRUE(g == NULL);
}

static void test_build_flow_rejects_negative_cap(void) {
    TEST_BEGIN("build_flow_rejects_negative_cap");
    vid_t src[] = {0};
    vid_t dst[] = {1};
    cap_t cap[] = {-1.0};
    Graph *g = graph_build_for_flow(2, 1, src, dst, cap);
    EXPECT_TRUE(g == NULL);
}

/* ------------------------------------------------------------------------- */
/*  Metric buffer                                                             */
/* ------------------------------------------------------------------------- */
static void test_metric_buffer(void) {
    TEST_BEGIN("metric_buffer");
    MetricBuffer buf;
    EXPECT_EQ_INT(metric_buffer_init(&buf, 0), GRAPH_OK);
    EXPECT_EQ_INT(buf.count, 0);
    EXPECT_EQ_INT(buf.capacity, 0);

    for (int i = 0; i < 100; ++i) {
        IterationMetric m;
        m.iter = i;
        m.path_len = i + 1;
        m.bottleneck = (double)i;
        m.flow_accum = (double)(i * 2);
        m.elapsed_us = 1.5;
        m.edges_visited = i * 10;
        EXPECT_EQ_INT(metric_buffer_push(&buf, &m), GRAPH_OK);
    }
    EXPECT_EQ_INT(buf.count, 100);
    EXPECT_TRUE(buf.capacity >= 100);
    EXPECT_EQ_INT(buf.items[42].iter, 42);
    EXPECT_EQ_INT(buf.items[42].path_len, 43);

    metric_buffer_clear(&buf);
    EXPECT_EQ_INT(buf.count, 0);
    EXPECT_TRUE(buf.capacity >= 100);

    metric_buffer_free(&buf);
    EXPECT_EQ_INT(buf.capacity, 0);
    EXPECT_TRUE(buf.items == NULL);
}

static void test_simulation_result(void) {
    TEST_BEGIN("simulation_result");
    SimulationResult r;
    simulation_result_init(&r, "test-algo");
    EXPECT_TRUE(r.algorithm_name != NULL);
    EXPECT_EQ_INT(strcmp(r.algorithm_name, "test-algo"), 0);
    EXPECT_EQ_INT(r.metrics.count, 0);

    IterationMetric m;
    m.iter = 1;
    m.path_len = 2;
    m.bottleneck = 3.0;
    m.flow_accum = 3.0;
    m.elapsed_us = 0.0;
    m.edges_visited = 0;
    EXPECT_EQ_INT(metric_buffer_push(&r.metrics, &m), GRAPH_OK);
    EXPECT_EQ_INT(r.metrics.count, 1);

    simulation_result_free(&r);
    EXPECT_EQ_INT(r.metrics.count, 0);
}

/* ------------------------------------------------------------------------- */
/*  Status strings                                                            */
/* ------------------------------------------------------------------------- */
static void test_strerror(void) {
    TEST_BEGIN("strerror");
    EXPECT_TRUE(graph_strerror(GRAPH_OK) != NULL);
    EXPECT_TRUE(graph_strerror(GRAPH_ENOMEM) != NULL);
    EXPECT_TRUE(graph_strerror(GRAPH_EINVAL) != NULL);
    EXPECT_TRUE(strcmp(graph_strerror(GRAPH_OK), "OK") == 0);
}

/* ------------------------------------------------------------------------- */
int main(void) {
    test_alloc_free_empty();
    test_alloc_invalid_args();
    test_build_directed_weighted();
    test_build_undirected_unweighted();
    test_build_rejects_flow_flag();
    test_build_out_of_range();
    test_build_flow_simple();
    test_build_flow_rejects_self_loop();
    test_build_flow_rejects_negative_cap();
    test_metric_buffer();
    test_simulation_result();
    test_strerror();
    TEST_REPORT();
}
