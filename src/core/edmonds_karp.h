/*
 * edmonds_karp.h -- Edmonds-Karp max-flow (BFS-based augmenting paths).
 *
 * Time complexity: O(V * E^2). Used as the reference implementation for the
 * thesis baseline. Iterations are deterministic given the input CSR ordering
 * (which graph_build_for_flow sorts canonically).
 */
#ifndef GRAPHLAB_EDMONDS_KARP_H
#define GRAPHLAB_EDMONDS_KARP_H

#include "graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Callback fired after each augmenting path is processed. Optional - pass
 * NULL to disable. The same metric is also appended to out->metrics.
 */
typedef void (*metric_cb)(const IterationMetric *m, void *user_data);

/*
 * Run Edmonds-Karp on the given flow network. Resets fn->flow to zero before
 * starting. On success, out->max_flow holds the maximum flow value and
 * out->metrics contains one IterationMetric per augmenting path found.
 *
 * Returns GRAPH_OK on success or a graph_status_t code on failure. out is
 * left in a consistent (possibly empty) state on failure.
 */
graph_status_t edmonds_karp_run(FlowNetwork *fn,
                                SimulationResult *out,
                                metric_cb on_iter,
                                void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* GRAPHLAB_EDMONDS_KARP_H */
