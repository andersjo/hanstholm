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
    : feature_builder(feature_set), corpus_dictionary(dict), num_rounds(num_rounds)
    {
        labeled_move_list = Strategy::moves(corpus_dictionary.relation_to_id.size());
        num_labeled_moves = labeled_move_list.size();
        weights = WeightMap(num_labeled_moves);
        scores.resize(num_labeled_moves);

    }
    void fit(std::vector<CSentence> & sentences);
    ParseResult parse(CSentence &);
private:
    void score_moves(std::vector<FeatureKey> &features);
    LabeledMove predict_move();
    LabeledMove compute_gold_move(CSentence & sent, CParseState & state);
    
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
void TransitionParser<Strategy>::fit(std::vector<CSentence> & sentences)
{
    std::vector<FeatureKey> features;
    std::vector<bool> decisions;

    for (int round_i = 0; round_i < num_rounds; round_i++) {
        for (auto sent : sentences) {
            auto state = CParseState(sent.tokens.size());
            
            while (!state.is_terminal()) {
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
                    decisions.push_back(false);
                    for (auto &feature : features) {
                        auto &w_section = weights.get(feature);
                        w_section.weights[pred_move.index] -= 1;
                        w_section.weights[gold_move.index] += 1;
                    }
                } else {
                    decisions.push_back(true);
                }
                
                // Perform the gold move
                perform_move(gold_move, state, sent.tokens);
                
                if (decisions.size() % 100 == 0) {
                    if (decisions.size() >= 100) {
                        auto sum = std::accumulate(decisions.cend() - 100, decisions.cend(), 0);
                        cout << sum << " correct\n";
                        
                    }
                }
                
                features.clear();
                
            }
        }
    }
};

template <typename Strategy>
void TransitionParser<Strategy>::score_moves(std::vector<FeatureKey> &features) {
    std::fill(scores.begin(), scores.end(), 0);
    
    for (FeatureKey & feature : features) {
        auto &w_section = weights.get(feature);
        for (int move_id = 0; move_id < num_labeled_moves; move_id++) {
            scores[move_id] += w_section.weights[move_id];
        }
    }
}

template <typename Strategy>
LabeledMove TransitionParser<Strategy>::predict_move() {
    return labeled_move_list[argmax(scores)];
}

template <typename Strategy>
LabeledMove TransitionParser<Strategy>::compute_gold_move(CSentence & sent, CParseState & state) {
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
    
    return *gold_move_ptr;

}





#endif
