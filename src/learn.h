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


template<typename Strategy>
class TransitionParser {
public:
    TransitionParser() = default;

    TransitionParser(CorpusDictionary &dict, std::unique_ptr<UnionList> &feature_builder_, size_t num_rounds = 5)
            : corpus_dictionary(dict), num_rounds(num_rounds), feature_builder(std::move(feature_builder_)) {
        labeled_move_list = Strategy::moves(corpus_dictionary.label_to_id.size());
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
    // FeatureBuilder2 feature_builder_old;
    std::unique_ptr<UnionList> feature_builder;

    std::vector<LabeledMove> labeled_move_list;
    size_t num_labeled_moves = 0;
    std::vector<weight_t> scores;

    void do_update(vector<FeatureKey> &features, LabeledMove &pred_move, LabeledMove &gold_move);

    void finish_learn();

    LabeledMove & argmax_move(LabeledMoveSet &allowed);

};


template<typename Strategy>
void TransitionParser<Strategy>::fit(std::vector<Sentence> &sentences) {
    std::vector<FeatureKey> features;

    for (int round_i = 0; round_i < num_rounds; round_i++) {
        int num_updates = 0;
        int num_tokens_seen = 0;
        cout << "Pass " << round_i + 1 << " begun\n";


        for (auto sent : sentences) {
            auto state = ParseState(sent.tokens.size());

            while (!state.is_terminal()) {
                num_tokens_seen++;

                // Compute features for the current state,
                // and score moves according to current model.
                feature_builder->fill_features(state, sent, features, 0);
                score_moves(features);

                // Get the best next move according to current parameters.
                auto allowed_moves = Strategy::allowed_labeled_moves(state);
                auto oracle_moves = Strategy::oracle(state, sent);
                LabeledMove & pred_move = argmax_move(allowed_moves);
                LabeledMove & gold_move = argmax_move(oracle_moves);

                // If predicted move and gold move are not identical,
                // update the model with the difference between the
                // feature representations of the two moves.
                if (pred_move != gold_move) {
                    num_updates++;
                    do_update(features, pred_move, gold_move);
                }

                perform_move(gold_move, state, sent.tokens);
                features.clear();

            }
        }

        double correct_pct = 1.0 - (static_cast<double>(num_updates) / static_cast<double>(num_tokens_seen));
        correct_pct *= 100;
        cout << correct_pct << " % correct decisions in round\n";

        /*
        for (const auto & operand: feature_builder->operands) {
            cerr << "Rule: " << operand->name << " applied " << operand->n_applications << " times, extracting "
                << operand->n_features << " features\n";
            operand->n_applications = 0;
            operand->n_features = 0;
        }
        */

    }

    finish_learn();
}

template<typename Strategy>
void TransitionParser<Strategy>::do_update(vector<FeatureKey> &features, LabeledMove &pred_move,
                                           LabeledMove &gold_move) {
    weights.num_updates++;
    for (const auto &feature : features) {
        // Idea: separate update step to a function
        // FIXME: Check that features are unique and do not occur multiple times in the feature vector
        auto section = weights.get_or_insert_section(feature);

        auto *w = section.weights();
        auto *acc_weights = section.acc_weights();
        auto *update_timestamp = section.update_timestamps();

        // Perform missed updates on the accumulated weights due to sparse updating
        float num_missed_updates_pred = weights.num_updates - update_timestamp[pred_move.index] - 1;
        float num_missed_updates_gold = weights.num_updates - update_timestamp[gold_move.index] - 1;

        acc_weights[pred_move.index] += num_missed_updates_pred * w[pred_move.index];
        acc_weights[gold_move.index] += num_missed_updates_gold * w[gold_move.index];

        update_timestamp[pred_move.index] = weights.num_updates;
        update_timestamp[gold_move.index] = weights.num_updates;

        // Gold
        w[gold_move.index] += feature.value;
        acc_weights[gold_move.index] += feature.value;

        // Pred
        w[pred_move.index] -= feature.value;
        acc_weights[pred_move.index] -= feature.value;
    }
}

template<typename Strategy>
void TransitionParser<Strategy>::finish_learn() {
    // Average weights in a hacky way that exposes details of the hash table better left unexposed.
    for (size_t key : weights.table_block.keys) {
        if (key != 0) {
            auto section = weights.get_section(key);
            auto *w = section.weights();

            auto *acc_weights = section.acc_weights();
            auto *update_timestamps = section.update_timestamps();

            for (int i = 0; i < weights.section_size; i++) {
                if (update_timestamps[i] == 0)
                    continue;

                // Add missed updates to acc_weights
                float num_missed_updates = (weights.num_updates - update_timestamps[i]);
                acc_weights[i] += w[i] * num_missed_updates;
                w[i] = acc_weights[i] / weights.num_updates;
            }
        }
    }
}

template<typename Strategy>
ParseResult TransitionParser<Strategy>::parse(const Sentence &sent) {
    std::vector<FeatureKey> features;
    auto state = ParseState(sent.tokens.size());

    while (!state.is_terminal()) {
        feature_builder->fill_features(state, sent, features, 0);
        score_moves(features);
        auto allowed_moves = Strategy::allowed_labeled_moves(state);

        LabeledMove & pred_move = argmax_move(allowed_moves);
        perform_move(pred_move, state, sent.tokens);

        features.clear();
    }

    return ParseResult(state.heads, state.labels);
};


template<typename Strategy>
void TransitionParser<Strategy>::score_moves(std::vector<FeatureKey> &features) {
    std::fill(scores.begin(), scores.end(), 0);

    for (FeatureKey &feature : features) {
        auto section = weights.get_or_insert_section(feature);
        auto *w = section.weights();

        for (int move_id = 0; move_id < num_labeled_moves; move_id++) {
            scores[move_id] += w[move_id];
        }
    }
}

template<typename Strategy>
LabeledMove TransitionParser<Strategy>::predict_move() {
    return labeled_move_list[argmax(scores)];
}

template<typename Strategy>
LabeledMove TransitionParser<Strategy>::compute_gold_move(Sentence &sent, ParseState &state) {
    // Get zero cost moves.
    LabeledMoveSet zero_cost_labeled_moves = Strategy::oracle(state, sent);

    // FIXME find lowest number possible
    weight_t best_score = -std::numeric_limits<weight_t>::infinity();
    LabeledMove *gold_move_ptr = nullptr;
    for (int i = 0; i < num_labeled_moves; i++) {
        auto &current = labeled_move_list[i];
        if (zero_cost_labeled_moves.test(current) && scores[i] >= best_score) {
            best_score = scores[i];
            gold_move_ptr = &current;
        }
    }

    assert(gold_move_ptr != nullptr);
    return *gold_move_ptr;
}


template<typename Strategy>
LabeledMove & TransitionParser<Strategy>::argmax_move(LabeledMoveSet &allowed) {
    weight_t best_val = -std::numeric_limits<weight_t>::infinity();
    int best_index = -1;

    for (const auto &move : labeled_move_list) {
        if (allowed.test(move) && scores[move.index] >= best_val) {
            best_index = static_cast<int>(move.index);
            best_val = scores[move.index];
        }
    }

    assert(best_index > -1);

    return labeled_move_list[best_index];
}


#endif

