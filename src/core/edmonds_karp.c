/*
 * edmonds_karp.c -- Edmonds-Karp max-flow implementation.
 */
#include "edmonds_karp.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/*  Monotonic clock (microseconds)                                            */
/* ------------------------------------------------------------------------- */
#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
static double monotonic_us(void) {
    static LARGE_INTEGER freq;
    static int initialised = 0;
    LARGE_INTEGER counter;
    if (!initialised) {
        QueryPerformanceFrequency(&freq);
        initialised = 1;
    }
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1e6 / (double)freq.QuadPart;
}
#elif defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#  include <time.h>
static double monotonic_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1000.0;
}
#else
#  include <time.h>
static double monotonic_us(void) {
    return (double)clock() * 1e6 / (double)CLOCKS_PER_SEC;
}
#endif

/* ------------------------------------------------------------------------- */
/*  Edmonds-Karp                                                              */
/* ------------------------------------------------------------------------- */
graph_status_t edmonds_karp_run(FlowNetwork *fn,
                                SimulationResult *out,
                                metric_cb on_iter,
                                void *user_data) {
    if (!fn || !out) return GRAPH_EINVAL;
    if (!fn->g || !fn->g->row_ptr || !fn->g->col_idx
            || !fn->g->cap || !fn->g->rev)
        return GRAPH_EINVAL;
    if (!(fn->g->flags & GRAPH_FLOW_NETWORK)) return GRAPH_EINVAL;

    Graph *g = fn->g;
    vid_t n = g->n;

    eid_t         *parent  = (eid_t *)        malloc((size_t)n * sizeof(eid_t));
    vid_t         *queue   = (vid_t *)        malloc((size_t)n * sizeof(vid_t));
    unsigned char *visited = (unsigned char *)malloc((size_t)n * sizeof(unsigned char));

    if (!parent || !queue || !visited) {
        free(parent);
        free(queue);
        free(visited);
        return GRAPH_ENOMEM;
    }

    flow_network_reset(fn);

    out->algorithm_name = "Edmonds-Karp";
    out->status         = GRAPH_OK;
    out->max_flow       = 0.0;
    out->total_iters    = 0;
    out->total_us       = 0.0;

    cap_t flow_accum = 0.0;
    int32_t iter = 0;
    double t_start = monotonic_us();

    for (;;) {
        double iter_start = monotonic_us();

        memset(visited, 0, (size_t)n * sizeof(unsigned char));
        for (vid_t v = 0; v < n; ++v) parent[v] = GRAPHLAB_INVALID_EID;

        vid_t qhead = 0;
        vid_t qtail = 0;
        queue[qtail++]     = fn->source;
        visited[fn->source] = 1;

        int64_t edges_visited = 0;
        int     found         = 0;

        while (qhead < qtail && !found) {
            vid_t u = queue[qhead++];
            eid_t row_end = g->row_ptr[u + 1];
            for (eid_t i = g->row_ptr[u]; i < row_end; ++i) {
                ++edges_visited;
                vid_t v = g->col_idx[i];
                if (visited[v]) continue;
                if (flow_residual(fn, i) <= 0.0) continue;
                visited[v] = 1;
                parent[v]  = i;
                if (v == fn->sink) {
                    found = 1;
                    break;
                }
                queue[qtail++] = v;
            }
        }

        if (!found) break;

        /* Compute bottleneck along the path source -> ... -> sink. */
        cap_t   bottleneck = -1.0;
        int32_t path_len   = 0;
        {
            vid_t v = fn->sink;
            while (v != fn->source) {
                eid_t e = parent[v];
                cap_t r = flow_residual(fn, e);
                if (bottleneck < 0.0 || r < bottleneck) bottleneck = r;
                v = g->col_idx[g->rev[e]];
                ++path_len;
            }
        }

        /* Augment: push bottleneck along the path. */
        {
            vid_t v = fn->sink;
            while (v != fn->source) {
                eid_t e = parent[v];
                fn->flow[e]         += bottleneck;
                fn->flow[g->rev[e]] -= bottleneck;
                v = g->col_idx[g->rev[e]];
            }
        }

        flow_accum += bottleneck;
        double iter_end = monotonic_us();

        IterationMetric m;
        m.iter          = iter;
        m.path_len      = path_len;
        m.bottleneck    = bottleneck;
        m.flow_accum    = flow_accum;
        m.elapsed_us    = iter_end - iter_start;
        m.edges_visited = edges_visited;

        graph_status_t st = metric_buffer_push(&out->metrics, &m);
        if (st != GRAPH_OK) {
            out->status = st;
            free(parent);
            free(queue);
            free(visited);
            return st;
        }
        if (on_iter) on_iter(&m, user_data);
        ++iter;
    }

    double t_end = monotonic_us();
    out->max_flow    = flow_accum;
    out->total_iters = iter;
    out->total_us    = t_end - t_start;
    out->status      = GRAPH_OK;

    free(parent);
    free(queue);
    free(visited);
    return GRAPH_OK;
}
