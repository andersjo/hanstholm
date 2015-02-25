#include "features.h"
#include <iostream>
#include <cassert>
#include <stddef.h>

using namespace std;







FeatureBuilder2::FeatureBuilder2(std::vector<combined_feature_t> feature_set) : feature_set(feature_set) { }

void FeatureBuilder2::build(CParseState & state, CSentence & sent, std::vector<FeatureKey> & features) {
    size_t combined_feature_num = 0;
    for (auto & combined_feature : feature_set) {
        features.push_back(FeatureKey(combined_feature_num));
        auto start_index = features.size() - 1;
        size_t value_offset = 0;

        for (auto & attribute_extractor : combined_feature) {

            auto it_pair = attribute_extractor.extract2(state, sent);
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






vector<combined_feature_t> nivre_feature_set() {
    vector<combined_feature_t> feature_combinations;
    using namespace state_location;
    using E = AttributeExtractor;
    
    auto S0w = E("S0:w", 'w', S0);
    auto S0p = E("S0:p", 'p', S0);
    auto N0w = E("N0:w", 'w', N0);
    auto N0p = E("N0:p", 'p', N0);
    auto N1w = E("N1:w", 'w', N1);
    auto N1p = E("N1:p", 'p', N1);
    auto N2w = E("N2:w", 'w', N2);
    auto N2p = E("N2:p", 'p', N2);
    
    auto S0_head_p = E("S0_head:p", 'p', S0_head);
    auto S0_right_p = E("S0_right:p", 'p', S0_right);
    auto S0_left_p = E("S0_left:p", 'p', S0_left);
    auto N0_left_p = E("N0_left:p", 'p', N0_left);
    
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
    
    return feature_combinations;
}



AttributeExtractor::AttributeExtractor(string name, namespace_t ns, state_location::LocationName location)
    : name(name), ns(ns), location(location)
{
    
}

const vector<Attribute> & AttributeExtractor::extract(const CParseState & state, const CSentence & sent) const {
    auto loc = state.locations();
    auto token_index = loc.at(location);
    
    if (token_index != -1) {
        auto & token = sent.tokens[token_index];
        if (token.namespaces.size() >= (ns+1)) {
            // FIXME why does this happen? ROOT token perhaps?
            // cout << "skipped token\n";
            return token.namespaces.at(ns);
        }
    }
    
    return _empty_attribute_vector;
}


std::pair<attribute_list_citerator, attribute_list_citerator>
AttributeExtractor::extract2(const CParseState & state, const CSentence & sent) const
{
    
    auto loc = state.locations();
    auto token_index = loc.at(location);
    
    if (token_index != -1) {
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
};




template<typename... args>
void FeatureBuilder::add(FeatureName name, const args & ...  rest) {
    if (!check_none_empty(rest...))
        return;

    // First call in the recursive add functions.
    features.push_back(FeatureKey{name});
    start_index = features.size() - 1;
    value_count = 0;
    // cout << "Call add feature. Currently " << features.size() << " added\n";
    add_(name, rest...);
}

template<typename T, typename... args>
bool FeatureBuilder::check_none_empty(const vector<T> & first, const args & ...rest) {
    // cout << "Vector size " << first.size() << "\n";
    if (first.size() == 0) return false;
    return check_none_empty(rest...);
}

template<typename T, typename... args>
bool FeatureBuilder::check_none_empty(const T & first, const args & ...rest) {
    if (first == -1) return false;
    return check_none_empty(rest...);
}

bool FeatureBuilder::check_none_empty() {
    return true;
}

template<typename T, typename... args>
void FeatureBuilder::add_(FeatureName name, const T & first, const args & ... rest) {
    for (size_t i = start_index; i < features.size(); i++)
        features[i].add_part(first, value_count);

    value_count += sizeof(first);
    add_(name, rest...);
}


template<typename... args>
void FeatureBuilder::add_(FeatureName name, const std::vector<PosTag> &first, const args & ... rest) {
    // auto x = FeatureKey::values.size();
    assert(sizeof(first[0].tag) + value_count <= features[start_index].values.size());

    for (size_t i = start_index; i < features.size(); i++) {
        for (int j = 1; j < first.size(); j++) {
            features.push_back(features[i]);
            features.back().add_part(first[j].tag, value_count);
            features.back().value *= first[j].weight;
        }

        features[i].add_part(first[0].tag, value_count);
        features[i].value *= first[0].tag;
    }

    value_count += sizeof(first[0].tag);

    add_(name, rest...);

}

void FeatureBuilder::add_(FeatureName name) {

}

template <typename T>
void FeatureKey::add_part(T part, size_t start_index) {
    // Assert, somehow, that the object is trivially copyable
    // cout << "Copying " << sizeof(part) << " at offset " << start_index << "\n";
    memcpy(values.data() + start_index, &part, sizeof(part));

}

/**
* Return the weights for the given feature.
* Inserts an new weight section if it doesn't exist already.
* This behaviour should be okay during test, because all weights are initialized to zero.
*
*/
WeightSection & WeightMap::get(FeatureKey key) {
    auto & section = weights[key];
    // Check if section is new
    if (section.weights.size() != section_size) {
        section.weights.resize(section_size);
        section.cumulative.resize(section_size);
    }

    return section;
}

WeightMap::WeightMap(size_t section_size) : section_size(section_size) { }
