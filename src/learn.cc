#include <random>
#include "learn.h"
#include "feature_handling.h"

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
                auto allowed_moves = strategy.allowed_labeled_moves(state, sent);
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

                // Error exploration?

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
        auto allowed_moves = strategy.allowed_labeled_moves(state, sent);

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


std::ostream& operator <<(std::ostream& os, const Move& move) {
    switch (move) {
        case Move::LEFT_ARC:
            os << "LEFT_ARC";
            break;
        case Move::REDUCE:
            os << "REDUCE";
            break;
        case Move::RIGHT_ARC:
            os << "RIGHT_ARC";
            break;
        case Move::SHIFT:
            os << "SHIFT";
            break;
        case Move::COUNT:
            os << "COUNT";
    }
    return os;
};


void update_span_states(const LabeledMove &lmove, ParseState &state, const Sentence &sent) {
    for (size_t i = 0; i < sent.span_constraints.size(); i++) {
        auto &span_state = state.span_states.at(i);
        auto &sc = sent.span_constraints.at(i);
        auto &stack = state.stack;

//        if (lmove.move == Move::LEFT_ARC && sc.is_inside(state.n0) && sc.is_inside(stack.back())) {
//            assert(span_state.num_connected_components >= 1);
//        }

        switch (lmove.move) {
            case Move::SHIFT:
                // A node of the span is shifted onto the stack
                if (sc.is_inside(state.n0))
                    span_state.num_connected_components += 1;
                break;
            case Move::RIGHT_ARC:
                // The first node of the span is inserted into the stack
                if (sc.span_start == state.n0)
                    span_state.num_connected_components = 1;
                break;
            case Move::LEFT_ARC:
                // Two nodes of the span become connected
                if (sc.is_inside(state.n0) && sc.is_inside(stack.back()))
                    span_state.num_connected_components -= 1;
                break;
            case Move::REDUCE:
                break;
            default:
                throw std::runtime_error("Invalid move");
        }


        if (lmove.move == Move::RIGHT_ARC || lmove.move == Move::LEFT_ARC) {
            bool s0_inside = sc.is_inside(stack.back());
            bool n0_inside = sc.is_inside(state.n0);
            if ((s0_inside && !n0_inside) || (!s0_inside && n0_inside )) {
                span_state.designated_root = s0_inside ? stack.back() : state.n0;
            }
        }
    }
}

ParseResult random_parse(const Sentence &sent, TransitionSystem &strategy, bool verbose) {
    std::random_device rd;
    std::mt19937 g(rd());

    auto state = ParseState(sent.tokens.size(), sent.span_constraints.size());

    auto possible_moves = strategy.moves(1);
    while (!state.is_terminal()) {
        // Pick a random move allowed in the current state.
        // Do this by randomly shuffling the `possible_moves` vector and take the first allowed element.
        std::shuffle(possible_moves.begin(), possible_moves.end(), g);
        auto allowed_moves = strategy.allowed_labeled_moves(state, sent);

        bool found_possible_move = false;
        for (const auto &possible_move : possible_moves) {
            if (allowed_moves.test(possible_move)) {
                if (verbose) {
                    cerr << "Take " << possible_move.move << " in state  ";
                    state.print_state();
                }

                update_span_states(possible_move, state, sent);
                perform_move(possible_move, state, sent.tokens);
                found_possible_move = true;
                break;
            }
        }

        if (!found_possible_move) {
            cerr << "Stuck in state ";
            state.print_state();
            cerr << "No possible move";
        }

        assert(found_possible_move);
    }

    return ParseResult(state.heads, state.labels);
}