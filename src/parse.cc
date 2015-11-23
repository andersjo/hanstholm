#include "features.h"
#include "feature_handling.h"

#include <fstream>
#include <sstream> // stringstream
#include <algorithm>

using namespace std;


std::ostream& operator <<(std::ostream& os, const MoveSet& s) {
    os << "MoveSet";
    if (s.test(Move::LEFT_ARC))
        os << " LEFT_ARC";
    if (s.test(Move::RIGHT_ARC))
        os << " RIGHT_ARC";
    if (s.test(Move::SHIFT))
        os << " SHIFT";
    if (s.test(Move::REDUCE))
        os << " REDUCE";

    os << "\n";
    return os;
};


bool Sentence::has_edge(token_index_t i, token_index_t j) const {
	return tokens[i].head == j || tokens[j].head == i;
}


ParseState::ParseState(size_t length) : length(length) {
    assert(length >= 2);
    assert(length < 1000);
	stack.reserve(length);
	stack.push_back(0);
	n0 = 1;
    heads = vector<token_index_t>(length, -1 );
    labels = vector<label_type_t>(length, -1);
    update_locations();
}

void ParseState::add_edge(token_index_t head, token_index_t dep, label_type_t label) {
    heads[dep] = head;
    labels[dep] = label;
    // cout << "Add edge: H=" << head << " D= " << dep << endl;
}

bool ParseState::is_terminal() {
    // A parse state is final iff
    // 1) the front of the buffer points at the last element (the root node); and
    // 2) the stack is empty.
    return (n0 == (length - 1)) && stack.empty();
}


// Zero cost moves from current state in Arc Eager parsing
LabeledMoveSet ArcEager::oracle(const ParseState & state, const Sentence & sent) {
    auto & stack = state.stack;
    auto & tokens = sent.tokens;
    auto n0 = state.n0;
    
    LabeledMoveSet moves = allowed_labeled_moves(state, sent);
    
    if (stack.size() == 0)
        return moves;
    
    auto s0 = stack.back();
    
    // LEFT-ARC adds the edge (n, s) and removes s from the stack.
    // Precondition: Must not be ROOT node. Cannot have a head
    //
    // Any dependent of s still on the stack will be lost.
    //
    // The node s is guaranteed to not have dependencies with indexes larger than n
    // due to the projectivity constraint.
    // Skip checks if (n, s) is a gold edge.
    if (tokens[s0].head != n0) {
        // Is the original head still recoverable? If it is it must be in the buffer.
        bool is_head_recoverable = state.has_head_in_buffer(s0, sent);
        if (is_head_recoverable) {
            moves.set(Move::LEFT_ARC, false);
        }
    }
    
    // RIGHT-ARC adds the edge (s, b) and pushes b onto the stack.
    // Since b is assigned a head by this operation, it loses the ability
    // to attach to any gold heads further down in the stack, or in the buffer.
    // The element b also loses any dependents in the stack.
    
    // Skip check if the added edge is a part of the gold set.
    if (tokens[n0].head != s0) {
        // cout << "Checking RIGHT-ARC. Doesn't match (s, b) = (" << s0 << ", " << n0 << ") does not match. Gold of n0 is" << state.hea;
        // Perhaps the gold edge because of a previous error?
        // Let's check if that is at all recoverable.
        bool is_recoverable = state.has_head_in_stack(n0, sent) || state.has_head_in_buffer(n0, sent);
        // If the gold edge is not recoverable, we still need to worry about losing dependencies of s in the stack?
        if (is_recoverable || state.has_dep_in_stack(n0, sent))
            moves.set(Move::RIGHT_ARC, false);
    }
    
    // REDUCE pops the top element s from the stack.
    // This prevents s from acquiring dependents in either the buffer
    // or the stack.
    if (state.has_dep_in_stack(s0, sent) || state.has_dep_in_buffer(s0, sent)) {
        moves.set(Move::REDUCE, false);
    }
    
    // SHIFT moves n to the top of the stack.
    // The element loses any heads or dependents in the stack.
    if (state.has_head_in_stack(n0, sent) || state.has_dep_in_stack(n0, sent))
        moves.set(Move::SHIFT, false);
    
    // Add labels if LEFT_ARC or RIGHT_ARC are among the possibilities
    if (moves.test(Move::LEFT_ARC)) {
        // Arc (b, s)
        moves.label = tokens[s0].label;
    } else if (moves.test(Move::RIGHT_ARC)) {
        // Arc (s, b)
        moves.label = tokens[n0].label;
    }

    // If we for some reason found no zero-cost moves, simply
    // return all the allowed moves as possibilities
    // TODO check if zero-cost moves are guaranteed to exist
    if (!(moves.test(Move::SHIFT) || moves.test(Move::REDUCE) || moves.test(Move::LEFT_ARC) || moves.test(Move::RIGHT_ARC)))
        return allowed_labeled_moves(state, sent);


    return moves;
}

