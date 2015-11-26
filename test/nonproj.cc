//
// Created by Anders Johannsen on 25/04/15.
//

#include "catch.h"

#include "nonproj.h"


TEST_CASE( "Non-projective trees are converted correctly" ) {

    std::vector<token_index_t> proj = {4, 0, 1, 2, -1};
    std::vector<token_index_t> nonproj_small = {4, 3, 0, 0, -1};
    std::vector<token_index_t> nonproj_long = {6, 0, 4, 0, 0, 1, -1};
    std::vector<token_index_t> nonproj_chain = {-1, 2, 3, 0, 1};


    SECTION( "non-projectivity is detected" ) {

        Projectivizer projectivizer = Projectivizer();

        REQUIRE_FALSE(projectivizer.is_nonprojective(proj));
        REQUIRE(projectivizer.is_nonprojective(nonproj_small));
        REQUIRE(projectivizer.is_nonprojective(nonproj_long));
        REQUIRE(projectivizer.is_nonprojective(nonproj_chain));
    }

    SECTION( "lifting eliminates non-projectiviy") {
        Projectivizer projectivizer = Projectivizer();

        projectivizer.lift_longest(proj);
        for (auto & vec : {nonproj_small, nonproj_long, nonproj_chain}) {
            auto vec_copy = vec;
            REQUIRE(projectivizer.is_nonprojective(vec_copy));
            projectivizer.lift_longest(vec_copy);
            REQUIRE_FALSE(projectivizer.is_nonprojective(vec_copy));
        }
    }
}

