//
// Created by Anders Johannsen on 25/04/15.
//

#include "catch.h"

#include "features.h"
#include "feature_set_parser.h"
#include "input.h"
#include "learn.h"


void check_arc_contraints(const Sentence &sent, const ParseResult &parse_result) {
    for (const ArcConstraint &arc_constraint : sent.arc_constraints) {
        REQUIRE(parse_result.heads.at(arc_constraint.dep) == arc_constraint.head);
    }
}

void check_span_constraints(const Sentence &sent, const ParseResult &parse_result) {
    // Check span constraints
    for (const SpanConstraint &span_constraint : sent.span_constraints) {
        // Find the root of the span
        int span_root = -1;
        for (int i = span_constraint.span_start; i <= span_constraint.span_end; i++) {
            // Go through each position inside the span

            if (!span_constraint.is_inside(parse_result.heads[i])) {
                REQUIRE(span_root == -1);
                span_root = i;
            }

            // Outside dependencies before span
            for (int j = 0; j < span_constraint.span_start; j++) {
                if (parse_result.heads[j] == i) {
                    REQUIRE(span_root == i);
                }
            }

            // Outside dependencies after span
            for (int j = span_constraint.span_end + 1; j < parse_result.heads.size(); j++) {
                if (parse_result.heads[j] == i) {
                    REQUIRE(span_root == i);
                }
            }
        }
    }
}

TEST_CASE( "Constraints are honored" ) {
    int RUNS = 100;
    bool VERBOSE = false;
    auto dict = CorpusDictionary();
    auto transition_system = ConstrainedArcEager();

    // Read in a test first_sent
    // FIXME reference this in a relative way
    auto reader = VwSentenceReader("/users/anders/code/hanstholm/test/constraints.hanstholm", dict);
    auto sentences = reader.read();
    REQUIRE(sentences.size() >= 3);
    auto arc_sent = sentences.at(0);
    auto span_sent = sentences.at(1);
    auto both_sent = sentences.at(2);

    SECTION( " randomly generated parses obey arc constraints " ) {
        for (int i = 0; i < RUNS; i++) {
            ParseResult parse_result = random_parse(arc_sent, transition_system, VERBOSE);
            check_arc_contraints(arc_sent, parse_result);
        }
    }

    SECTION( " randomly generated parses obey span constraints " ) {
        for (int i = 0; i < RUNS; i++) {
            ParseResult parse_result = random_parse(span_sent, transition_system, VERBOSE);
            check_span_constraints(span_sent, parse_result);
        }
    }

    SECTION( " randomly generated parses simultaneously obey arc and span constraints" ) {
        for (int i = 0; i < RUNS; i++) {
            ParseResult parse_result = random_parse(both_sent, transition_system, VERBOSE);
            check_span_constraints(both_sent, parse_result);
            check_arc_contraints(both_sent, parse_result);
        }

    }


}

