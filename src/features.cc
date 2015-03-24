#include <stddef.h>
#include <Python/Python.h>
#include "features.h"

using namespace std;


FeatureBuilder2::FeatureBuilder2(std::vector<combined_feature_t> feature_set) : feature_set(feature_set) { }

void FeatureBuilder2::build(ParseState & state, const Sentence & sent, std::vector<FeatureKey> & features) {
    size_t combined_feature_num = 0;
    for (auto & combined_feature : feature_set) {
        features.push_back(FeatureKey(combined_feature_num));
        auto start_index = features.size() - 1;
        size_t value_offset = 0;

        for (auto & attribute_extractor : combined_feature) {

            auto it_pair = attribute_extractor.extract(state, sent);
            auto it_begin = it_pair.first;
            auto it_end = it_pair.second;
            if (it_begin == it_end) {
                // No attributes found - break and backtrack
                features.resize(start_index);
                break;
            }
            
            for (size_t i = start_index; i < features.size(); i++) {
                features[i].add_part(static_cast<uint32_t>(it_begin->index), value_offset);
                features[i].value *= it_begin->value;
                
                // In case we only have a single feature this loop is not entered
                for (auto it = it_begin + 1; it != it_end; it++) {
                    auto copied_feature = features[i];
                    copied_feature.add_part(static_cast<uint32_t>(it->index), value_offset);
                    copied_feature.value *= it->value;
                    features.push_back(copied_feature);
                }
            }
            
            value_offset += sizeof(uint32_t);
        }
        
        combined_feature_num++;
    }
}






vector<combined_feature_t> nivre_feature_set(CorpusDictionary & dict) {
    vector<combined_feature_t> feature_combinations;
    using namespace state_location;
    using E = AttributeExtractor;
    
    
    auto S0w = E("S0:w", dict.map_namespace("w"), S0);
    auto S0p = E("S0:p", dict.map_namespace("p"), S0);
    auto N0w = E("N0:w", dict.map_namespace("w"), N0);
    auto N0p = E("N0:p", dict.map_namespace("p"), N0);
    auto N1w = E("N1:w", dict.map_namespace("w"), N1);
    auto N1p = E("N1:p", dict.map_namespace("p"), N1);
    auto N2w = E("N2:w", dict.map_namespace("w"), N2);
    auto N2p = E("N2:p", dict.map_namespace("p"), N2);
    
    auto S0_head_p = E("S0_head:p", dict.map_namespace("p"), S0_head);
    auto S0_right_p = E("S0_right:p", dict.map_namespace("p"), S0_right);
    auto S0_left_p = E("S0_left:p", dict.map_namespace("p"), S0_left);
    auto N0_left_p = E("N0_left:p", dict.map_namespace("p"), N0_left);


    // {((S0w, S0p), (N0w, N0p))},
    // ProductCombiner(ProductCombiner(S0w, S0p), ProductCombiner(N0w, N0p));
    // S0w, S0p, N0w, N0p
    // ProductCombiner(S0w, ProductCombiner(S0p, ProductCombiner(N0w, N0p));
    //

    feature_combinations = {
        {S0w, S0p},
        {S0w, S0p},
        {S0w},
        {S0p},
        {N0w, N0p},
        {N0w},
        {N0p},
        {N1w, N1p},
        {N1w},
        {N1p},
        {N2w, N2p},
        {N2w},
        {N2p},
        
        // Word pairs,
        {S0w, S0p, N0w, N0p},
        {S0w, S0p, N0w},
        {S0w, N0w, N0p},
        {S0w, S0p, N0p},
        {S0p, N0w, N0p},
        {S0w, N0w},
        {S0p, N0p},
        {N0p, N1p},
        
        // Three words,
        {N0p, N1p, N2p},
        {S0p, N0p, N1p},
        {S0_head_p, S0p, N0p},
        {S0p, S0_left_p, N0p},
        {S0p, S0_right_p, N0p},
        {S0p, N0p, N0_left_p}
        
    };


    // auto inner_combiner = ProductCombiner<AttributeExtractor, AttributeExtractor>(N0p, N1p);
    //auto outer_combiner = ProductCombiner<AttributeExtractor, ProductCombiner>(N0p, inner_combiner);
    // auto inner_combiner = ProductCombiner(N0p, N1p);
    // auto outer_combiner = ProductCombiner(N1w, inner_combiner);
    // auto top_combiner = ProductCombiner(outer_combiner, DotProductCombiner(N0p, N1p));
    // top_combiner.fill_features(nullptr, nullptr, nullptr, 0);
    //cout << top_combiner

//    auto smth = ProductCombiner<AttributeExtractor, ProductCombiner>(S0p,
//            ProductCombiner<AttributeExtractor, AttributeExtractor>(N0p, N1p));
    // auto smth = ProductCombiner(S0p, DotProductCombiner(N0p, N1p));

    return feature_combinations;
}



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

    // assert(token_index <= static_cast<int>(sent.tokens.size()));

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

    /*
    if (token_index >= 0 && token_index < sent.tokens.size()) {
        auto & token = sent.tokens[token_index];

        attribute_list_citerator it_begin = token.attributes.cbegin();
        attribute_list_citerator it_end = token.attributes.cend();

        // Go forward as long as
        // 1) we are not at the end; and
        // 2) we are not in the right namespace
        while (it_begin != it_end && it_begin->ns != ns) {
            it_begin++;
        }
        

        // Go backwards as long as
        // 1) we haven't reached the beginning; and
        // 2) we are not in the right namespace
        while (it_begin != it_end && (it_end - 1)->ns != ns)
            it_end--;
        
        return make_pair(it_begin, it_end);
    }
    
    return make_pair(_empty_attribute_vector.cend(), _empty_attribute_vector.cend());
    */
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
    hash_combine(hashed_val, part);
    // hash_combine(<#(std::size_t&)seed#>, <#(T const &)v#>)
    // Assert, somehow, that the object is trivially copyable
    // cout << "Copying " << sizeof(part) << " at offset " << start_index << "\n";
    // memcpy(values.data() + start_index, &part, sizeof(part));

}

template <typename T>
void FeatureKey::add_part(T part) {

}

void FeatureKey::add_attribute(Attribute attribute, float val) {
    hash_combine(hashed_val, attribute.index);
    value *= val;
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

WeightSectionWrap WeightMap::get_or_insert_section(size_t key) {
    float * val_ptr = table_block.lookup(key);
    if (val_ptr == nullptr)
        return WeightSectionWrap(table_block.insert(key), section_size);
    else
        return WeightSectionWrap(val_ptr, section_size);
}


/*
WeightSection & WeightMap::get(FeatureKey key) {
    //std::hash<FeatureKey> hash_fn;
    //size_t id_hash = hash_fn(key);


    HashCell * cell_ptr = table.Lookup(key.hashed_val);
    if (cell_ptr == nullptr) {
        cell_ptr = table.Insert(key.hashed_val);
        cell_ptr->value = new WeightSection(section_size);
    }

    // Safe to dereference this pointer?
    assert(cell_ptr->value != nullptr);
    return *cell_ptr->value;

//    auto & section = weights[key];
//    // Check if section is new
//    if (section.weights.size() != section_size) {
//        section.weights.resize(section_size);
//        section.cumulative.resize(section_size);
//    }
//
//    return section;
}
*/

WeightMap::WeightMap(size_t section_size_)
        : table_block(262144, section_size_ * WeightSectionWrap::num_blocks), section_size(section_size_)  {
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

