#ifndef FEATURES_H
#define FEATURES_H

#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <unordered_map>

#include "parse.h"
#include "hash.h"
#include "hashtable.h"


struct FeatureKey {
    size_t hashed_val = 0;
    // short feature_template_index;
    // std::array<char, 16> values {};

    template <typename T>
    void add_part(T, size_t);

    template <typename T>
    void add_part(T);

    void add_attribute(Attribute, float=1);


    // Conceptually the value is not part of the key,
    // but it is kept here for convenience.
    float value = 1.0;
    FeatureKey() {};
    FeatureKey(size_t feature_num)  {
        hashed_val = feature_num;
        // feature_template_index = static_cast<short>(feature_num);
    };

    /*
    bool operator==(const FeatureKey & other) const {
        return (values == other.values
                && feature_template_index == other.feature_template_index);
    }
    */
};

using attribute_list_citerator = std::vector<Attribute>::const_iterator;
// Atomic attribute

struct ExtractorBase {
    virtual void fill_features(const ParseState &state, const Sentence &sent, std::vector<FeatureKey> & features, size_t start_index) {};
};

class AttributeExtractor : public ExtractorBase {
public:
    std::string name;
    state_location::LocationName location;
    namespace_t ns;
    std::pair<attribute_list_citerator, attribute_list_citerator> extract(const ParseState &state, const Sentence &sent) const;
    std::vector<Attribute> _empty_attribute_vector {};

    void fill_features(const ParseState &state, const Sentence &sent, std::vector<FeatureKey> & features, size_t start_index) override;

    AttributeExtractor(std::string, namespace_t ns, state_location::LocationName location);
};


using combined_feature_t = std::vector<AttributeExtractor>;
std::vector<combined_feature_t> nivre_feature_set();


class ProductCombiner : public ExtractorBase {
public:
    ProductCombiner(ExtractorBase lhs_, ExtractorBase rhs_) : lhs(lhs_), rhs(rhs_) {};
    void fill_features(const ParseState &state, const Sentence &sent, std::vector<FeatureKey> & features, size_t start_index) override;
private:
    ExtractorBase lhs;
    ExtractorBase rhs;
};


class DotProductCombiner : public ExtractorBase {
public:
    DotProductCombiner(AttributeExtractor lhs, AttributeExtractor rhs);
    void fill_features(const ParseState &state, const Sentence &sent, std::vector<FeatureKey> & features, size_t start_index) override;
private:
    AttributeExtractor lhs;
    AttributeExtractor rhs;
};

// rhs.fill_features();


// auto what = ProductCombiner(2.0, 5.0);


/*
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
*/
class FeatureBuilder2 {
public:
    std::vector<combined_feature_t> feature_set;
    void build(ParseState &, const Sentence &, std::vector<FeatureKey> &);
    FeatureBuilder2(std::vector<combined_feature_t>);
};

struct WeightSection {
    std::vector<weight_t> weights;
    std::vector<weight_t> cumulative;
    WeightSection(size_t section_size) {
        weights.resize(section_size);
        cumulative.resize(section_size);
    }
};

/*
class WeightSection {
    WeightSection(size_t aligned_section_size, float * base_ptr) {

    }
    float * base_ptr;

};
*/



struct HashCell {
    size_t key;
    // Perhaps change to a pointer to alloced memory to avoid another layer of indirection
    WeightSection * value;
};


class WeightMap {

public:
    WeightMap() {};
    WeightMap(size_t);
    WeightSection & get(FeatureKey);
    float *get_or_insert(FeatureKey);
    // std::unordered_map<FeatureKey, WeightSection> weights;
    // HashTable<HashCell> table { 262144 };
    HashTableBlock table_block;
    float * weights_begin(float * base) {
        return base;
    };

private:
    size_t section_size;
    // size_t aligned_section_size;
};


#endif