vector<LabeledMove> ArcEager::moves(size_t num_labels) {
    vector<LabeledMove> labeled_moves {};
    
    labeled_moves.push_back(LabeledMove(Move::SHIFT, -1));
    labeled_moves.push_back(LabeledMove(Move::REDUCE, -1));
    
    for (int i = 0; i < num_labels; i++)
        labeled_moves.push_back(LabeledMove(Move::LEFT_ARC, i));
    
    for (int i = 0; i < num_labels; i++)
        labeled_moves.push_back(LabeledMove(Move::RIGHT_ARC, i));
    
    
    // Assign indexes to the moves
    size_t counter = 0;
    for (auto & lmove : labeled_moves)
        lmove.index = counter++;
    
    return labeled_moves;
}

LabeledMoveSet ArcEager::allowed_labeled_moves(const ParseState &state, const Sentence & sent) {
    // This method only gives valid answers when the current state is non-terminal.
    // Specifically, it assumes that return value of `state.is_terminal` is false.
    LabeledMoveSet moves {};
    auto & stack = state.stack;

    if (state.stack.size() == 0) {
        assert(state.n0 < (state.length - 1));
        moves.set(Move::SHIFT);
        return moves;
    } else {
        // Allow everything initially, then remove
        // invalid moves
        moves.allow_all();

        // SHIFT moves N0 to the stack and is not possible
        // if N0 is set to the last token in the sentence.
        // RIGHT-ARC performs a SHIFT.
        if (state.n0 == (state.length-1)) {
            moves.set(Move::SHIFT, false);
            moves.set(Move::RIGHT_ARC, false);
        }

        // REDUCE throws S0 away,
        // and is illegal if S0 has not been assigned a head.
        if (state.heads[stack.back()] == -1) {
            moves.set(Move::REDUCE, false);
        }

        // LEFT-ARC makes B0 the head of S0.
        // It is an illegal move if S0 already has a head.
        if (state.heads[stack.back()] != -1)
            moves.set(Move::LEFT_ARC, false);
    }

    return moves;
}



void perform_move(LabeledMove lmove, ParseState &state, const vector<Token> &tokens) {
    auto & stack = state.stack;
    switch (lmove.move) {
        case Move::SHIFT:
            assert(state.n0 < tokens.size() - 1);
            stack.push_back(state.n0);
            state.n0++;
            break;
        case Move::RIGHT_ARC:
            assert(!stack.empty() && state.n0 < tokens.size() - 1);
            state.add_edge(stack.back(), state.n0, lmove.label);
            stack.push_back(state.n0);
            state.n0++;
            break;
        case Move::LEFT_ARC:
            assert(!stack.empty());
            state.add_edge(state.n0, stack.back(), lmove.label);
            stack.pop_back();
            break;
        case Move::REDUCE:
            assert(!stack.empty());
            stack.pop_back();
            break;
        default:
            throw std::runtime_error("Invalid move");
    }
    state.update_locations();
}


