#ifndef FEATURES_H
#define FEATURES_H

#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <unordered_map>

#include "parse.h"
#include "hash.h"

enum FeatureName {
    // Single words
    S0w_and_S0p,
    S0w,
    S0p,
    N0w_and_N0p,
    N0w,
    N0p,
    N1w_and_N1p,
    N1w,
    N1p,
    N2w_and_N2p,
    N2w,
    N2p,
    
    // Word pairs
    S0w_and_S0p_and_N0w_and_N0p,
    S0w_and_S0p_and_N0w,
    S0w_and_N0w_and_N0p,
    S0w_and_S0p_and_N0p,
    S0p_and_N0w_and_N0p,
    S0w_and_N0w,
    S0p_and_N0p,
    N0p_and_N1p,

    // Three words
    N0p_and_N1p_and_N2p,
    S0p_and_N0p_and_N1p,
    S0_head_p_and_S0p_and_N0p,
    S0p_and_S0_left_p_and_N0p,
    S0p_and_S0_right_p_and_N0p,
    S0p_and_N0p_and_N0_left_p
    
};

using attribute_list_citerator = std::vector<Attribute>::const_iterator;
// Atomic attribute
struct AttributeExtractor {
    std::string name;
    state_location::LocationName location;
    namespace_t ns;
    const std::vector<Attribute> & extract(const CParseState & state, const CSentence & sent) const;
    std::pair<attribute_list_citerator, attribute_list_citerator> extract2(const CParseState & state, const CSentence & sent) const;
    std::vector<Attribute> _empty_attribute_vector {};


    
    AttributeExtractor(std::string, namespace_t ns, state_location::LocationName location);
    
};

using combined_feature_t = std::vector<AttributeExtractor>;

std::vector<combined_feature_t> nivre_feature_set();

struct FeatureKey {
    short feature_template_index;
    std::array<char, 16> values {};
    template <typename T>
    void add_part(T, size_t);

    // Conceptually the value is not part of the key,
    // but it is kept here for convenience.
    float value = 1.0;
    FeatureKey() {};
    
    FeatureKey(FeatureName feature_name) : feature_template_index(feature_name) { };
    FeatureKey(size_t feature_num)  {
        feature_template_index = static_cast<short>(feature_num);
    };

    bool operator==(const FeatureKey & other) const {
        return (values == other.values
                && feature_template_index == other.feature_template_index);
    }
};

namespace std {
    template <> struct hash<FeatureKey>
    {
        size_t operator()(const FeatureKey & f) const {
            std::size_t seed = 0;
            hash_range(seed, f.values.cbegin(), f.values.cend());
            hash_combine(seed, f.feature_template_index);
            return seed;
        }
    };

}

class FeatureBuilder2 {
public:
    std::vector<combined_feature_t> feature_set;
    void build(CParseState &, CSentence &, std::vector<FeatureKey> &);
    FeatureBuilder2(std::vector<combined_feature_t>);
};



class FeatureBuilder {
public:
    template<typename... args>
    void add(FeatureName, const args & ...);

    void extract(CParseState &, CSentence &);
    std::vector <FeatureKey> features;

private:
    template<typename T, typename... args>
    void add_(FeatureName name, const T & first, const args & ... rest);

    template<typename... args>
    void add_(FeatureName name, const std::vector<PosTag> & first, const args & ... rest);

    void add_(FeatureName name);

    size_t start_index;
    size_t value_count;

    // Check for empty vectors and invalid arguments
    template <typename T, typename... args>
    bool check_none_empty(const T & first, const args & ...);
    template <typename T, typename... args>
    bool check_none_empty(const std::vector<T> & first, const args & ...);
    bool check_none_empty();
};


struct WeightSection {
    std::vector<weight_t> weights;
    std::vector<weight_t> cumulative;
    int last_update = 0;
};

class WeightMap {

public:
    WeightMap() {};
    WeightMap(size_t);
    WeightSection & get(FeatureKey);
    std::unordered_map<FeatureKey, WeightSection> weights;
private:
    size_t section_size;
};


#endif