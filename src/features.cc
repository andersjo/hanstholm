#include <stddef.h>
#include "features.h"

using namespace std;


AttributeExtractor::AttributeExtractor(string name, namespace_t ns, state_location::LocationName location)
    : name(name), location(location), ns(ns)
{
    
}

std::pair<attribute_list_citerator, attribute_list_citerator>
AttributeExtractor::extract(const ParseState &state, const Sentence &sent) const
{
    
    int token_index = state.locations_[location];
    if (token_index >= static_cast<int>(sent.tokens.size())) {
        // cout << "Token index beyond sentence size. Location: " << location << " pointing to content index: " << token_index << endl;
    }

    // Token index should be inside the sentence.
    // It may outside the sentence when the location is not defined (value = -1)
    // or when N0 is pushed beyond the last content.
    // FIXME: should never be pushed beyond the last content.

    if (token_index >= 0 && token_index < sent.tokens.size()) {
        auto &token = sent.tokens[token_index];

        for (auto & ns_front : token.namespaces_ng) {
            // cerr << "ns_front.index = " << ns_front.index << " vs. " << ns << "\n";
            if (ns_front.index == ns) {
                // cerr << "Found ns front with size " << ns_front.attributes.size() << "\n";
                return make_pair(ns_front.attributes.cbegin(), ns_front.attributes.cend());
            }
        }

    }

    return make_pair(_empty_attribute_vector.cend(), _empty_attribute_vector.cend());

};

void AttributeExtractor::fill_features(const ParseState &state, const Sentence &sent, std::vector<FeatureKey> &features, size_t start_index) {
    int token_index = state.locations_[location];
    assert(token_index < sent.tokens.size());

    if (token_index >= 0) {
        auto & attributes_in_ns = sent.tokens[token_index].namespaces[ns];
        size_t initial_vector_size = features.size();
        for (size_t i = start_index; i < initial_vector_size; i++) {
            for (size_t j = 1; j < attributes_in_ns.size(); j++) {
                // Make a copy of the current features by inserting it into the vector
                features.push_back(features[i]);
                features.back().add_attribute(attributes_in_ns[i]);
            }
            features[i].add_attribute(attributes_in_ns[i]);
        }
    }
}




template <typename T>
void FeatureKey::add_part(T part, size_t start_index) {
    throw std::runtime_error("Deprecated");
    // hash_combine(hashed_val, part);
}

template <typename T>
void FeatureKey::add_part(T part) {
    throw std::runtime_error("Deprecated");
}

void FeatureKey::add_attribute(Attribute attribute) {
    hash_combine(hashed_val, attribute.index);
    value *= attribute.value;
}



/**
* Return the weights for the given feature.
* Inserts an new weight section if it doesn't exist already.
* This behaviour should be okay during test, because all weights are initialized to zero.
*
*/
/*
WeightSection & WeightMap::get(FeatureKey key) {
    auto & section = weights[key];
    // Check if section is new
    if (section.weights.size() != section_size) {
        section.weights.resize(section_size);
        section.cumulative.resize(section_size);
    }

    return section;
}
*/

float *WeightMap::get_or_insert(FeatureKey key) {
    float * val_ptr = table_block.lookup(key.hashed_val);
    if (val_ptr == nullptr)
        return table_block.insert(key.hashed_val);
    else
        return val_ptr;
}

WeightSectionWrap WeightMap::get_or_insert_section(FeatureKey key) {
    float * val_ptr = table_block.lookup(key.hashed_val);
    if (val_ptr == nullptr)
        return WeightSectionWrap(table_block.insert(key.hashed_val), section_size);
    else
        return WeightSectionWrap(val_ptr, section_size);
}

WeightSectionWrap WeightMap::get_section(size_t key) {
    float * val_ptr = table_block.lookup(key);
    if (val_ptr == nullptr)
        throw std::out_of_range("Key " + std::to_string(key) + " not found");
    else
        return WeightSectionWrap(val_ptr, section_size);
}

WeightMap::WeightMap(size_t section_size_)
        : table_block(8388608, section_size_ * WeightSectionWrap::num_blocks), section_size(section_size_)  {
}

void ProductCombiner::fill_features(const ParseState &state, const Sentence &sent, std::vector<FeatureKey> &features, size_t start_index) {
    rhs.fill_features(state, sent, features, start_index);
    lhs.fill_features(state, sent, features, start_index);
}

DotProductCombiner::DotProductCombiner(AttributeExtractor lhs_, AttributeExtractor rhs_)
    : lhs(lhs_), rhs(rhs_) { }

void DotProductCombiner::fill_features(const ParseState &state, const Sentence &sent, std::vector<FeatureKey> &features, size_t start_index) {
    // Assume that the values in each namespace are sorted
    auto lhs_it_pair = lhs.extract(state, sent);
    auto lhs_it = lhs_it_pair.first;
    auto lhs_end = lhs_it_pair.second;

    auto rhs_it_pair = rhs.extract(state, sent);
    auto rhs_it = rhs_it_pair.first;
    auto rhs_end = rhs_it_pair.second;

    float dot_product = 0;
    while (lhs_it != lhs_end && rhs_it != rhs_end) {
        if (lhs_it->index == rhs_it->index) {
            dot_product += lhs_it->value * rhs_it->value;
            lhs_it++; rhs_it++;
        } else if (lhs_it->index > rhs_it->index) {
            rhs_it++;
        } else {
            lhs_it++;
        }
    }

    size_t vector_initial_size = features.size();
    for (size_t i = start_index; i < vector_initial_size; i++) {
        // FIXME what is a proper feature index of the vector product of two namespaces?
        features[i].add_part(6488931208);
        features[i].add_part(lhs.ns);
        features[i].add_part(rhs.ns);

    }
}