int ParseState::find_left_dep(int middle, int start) const {
    for (int i = start; i < middle; i++) {
        if (heads[i] == middle) return i;
    }
    return -1;
}


int ParseState::find_right_dep(int middle, int last) const {
    for (int i = last; i > middle; i--) {
        if (heads[i] == middle) return i;
    }
    return -1;
}



void ParseState::update_locations() {
    using namespace state_location;

    locations_.fill(-1);

    if (stack.size() >= 1) {
        locations_[S0] = stack.back();
        // auto max_head = max_element(heads.begin(), heads.end());
        // cout << *max_head << endl;
        locations_[S0_head] = heads[locations_[S0]];
        locations_[S0_left] = find_left_dep(locations_[S0], 0);
        if (locations_[S0_left] != -1)
            locations_[S0_left2] = find_left_dep(locations_[S0], locations_[S0_left] + 1);


        locations_[S0_right] = find_right_dep(locations_[S0], static_cast<int>(length - 1));
        if (locations_[S0_right] != -1)
            locations_[S0_right2] = find_right_dep(locations_[S0], locations_[S0_right] - 1);
    }

    locations_[N0] = n0;
    locations_[N0_left] = find_left_dep(locations_[N0], 0);
    if (locations_[N0_left] != -1)
        locations_[N0_left2] = find_left_dep(locations_[N0], locations_[N0_left] + 1);

    locations_[N0_right] = find_right_dep(locations_[N0], static_cast<int>(length - 1));

    if (locations_[N0] < (length - 1))
        locations_[N1] = locations_[N0] + 1;
    if (locations_[N0] < (length - 2))
        locations_[N2] = locations_[N0] + 2;
}


bool ParseState::has_head_in_buffer(token_index_t index, Sentence const & sent) const {
    return sent.tokens[index].head >= n0;
}

bool ParseState::has_head_in_stack(token_index_t index, Sentence const & sent) const {
    for (auto i : stack) {
        if (sent.tokens[index].head == i)
            return true;
    }
    return false;
}

bool ParseState::has_dep_in_buffer(token_index_t index, Sentence const & sent) const {
    for (int i = n0; i < length; i++) {
        if (sent.tokens[i].head == index)
            return true;
    }
    return false;
}

bool ParseState::has_dep_in_stack(token_index_t index, Sentence const & sent) const {
    for (auto i : stack) {
        if (sent.tokens[i].head == index)
            return true;
    }
    return false;
}

label_type_t CorpusDictionary::map_label(string label) {
    return map_any(label_to_id, label);
}

attribute_t CorpusDictionary::map_attribute(string attribute) {
    return map_any(attribute_to_id, attribute);
}


namespace_t CorpusDictionary::map_namespace(string ns) {
    return map_any(namespace_to_id, ns);
}

template <typename T>
T CorpusDictionary::map_any(unordered_map<string, T> & map, string key) {
    if (frozen) {
        auto got = map.find(key);
        if (got != map.end())
            return got->second;
        else
            return -1;
    } else {
        auto pair = map.emplace(key, map.size());
        return pair.first->second;
    }
}

const attribute_vector &Token::find_namespace(namespace_t ns, namespace_t token_specific_ns) const {
    auto ns_found = std::find_if(namespaces_ng.cbegin(), namespaces_ng.cend(),
                                 [ns,token_specific_ns](const NamespaceFront & ns_front) {
                                     return ns_front.index == ns && ns_front.token_specific_ns == token_specific_ns;} );


    if (ns_found != namespaces_ng.cend()) {
        return ns_found->attributes;
    } else {
        return _empty_attribute_vector;
    }
}


