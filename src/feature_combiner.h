//
// Created by Anders Johannsen on 07/04/15.
//

#ifndef _HANSTHOLM_FEATURE_COMBINER_H_
#define _HANSTHOLM_FEATURE_COMBINER_H_

#include <list>
#include <boost/algorithm/string/join.hpp>
#include "features.h"
#include <memory>

using attribute_list_citerator = std::vector<Attribute>::const_iterator;

struct FeatureCombinerBase {
    FeatureCombinerBase(std::string name) : name(name) {};
    std::string name;
    virtual void fill_features(const ParseState &state, const Sentence &sent,
                               std::vector<FeatureKey> & features, size_t start_index) {};
    virtual std::pair<attribute_list_citerator, attribute_list_citerator>
            find_attributes(const ParseState &state, const Sentence &sent) const {
        throw std::runtime_error("Not implemented");
    };
    virtual bool good(const ParseState &state) const {
        return true;
    }

    int n_applications = 0;
    int n_features = 0;
};

using feature_combiner_uptr = std::unique_ptr<FeatureCombinerBase>;

struct Location : FeatureCombinerBase {
    Location(std::string &name, state_location::LocationName location_, namespace_t ns_)
            : FeatureCombinerBase(name), location(location_), ns(ns_) { }

    state_location::LocationName location;
    namespace_t ns;
    token_index_t token_specific_ns = -1;
    virtual std::pair<attribute_list_citerator, attribute_list_citerator>
            find_attributes(const ParseState &state, const Sentence &sent) const override;

    virtual void fill_features(const ParseState &state, const Sentence &sent, std::vector<FeatureKey> &features,
                               size_t start_index) override;


    virtual bool good(const ParseState &state) const override;
};

struct UnionList : FeatureCombinerBase {
    UnionList(std::list<feature_combiner_uptr> &operands_)
            : FeatureCombinerBase(""), operands(std::move(operands_)) {
        std::vector<std::string> names;
        std::transform(operands.cbegin(), operands.cend(),
                       std::back_inserter(names),
                       [](const feature_combiner_uptr &fc) { return fc->name; }
        );
        name = boost::algorithm::join(names, " u\n");

    };
    // UnionList() : FeatureCombinerBase("Empty") {};
    std::list<feature_combiner_uptr > operands;


    virtual void fill_features(const ParseState &state, const Sentence &sent, std::vector<FeatureKey> &features,
                               size_t start_index) override;
};

struct BinaryCombiner : FeatureCombinerBase {
    BinaryCombiner(std::string &name, feature_combiner_uptr lhs_, feature_combiner_uptr rhs_)
            : FeatureCombinerBase(name), lhs(std::move(lhs_)), rhs(std::move(rhs_)) {
    }

    feature_combiner_uptr lhs;
    feature_combiner_uptr rhs;
};


struct CartesianProduct : BinaryCombiner {
    CartesianProduct(std::string &name, feature_combiner_uptr lhs_, feature_combiner_uptr rhs_)
            : BinaryCombiner(name, std::move(lhs_), std::move(rhs_)) {
    }

    virtual void fill_features(const ParseState &state, const Sentence &sent, std::vector<FeatureKey> &features,
                               size_t start_index) override;

    virtual bool good(const ParseState &state) const override;
};

struct Union : BinaryCombiner {
    Union(std::string &name, feature_combiner_uptr lhs_, feature_combiner_uptr rhs_)
            : BinaryCombiner(name, std::move(lhs_), std::move(rhs_)) {
    }
};



#endif //_HANSTHOLM_FEATURE_COMBINER_H_
