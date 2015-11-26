//
// Created by Anders Johannsen on 26/11/2015.
//

#include <algorithm>
#include <vector>
#include <numeric>
#include "nonproj.h"

int Projectivizer::longest_nonprojective_edge(std::vector<token_index_t> &heads) {
    // Fill with token indexes
    longest_edges.resize(heads.size());
    std::iota(longest_edges.begin(), longest_edges.end(), 0);

    // Sort token indexes according to edge length
    std::sort(longest_edges.begin(), longest_edges.end(), [&heads](token_index_t token_a, token_index_t token_b) {
        return abs(token_a - heads[token_a]) > abs(token_b - heads[token_b]);
    });


    for (auto v : longest_edges) {
        auto u = heads[v];
        auto first_node = std::min(u, v);
        auto last_node = std::max(u, v);

        if (last_node - first_node == 1)
            return -1;

        // Check whether any node spanned by this edge has
        // a head or a dependency outside the span
        for (int i = first_node + 1; i < last_node; i++) {
            if (heads[i] < first_node || heads[i] > last_node)
                return v;
        }
    }

    return -1;
}

void Projectivizer::lift_longest(std::vector<token_index_t> &heads) {
    // This function transforms a non-projective dependency tree into a projective one
    // by re-attaching crossing edges higher in the tree.
    // This process is known as "lifting"
    auto v = longest_nonprojective_edge(heads);
    while (v != -1) {
        // The currently longest non-projective edge is (u, v).
        auto u = heads[v];
        // Take a step towards resolving the non-projectivity by attaching
        // v higher in the tree.
        heads[v] = heads[u];
        v = longest_nonprojective_edge(heads);
    }
}

bool Projectivizer::is_nonprojective(std::vector<token_index_t> &heads) {
    for (int v = 0; v < heads.size(); v++) {
        auto first_node = std::min(v, heads[v]);
        auto last_node = std::max(v, heads[v]);

        if (last_node - first_node == 1)
            continue;

        // Check whether any node spanned by this edge has
        // a head or a dependency outside the span
        for (int i = first_node + 1; i < last_node; i++) {
            if (heads[i] < first_node || heads[i] > last_node)
                return true;
        }
    }
    return false;
}