void enforce_arc_constraints(const ParseState &state, const Sentence &sent, LabeledMoveSet &allowed_moves) {
    // Check arc constraints
    const auto s0 = state.stack.back();
    const auto n0 = state.n0;

    for (const auto & ac : sent.arc_constraints) {
        // LEFT-ARC (S|i, j|B): adds (j, i), pops i from S
        // Makes any edge (x, i) and (i, x) unreachable, where x is in B
        if ((ac.head == s0 && ac.dep > n0) || (ac.dep == s0 && ac.head > n0))
            allowed_moves.set(Move::LEFT_ARC, false);

        // RIGHT-ARC (S|i, j|B): adds (i, j), pushes j on S
        // Makes any edge (j, x) and (x, j) unreachable, where x is in S
        if (ac.head == n0 || ac.dep == n0) {
            // Exclude top of the stack
            for (auto si = state.stack.cbegin(); si != state.stack.cend() - 1; si++) {
                if (ac.head == *si || ac.dep == *si) {
                    allowed_moves.set(Move::RIGHT_ARC, false);
                    continue;
                }
            }
        }

        // REDUCE (S|i, j|B): pops i from S
        // Makes any edge (x, i) and (i, x) unreachable, where x is in B
        if ((ac.head == s0 && ac.dep >= n0) || (ac.dep == s0 && ac.head >= n0))
            allowed_moves.set(Move::REDUCE, false);

        // SHIFT (S, j|B): pushes j onto S
        // Makes any edge (j, x) and (x, j) unreachable, where x is in S
        if (ac.head == n0 || ac.dep == n0) {
            for (auto si = state.stack.cbegin(); si != state.stack.cend(); si++) {
                if (ac.head == *si || ac.dep == *si) {
                    allowed_moves.set(Move::SHIFT, false);
                    continue;
                }
            }
        }
    }
}

void enforce_span_constraints(const ParseState &state, const Sentence &sent, LabeledMoveSet &allowed_moves) {
    const auto s0 = state.stack.back();
    const auto n0 = state.n0;

    for (const auto & sc : sent.span_constraints) {
        const bool s0_inside = sc.is_inside(s0);
        const bool n0_inside = sc.is_inside(n0);
        if (!(s0_inside || n0_inside))
            continue;
        
        const bool n0_is_root = n0_inside && sc.is_n0_root(state);
        const bool s0_is_root = s0_inside && sc.is_s0_root(state);
        const bool has_root = n0_is_root || s0_is_root || sc.has_root(state);
        assert(!(s0_is_root && n0_is_root));


        bool no_right_arc = false;
        bool no_shift = false;
        bool no_reduce = false;

        //
        // LEFT-ARC (S|i, j|B): adds (j, i), pops i from S
        //
        bool no_left_arc = false;

        // S0 is span root, but we are trying to make it a dependent of N0
        no_left_arc |= s0_is_root && n0_inside;

        // S0 is not the span root, but we are trying to make it the dependent of something outside of the span
        no_left_arc |= has_root && !s0_is_root && !n0_inside;

        // We're trying to make N0 the head of something outside the span, but it is not the root
        no_left_arc |= sc.permit_root_deps && n0_inside && !s0_inside && has_root && !n0_is_root;

        // We're trying to make N0 the head of a node outside the span, but we don't allow outside dependencies
        no_left_arc |= !sc.permit_root_deps && n0_inside && !s0_inside;

        //
        // RIGHT-ARC (S|i, j|B): adds (i, j), pushes j on S
        //

        // We can only add an edge from S0 to outside the span if S0 is the root,
        // and no span nodes remain on the stack.
        if (sc.span_end == s0) {
            no_right_arc |= !s0_is_root;

            if (state.stack.size() > 1) {
                const auto s1 = *(state.stack.cend() - 2);
                no_right_arc = no_right_arc && (sc.span_start <= s1 <= sc.span_end);
            }
        }

        // We're trying to make S0 head of N0, but N0 is span root.
        no_right_arc |= n0_is_root && s0_inside;

        // Although N0 is not span root, we're trying to make it a dependent of something outside the span
        no_right_arc |= !n0_is_root && !s0_inside;

        // We're giving S0 a dependent outside the span, but that's not allowed
        no_right_arc |= !sc.permit_root_deps && !n0_inside;

        // We're giving S0 a dependent outside the span, but S0 is not the root
        no_right_arc |= sc.permit_root_deps && !s0_is_root;

        //
        // REDUCE (S|i, j|B): pops i from S
        //

        // We're about to lose S0, but it's the root of the span, and there's
        // no way N0 can be a descendant of S0 already
        no_reduce = s0_inside && n0_inside && s0_is_root;

        //
        // SHIFT (S, j|B): pushes j onto S
        //

        // We're pushing the last token in the span to the stack, making
        // it unavailable for further attachments inside the span.
        // The span must therefore have no unfinished nodes.

        no_shift |= sc.span_end == n0 && (sc.span_start <= s0 <= sc.span_end);

        if (no_left_arc)    allowed_moves.set(Move::LEFT_ARC, false);
        if (no_right_arc)   allowed_moves.set(Move::RIGHT_ARC, false);
        if (no_shift)       allowed_moves.set(Move::SHIFT, false);
        if (no_reduce)      allowed_moves.set(Move::REDUCE, false);

    }
}

