#include <utility>
#include "parse.h"
#include "feature_combiner.h"


std::pair<attribute_list_citerator, attribute_list_citerator> Location::find_attributes(const ParseState &state,
                                                                                        const Sentence &sent) const {

    int token_index = state.locations_[location];
    // Token index should never be outside the sentence.
    assert(token_index < static_cast<int>(sent.tokens.size()));
    // Except for the value -1, which means 'not found'.
    assert(token_index >= -1);

    if (token_index >= 0) {
        auto &token = sent.tokens[token_index];
        auto &attributes = token.find_namespace(ns, token_specific_ns);
        return std::make_pair(attributes.cbegin(), attributes.cend());
    } else {
        auto &first_token = sent.tokens[0];
        return std::make_pair(first_token._empty_attribute_vector.cbegin(), first_token._empty_attribute_vector.cend());
    }
}


void Location::fill_features(const ParseState &state, const Sentence &sent, std::vector<FeatureKey> &features,
                             size_t start_index) {
    size_t initial_vector_size = features.size();
    assert(initial_vector_size - start_index >= 1);

    auto it_pair = find_attributes(state, sent);
    if (it_pair.first != it_pair.second) {
        auto it = it_pair.first;

        for (size_t i = start_index; i < initial_vector_size; i++) {
            // Skip the first element. We'll get back to it at the end of the loop.
            it++;
            for (; it != it_pair.second; it++) {
                // Insert new features for the 1..n attributes in the namespace.
                // These end up at indices beyond `initial_vector_size`, so we won't see them again in this loop.
                features.push_back(features[i]);
                features.back().add_attribute(*it);
            }
            // Now add the first attribute to the current feature.
            features[i].add_attribute(*it_pair.first);
        }
    }
}

bool Location::good(const ParseState &state) const {
    return state.locations_[location] != -1;
}


void CartesianProduct::fill_features(const ParseState &state, const Sentence &sent, std::vector<FeatureKey> &features,
                                     size_t start_index) {
    lhs->fill_features(state, sent, features, start_index);
    rhs->fill_features(state, sent, features, start_index);
    n_applications += 1;
    n_features += features.size() - start_index;
}

void UnionList::fill_features(const ParseState &state, const Sentence &sent, std::vector<FeatureKey> &features,
                              size_t start_index) {
    for (const auto & operand : operands) {
        if (operand->good(state)) {
            features.push_back(FeatureKey());
            operand->fill_features(state, sent, features, features.size() - 1);
        }
    }
}

bool CartesianProduct::good(const ParseState &state) const {
    return lhs->good(state) && rhs->good(state);
}
