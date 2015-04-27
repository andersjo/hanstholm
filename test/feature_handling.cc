//
// Created by Anders Johannsen on 25/04/15.
//

#include "catch.h"

#include <features.h>
#include <feature_set_parser.h>
#include <input.h>

TEST_CASE( "features are combined" ) {
    auto dict = CorpusDictionary();

    // Read in a test sentence
    // FIXME reference this in a relative way
    auto reader = VwSentenceReader("/users/anders/code/hanstholm/test/call_me.hanstholm", dict);
    auto sentence = reader.read().at(0);

    // Initialize parse state
    auto state = ParseState(sentence.tokens.size());
    std::vector<FeatureKey> features;

    SECTION( "missing initial entry nothing is added" ) {
        // Missing an initial entry in the features vector, nothing is added
        auto cartesian_product = parse_feature_line("S0:w ++ S0:p", dict);
        REQUIRE(cartesian_product->good(state));
        cartesian_product->fill_features(state, sentence, features, 0);
        REQUIRE(features.size() == 0);
    }

    SECTION( " cartesian product of 2 namespaces" ) {
        auto cartesian_product = parse_feature_line("S0:w ++ S0:p", dict);
        features.push_back(FeatureKey(0));
        cartesian_product->fill_features(state, sentence, features, 0);
        REQUIRE(features.size() == 2);

        SECTION(" feature values are combined" ) {
            REQUIRE( features[0].value ==  Approx( 0.6 ) );
            REQUIRE( features[1].value ==  Approx( 0.4 ) );
        }
    }

    SECTION( " cartesian product of 3 namespaces" ) {
        auto cartesian_product = parse_feature_line("S0:p ++ S0:p ++ S0:w", dict);
        features.push_back(FeatureKey(0));
        cartesian_product->fill_features(state, sentence, features, 0);
        REQUIRE(features.size() == 4);
    }
}
