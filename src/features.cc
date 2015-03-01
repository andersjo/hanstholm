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
    : name(name), location(location), ns(ns)
{
    
}

std::pair<attribute_list_citerator, attribute_list_citerator>
AttributeExtractor::extract2(const ParseState & state, const Sentence & sent) const
{
    
    int token_index = state.locations_[location];
    if (token_index >= static_cast<int>(sent.tokens.size())) {
        // cout << "Token index beyond sentence size. Location: " << location << " pointing to token index: " << token_index << endl;
    }

    // assert(token_index <= static_cast<int>(sent.tokens.size()));

    // Token index should be inside the sentence.
    // It may outside the sentence when the location is not defined (value = -1)
    // or when N0 is pushed beyond the last token.
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
};




template <typename T>
void FeatureKey::add_part(T part, size_t start_index) {
    hash_combine(hashed_val, part);
    // hash_combine(<#(std::size_t&)seed#>, <#(T const &)v#>)
    // Assert, somehow, that the object is trivially copyable
    // cout << "Copying " << sizeof(part) << " at offset " << start_index << "\n";
    // memcpy(values.data() + start_index, &part, sizeof(part));

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
        : table_block(262144, section_size_*2), section_size(section_size_)  {

}

