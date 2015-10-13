#ifndef FEATURES_H
#define FEATURES_H

#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <unordered_map>

#include "feature_handling.h"
#include "hash.h"
#include "hashtable.h"

struct FeatureKey {
    size_t hashed_val = 0;

    template <typename T>
    void add_part(T, size_t);

    template <typename T>
    void add_part(T);

    void add_attribute(Attribute);

    // Conceptually the value is not part of the key,
    // but it is kept here for convenience.
    float value = 1.0;
    // FeatureKey() {};
    FeatureKey(size_t feature_num = 0) : hashed_val(feature_num), value(1.0) { };
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
std::vector<combined_feature_t> nivre_feature_set(CorpusDictionary &);


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


struct WeightSection {
    std::vector<weight_t> weights;
    std::vector<weight_t> cumulative;
    WeightSection(size_t section_size) {
        weights.resize(section_size);
        cumulative.resize(section_size);
    }
};


struct HashCell {
    size_t key;
    // Perhaps change to a pointer to alloced memory to avoid another layer of indirection
    WeightSection * value;
};



// A weight section consists of a number of named blocks.
// Idea: generalize this concept by using enums for names and 2D Eigen for data storage.
struct WeightSectionWrap {
    WeightSectionWrap(float * const base, const size_t num_elems) : base(base), num_elems(num_elems) {};
    inline float * const weights() { return base; };
    inline float * const acc_weights() { return base + num_elems; };
    inline float * const update_timestamps() { return base + num_elems * 2; };
    float * const base;
    size_t num_elems;
    const static size_t num_blocks = 3;
};

class WeightMap {

public:
    WeightMap() {};
    WeightMap(size_t);
    WeightSection & get(FeatureKey);
    float *get_or_insert(FeatureKey);
    std::vector<size_t> all_keys();
    WeightSectionWrap get_or_insert_section(FeatureKey);
    WeightSectionWrap get_section(size_t);
    HashTableBlock table_block;
    size_t num_updates = 0;

    // Temp made public
    size_t section_size;

private:
    // size_t aligned_section_size;
};


#endif