//
// Created by Anders Johannsen on 07/04/15.
//

#ifndef _HANSTHOLM_FEATURE_COMBINER_H_
#define _HANSTHOLM_FEATURE_COMBINER_H_

struct FeatureCombinerBase {
    FeatureCombinerBase(std::string name) : name(name) {};
    std::string name;
};

using feature_combiner_uptr = std::unique_ptr<FeatureCombinerBase>;

struct Location : FeatureCombinerBase {
    Location(std::string &name, state_location::LocationName location_, namespace_t ns_)
            : FeatureCombinerBase(name), location(location_), ns(ns_) { }

    // feature_combiner_uptr location;
    state_location::LocationName location;
    namespace_t ns;
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
};

struct Union : BinaryCombiner {
    Union(std::string &name, feature_combiner_uptr lhs_, feature_combiner_uptr rhs_)
            : BinaryCombiner(name, std::move(lhs_), std::move(rhs_)) {
    }
};



#endif //_HANSTHOLM_FEATURE_COMBINER_H_
