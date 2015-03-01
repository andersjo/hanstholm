//
//  learn.h
//  rungsted-parser
//

#ifndef rungsted_parser_learn_h
#define rungsted_parser_learn_h

#include "parse.h"
#include "features.h"
#include <vector>
#include <numeric>

using namespace std;
template <typename T>
size_t argmax(T container) {
    typename T::value_type best_val {};
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


template <typename Strategy>
class TransitionParser {
public:
    TransitionParser() = default;
    TransitionParser(CorpusDictionary& dict, std::vector<combined_feature_t> feature_set, size_t num_rounds = 5)
    : corpus_dictionary(dict), num_rounds(num_rounds), feature_builder(feature_set)
    {
        labeled_move_list = Strategy::moves(corpus_dictionary.label_to_id.size());
        num_labeled_moves = labeled_move_list.size();
        weights = WeightMap(num_labeled_moves);
        scores.resize(num_labeled_moves);

    }
    void fit(std::vector<Sentence> & sentences);
    ParseResult parse(const Sentence &);
private:
    void score_moves(std::vector<FeatureKey> &features);
    LabeledMove predict_move();
    LabeledMove compute_gold_move(Sentence & sent, ParseState & state);
    
    // Reference or copy?
    CorpusDictionary & corpus_dictionary;
    size_t num_rounds = 5;
    WeightMap weights;
    FeatureBuilder2 feature_builder;
    std::vector<LabeledMove> labeled_move_list;
    size_t num_labeled_moves = 0;
    std::vector<weight_t> scores;
};


template <typename Strategy>
void TransitionParser<Strategy>::fit(std::vector<Sentence> & sentences)
{
    std::vector<FeatureKey> features;

    for (int round_i = 0; round_i < num_rounds; round_i++) {
        int num_updates = 0;
        int num_tokens_seen = 0;
        cout << "Round " << round_i << " begun\n";
        cout << "Here, " << (static_cast<double>(weights.table_block.num_keys_searched) / static_cast<double>(weights.table_block.num_lookups))  << " keys searched on avg.\n";
        weights.table_block.num_keys_searched = 1;
        weights.table_block.num_lookups = 1;


        for (auto sent : sentences) {
            auto state = ParseState(sent.tokens.size());

            while (!state.is_terminal()) {
                num_tokens_seen++;
                // Get the best next move according to current parameters.

                // Compute features for the current state,
                // and score moves according to current model.
                feature_builder.build(state, sent, features);
                score_moves(features);

                // Predicted move is the argmax of scores
                LabeledMove pred_move = predict_move();
                // Gold move is the argmax of the zero-cost
                LabeledMove gold_move = compute_gold_move(sent, state);

                // If predicted move and gold move are not identical,
                // update the model with the difference between the
                // feature representations of the two moves.
                if (pred_move != gold_move) {
                    num_updates++;
                    for (auto &feature : features) {
                        float * w_section = weights.get_or_insert(feature);
                        weights.weights_begin(w_section)[pred_move.index] -= 1;
                        weights.weights_begin(w_section)[gold_move.index] += 1;
                    }
                }
                // Perform the gold move
//                auto allowed_moves = Strategy::allowed_labeled_moves(state);
//                if (allowed_moves.test(Move::RIGHT_ARC)) cout << "RIGHT_ARC ";
//                if (allowed_moves.test(Move::LEFT_ARC)) cout << "LEFT_ARC ";
//                if (allowed_moves.test(Move::SHIFT)) cout << "SHIFT ";
//                if (allowed_moves.test(Move::REDUCE)) cout << "REDUCE ";
//                cout << " allowed\n";



                perform_move(gold_move, state, sent.tokens);

                features.clear();

            }
        }

        double correct_pct = 1.0 - (static_cast<double>(num_updates) / static_cast<double>(num_tokens_seen));
        correct_pct *= 100;
        cout << correct_pct << " % correct decisions in round\n";
    }
}


template <typename Strategy>
ParseResult TransitionParser<Strategy>::parse(const Sentence & sent)
{

    std::vector<FeatureKey> features;
    auto state = ParseState(sent.tokens.size());

    while (!state.is_terminal()) {
        feature_builder.build(state, sent, features);
        score_moves(features);
        auto allowed_moves = Strategy::allowed_labeled_moves(state);

        bool allowed_move_found = false;
        for (auto lmove: labeled_move_list) {
            // Convert labeled moves to unlabeled before testing
            if (!allowed_moves.test(lmove.move)) {
                scores[lmove.index] = -std::numeric_limits<weight_t >::infinity();
            } else {
                allowed_move_found = true;
            }
        }
        assert(allowed_move_found);


        LabeledMove pred_move = predict_move();
        cout << "Predicted move " << static_cast<int>(pred_move.move) << "\n";
        cout << "State N0 is" << state.n0 << "\n";
        cout << "Stack size is " << state.stack.size() << "\n";
        perform_move(pred_move, state, sent.tokens);

        features.clear();
    }

    return ParseResult(state.heads, state.labels);
};


template <typename Strategy>
void TransitionParser<Strategy>::score_moves(std::vector<FeatureKey> &features) {
    std::fill(scores.begin(), scores.end(), 0);

    for (FeatureKey & feature : features) {
        float * w_section = weights.get_or_insert(feature);
        for (int move_id = 0; move_id < num_labeled_moves; move_id++) {
            scores[move_id] += weights.weights_begin(w_section)[move_id];
        }
    }
}

template <typename Strategy>
LabeledMove TransitionParser<Strategy>::predict_move() {
    return labeled_move_list[argmax(scores)];
}

template <typename Strategy>
LabeledMove TransitionParser<Strategy>::compute_gold_move(Sentence & sent, ParseState & state) {
    // Get zero cost moves.
    LabeledMoveSet zero_cost_labeled_moves = Strategy::oracle(state, sent);

    // FIXME find lowest number possible
    weight_t best_score = -1E7;
    LabeledMove *gold_move_ptr = nullptr;
    for (int i = 0; i < num_labeled_moves; i++) {
        auto & current = labeled_move_list[i];
        if (zero_cost_labeled_moves.test(current) && scores[i] >= best_score) {
            best_score = scores[i];
            gold_move_ptr = &current;
        }
    }

    assert(gold_move_ptr != nullptr);
    return *gold_move_ptr;
}

#endif
