#include "learn.h"

void TransitionParser::fit(std::vector<Sentence> &sentences) {
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
                auto allowed_moves = strategy.allowed_labeled_moves(state);
                auto oracle_moves = strategy.oracle(state, sent);
                LabeledMove & pred_move = argmax_move(allowed_moves);
                LabeledMove & gold_move = argmax_move(oracle_moves);

                // If predicted move and gold move are not identical,
                // update the model with the difference between the
                // feature representations of the two moves.
                // EARLY UPDATE
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

    }

    finish_learn();
}

void TransitionParser::do_update(vector<FeatureKey> &features, LabeledMove &pred_move,
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

void TransitionParser::finish_learn() {
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

ParseResult TransitionParser::parse(const Sentence &sent) {
    std::vector<FeatureKey> features;
    auto state = ParseState(sent.tokens.size());

    while (!state.is_terminal()) {
        feature_builder->fill_features(state, sent, features, 0);
        score_moves(features);
        auto allowed_moves = strategy.allowed_labeled_moves(state);

        LabeledMove & pred_move = argmax_move(allowed_moves);
        perform_move(pred_move, state, sent.tokens);

        features.clear();
    }

    return ParseResult(state.heads, state.labels);
};


void TransitionParser::score_moves(std::vector<FeatureKey> &features) {
    std::fill(scores.begin(), scores.end(), 0);

    for (FeatureKey &feature : features) {
        auto section = weights.get_or_insert_section(feature);
        auto *w = section.weights();

        for (int move_id = 0; move_id < num_labeled_moves; move_id++) {
            scores[move_id] += w[move_id];
        }
    }
}

LabeledMove TransitionParser::predict_move() {
    return labeled_move_list[argmax(scores)];
}

LabeledMove TransitionParser::compute_gold_move(Sentence &sent, ParseState &state) {
    // Get zero cost moves.
    LabeledMoveSet zero_cost_labeled_moves = strategy.oracle(state, sent);

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


LabeledMove & TransitionParser::argmax_move(LabeledMoveSet &allowed) {
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
