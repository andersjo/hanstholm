//
//  learn.h
//  rungsted-parser
//

#ifndef rungsted_parser_learn_h
#define rungsted_parser_learn_h

#include "feature_handling.h"
#include "features.h"
#include "feature_combiner.h"
#include <vector>
#include <numeric>

using namespace std;

template<typename T>
size_t argmax(T container) {
    typename T::value_type best_val = -std::numeric_limits<typename T::value_type>::infinity();

    size_t best_index = 0;
    size_t index = 0;

    for (auto element : container) {
        if (element >= best_val) {
            best_index = index;
            best_val = element;
        }
        index++;
    }
    return best_index;
}


class TransitionParser {
public:
    TransitionParser() = default;

    TransitionParser(CorpusDictionary &dict, std::unique_ptr<UnionList> &feature_builder_, TransitionSystem &strategy_,
                     size_t num_rounds = 5)
            : corpus_dictionary(dict), num_rounds(num_rounds), feature_builder(std::move(feature_builder_)),
              strategy(strategy_) {

        labeled_move_list = strategy.moves(corpus_dictionary.label_to_id.size());
        num_labeled_moves = labeled_move_list.size();
        weights = WeightMap(num_labeled_moves);
        scores.resize(num_labeled_moves);
    }

    void fit(std::vector<Sentence> &sentences);

    ParseResult parse(const Sentence &);

private:
    void score_moves(std::vector<FeatureKey> &features);

    LabeledMove predict_move();

    LabeledMove compute_gold_move(Sentence &sent, ParseState &state);

    // Reference or copy?
    CorpusDictionary &corpus_dictionary;
    size_t num_rounds = 5;
    WeightMap weights;
    std::unique_ptr<UnionList> feature_builder;

    std::vector<LabeledMove> labeled_move_list;
    size_t num_labeled_moves = 0;
    std::vector<weight_t> scores;
    TransitionSystem &strategy;

    void do_update(vector<FeatureKey> &features, LabeledMove &pred_move, LabeledMove &gold_move);

    void finish_learn();

    LabeledMove &argmax_move(LabeledMoveSet &allowed);

};


ParseResult random_parse(const Sentence &sent, TransitionSystem &strategy, bool verbose=false);
void update_span_states(const LabeledMove &lmove, ParseState &state, const Sentence &sent);

#endif