LabeledMoveSet ConstrainedArcEager::allowed_labeled_moves(const ParseState &state, const Sentence &sent) {
    LabeledMoveSet allowed_moves = ArcEager::allowed_labeled_moves(state, sent);
    enforce_arc_constraints(state, sent, allowed_moves);
    enforce_span_constraints(state, sent, allowed_moves);

    return allowed_moves;
}


bool SpanConstraint::is_s0_root(const ParseState &state) const {
    // Check whether the given node is root of the span.
    // A node is the root if one of the below conditions hold:
    // - node has a head outside the span
    // - node has a dependent outside
    token_index_t s0 = state.stack.back();
    
    if (state.heads[s0] < span_start || state.heads[s0] > span_end)
        return true;
    
    int s0_left = state.locations_[state_location::S0_left];
    int s0_right = state.locations_[state_location::S0_right];

    return (s0_left != -1 && s0_left < span_start) || (s0_right != -1 && s0_right < span_end);
}

bool SpanConstraint::is_n0_root(const ParseState &state) const {
    // Check whether the given node is root of the span.
    // A node is the root if one of the below conditions hold:
    // - node has a head outside the span
    // - node has a dependent outside
    token_index_t n0 = state.n0;

    if (state.heads[n0] < span_start || state.heads[n0] > span_end)
        return true;

    int n0_left = state.locations_[state_location::N0_left];
    int n0_right = state.locations_[state_location::N0_right];

    return (n0_left != -1 && n0_left < span_start) || (n0_right != -1 && n0_right < span_end);

}

bool SpanConstraint::has_root(const ParseState &state) const {
    // TODO Find a more efficient way to check this

    // A node inside the span has a dependent before the span
    for (int i = 0; i < span_start; i++) {
        if (span_start <= state.heads[i] <= span_end)
            return true;
    }

    // A node inside the span has a dependent after the span
    for (int i = span_end + 1; i < state.length; i++) {
        // Is modified by something inside the span
        if (span_start <= state.heads[i] <= span_end)
            return true;
    }

    // A node inside the span has a head outside the span
    for (int i = span_end; i <= span_end; i++) {
        if (state.heads[i] < span_start || state.heads[i] > span_end)
            return true;
    }

    return false;
}

void Sentence::score(const ParseResult &result, ParseScore &parse_score) const {
    assert(result.heads.size() == tokens.size());

    for (int i = 0; i < tokens.size(); i++) {
        bool head_correct = result.heads[i] == tokens[i].head;
        bool label_correct = result.labels[i] == tokens[i].label;

        parse_score.num_correct_unlabeled += head_correct;
        parse_score.num_correct_labeled += (head_correct && label_correct);
        parse_score.num_total += 1;
    }
}

float ParseScore::las() {
    if (num_total > 0)
        return static_cast<float>(num_correct_labeled) / num_total;
    else
        return 0;
}

float ParseScore::uas() {
    if (num_total > 0)
        return static_cast<float>(num_correct_unlabeled) / num_total;
    else
        return 0;
}
