#include "features.h"

#include <fstream>
#include <Python/Python.h>
#include <sstream> // stringstream


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

unsigned int Sentence::unlabeled_score(vector<token_index_t> pred_heads) {
    assert(pred_heads.size() == tokens.size());
    unsigned int uas = 0;
    for (int i = 0; i < tokens.size(); i++) {
        uas += (pred_heads[i] == tokens[i].head);
    }

    return uas;
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

bool ParseState::items_remaining() {
	// Maybe better to check for n0->head == -1
	return stack.size() > 0 || (n0 + 1) < length;
}

bool ParseState::is_terminal() {
    return !items_remaining();
}


LabeledMoveSet allowed_labeled_moves(const ParseState & state) {
    LabeledMoveSet moves {};
    auto & stack = state.stack;
    
    if (state.stack.size() == 0) {
        moves.set(Move::SHIFT);
        return moves;
    } else {
        // Allow everything initially, then remove
        // invalid moves
        moves.allow_all();
        
        // REDUCE throws S0 away,
        // and is illegal if S0 has not been assigned a head.
        if (state.heads[stack.back()] == -1)
            moves.set(Move::REDUCE, false);
        
        // LEFT-ARC makes B0 the head of S0.
        // It is an illegal move if S0 already has a head.
        if (state.heads[stack.back()] != -1)
            moves.set(Move::LEFT_ARC, false);
        
        // LEFT-ARC not possible if S0 is the ROOT token.
        if (stack.back() == (state.length - 1))
            moves.set(Move::LEFT_ARC, false);
    }
    
    return moves;
}

// Zero cost moves from current state in Arc Eager parsing
LabeledMoveSet ArcEager::oracle(const ParseState & state, const Sentence & sent) {
    auto & stack = state.stack;
    auto & tokens = sent.tokens;
    auto n0 = state.n0;
    
    LabeledMoveSet moves = allowed_labeled_moves(state);
    
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
        // Does s have any dependencies in the stack, which will be lost by attaching
        // s to b?
        if (is_head_recoverable || state.has_dep_in_stack(s0, sent)) {
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


void perform_move(LabeledMove lmove, ParseState &state, vector<Token> &tokens) {
    auto & stack = state.stack;
    switch (lmove.move) {
        case Move::SHIFT:
            stack.push_back(state.n0);
            state.n0++;
            break;
        case Move::RIGHT_ARC:
            state.add_edge(stack.back(), state.n0, lmove.label);
            stack.push_back(state.n0);
            state.n0++;
            break;
        case Move::LEFT_ARC:
            state.add_edge(state.n0, stack.back(), lmove.label);
            stack.pop_back();
            break;
        case Move::REDUCE:
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

const state_location_t ParseState::locations() const {
    auto loc = state_location_t();
    using namespace state_location;
    loc.fill(-1);

    if (stack.size() >= 1) {
        loc[S0] = stack.back();
        // auto max_head = max_element(heads.begin(), heads.end());
        // cout << *max_head << endl;
        loc[S0_head] = heads[loc[S0]];
        loc[S0_left] = find_left_dep(loc[S0], 0);
        if (loc[S0_left] != -1)
            loc[S0_left2] = find_left_dep(loc[S0], loc[S0_left] + 1);
        
        loc[S0_right] = find_right_dep(loc[S0], static_cast<int>(length - 1));
        if (loc[S0_right] != -1)
            loc[S0_right2] = find_right_dep(loc[S0], loc[S0_right] - 1);
    }
    
    loc[N0] = n0;
    loc[N0_left] = find_left_dep(loc[N0], 0);
    if (loc[N0_left] != -1)
        loc[N0_left2] = find_left_dep(loc[N0], loc[N0_left] + 1);
    
    if (loc[N0] < (length - 1))
        loc[N1] = loc[N0] + 1;
    if (loc[N0] < (length - 2))
        loc[N2] = loc[N0] + 2;
    
    return loc;
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

