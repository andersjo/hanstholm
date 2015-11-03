#ifndef FEATURE_HANDLING_H
#define FEATURE_HANDLING_H

#include <vector>
#include <list>
#include <bitset>
#include <array>
#include <string>
#include <iterator>
#include <ostream>
#include <assert.h>
#include <unordered_map>


#include "hash.h"

const unsigned int max_labels = 64;
using head_index_t = int;
using token_index_t = int;
using weight_t = float;

using pos_feature_t = signed char;
using label_type_t = int;
using feature_type_t = signed char;
using lexical_feature_t = int;
using attribute_t = int;
using namespace_t = int;


/**
 * Maps words, part-of-speech tags, and labels to integers.
 */
class CorpusDictionary {
public:
    std::unordered_map<std::string, label_type_t> label_to_id;
    label_type_t map_label(std::string);
    
    std::unordered_map<std::string, attribute_t> attribute_to_id;
    attribute_t map_attribute(std::string);

    std::unordered_map<std::string, namespace_t> namespace_to_id;
    namespace_t map_namespace(std::string);

    bool frozen = false;
private:
    template <typename T>
    T map_any(std::unordered_map<std::string, T> &, std::string);
};

template <typename Key, typename Value>
std::unordered_map<Value, Key> invert_map(std::unordered_map<Key, Value> & orig_map) {
    std::unordered_map<Value, Key> inverted_map {};
    for (auto & kv_pair : orig_map)
        inverted_map.emplace(kv_pair.second, kv_pair.first);
    return inverted_map;
}


struct Attribute {
    size_t index;
    weight_t value;
    Attribute(size_t index, weight_t value) : index(index), value(value) {};
};

// Printable characters are in the range 32 - 126, both inclusive
constexpr size_t first_printable_char = 32;
constexpr size_t last_printable_char = 126;
constexpr size_t num_printable_chars = last_printable_char - first_printable_char + 1;

using attribute_vector = std::vector<Attribute>;

// FIXME better name
struct NamespaceFront {
    namespace_t index = -1;
    token_index_t token_specific_ns = -1;
    attribute_vector attributes;
    // std::vector<attribute_vector> edge_attributes {};
    /*
    NamespaceFront() {
        std::cout << "Constructor called " << attributes.size() << "\n";
    }
    */
};


struct Token {
    std::string id;
    std::vector<Attribute> attributes {};
    std::vector<attribute_vector> namespaces {};
    std::vector<NamespaceFront> namespaces_ng {};
    attribute_vector _empty_attribute_vector {};
    label_type_t label;
    token_index_t index;
    token_index_t head;
    const attribute_vector & find_namespace(namespace_t ns, namespace_t token_specific_ns = -1) const;
};

struct ParseResult {
    std::vector<token_index_t> heads;
    std::vector<label_type_t> labels;
    // ParseResult() = delete;
    ParseResult() = default;
    // Too much copying? Are arguments copied for passing into the constructor?
    ParseResult(std::vector<token_index_t> heads, std::vector<label_type_t> labels) : heads(heads), labels(labels) {
        assert(heads.size() == labels.size());
    };
};

struct ArcConstraint {
    token_index_t head;
    token_index_t dep;
    label_type_t label;

    ArcConstraint() = default;
    ArcConstraint(token_index_t head, token_index_t dep, label_type_t label) : head(head), dep(dep), label(label) { }
};

struct ParseState;

struct SpanConstraint {
    token_index_t span_start;
    token_index_t span_end;
    bool permit_root_deps = true;

    SpanConstraint() = default;
    SpanConstraint(token_index_t span_end, token_index_t span_start) : span_start(span_start), span_end(span_end)  { }
    bool is_inside(token_index_t index) const {
        return span_start <= index <= span_end;
    }

    bool has_root(const ParseState &state) const;
    bool is_s0_root(const ParseState &state) const;
    bool is_n0_root(const ParseState &state) const;
};

struct Sentence {
	std::vector <Token> tokens;
    std::vector <ArcConstraint> arc_constraints;
    std::vector <SpanConstraint> span_constraints;

	bool has_edge(token_index_t, token_index_t) const;
    unsigned int unlabeled_score(std::vector<token_index_t>);
};


namespace state_location {
    enum LocationName {
        S0, // Top of stack
        S0_head,  // Head of S0
        S0_left, // Leftmost modifier of S0
        S0_left2, // The second leftmost modifier of S0
        S0_right, // The rightmost modifier of S0
        S0_right2, // The second rightmost modifier of S0
        N0, // Front element in buffer
        N1, // Second element in buffer
        N2, // Third element in buffer
        N0_left, // Leftmost modifier of N0
        N0_left2, // The second leftmost modifier of N0
        N0_right, // Rightmost modifier of N0
        COUNT
    };

    static const std::unordered_map<std::string, LocationName> name_to_id = {
            {"S0", S0},
            {"S0_head", S0_head},
            {"S0_left", S0_left},
            {"S0_left2", S0_left2},
            {"S0_right", S0_right},
            {"S0_right2", S0_right2},
            {"N0", N0},
            {"N1", N1},
            {"N2", N2},
            {"N0_left", N0_left},
            {"N0_left2", N0_left2},
            {"N0_right", N0_right}
    };

}

using state_location_t = std::array<token_index_t, state_location::LocationName::COUNT>;


struct ParseState {
	size_t length;
	std::vector<token_index_t> stack;
    token_index_t n0;
	std::vector<token_index_t> heads;
    std::vector<label_type_t> labels;
    state_location_t locations_ {};

    ParseState(size_t length);
	void add_edge(token_index_t head, token_index_t dep, label_type_t label);

    void update_locations();

    int find_left_dep(token_index_t middle, token_index_t start) const;
    int find_right_dep(token_index_t middle, token_index_t end) const;

    bool has_head_in_buffer(token_index_t, const Sentence &) const;
    bool has_head_in_stack(token_index_t, const Sentence &) const;
    bool has_dep_in_buffer(token_index_t, const Sentence &) const;
    bool has_dep_in_stack(token_index_t, const Sentence &) const;

    bool is_terminal();
};



enum class Move {
	SHIFT, REDUCE, LEFT_ARC, RIGHT_ARC, COUNT
};


struct LabeledMove {
    Move move;
    label_type_t label;
    size_t index {};
    LabeledMove(Move move, label_type_t label) : move(move), label(label) {};
    // LabeledMove() = delete;
    bool operator ==(const LabeledMove &other) const {
        return (move == other.move) && (label == other.label);
    }
    bool operator !=(const LabeledMove &other) const {
        return !(*this == other);
    }
    
};

class LabeledMoveSet {
public:
    label_type_t label = -1;
    
    void set(LabeledMove lmove, bool value=true) {
        moves.set(static_cast<size_t>(lmove.move), value);
        label = lmove.label;
    }
    
    void set(Move move, bool value=true) {
        moves.set(static_cast<size_t>(move), value);
    }

    bool test(LabeledMove lmove) const {
        return test(lmove.move) && (label == -1 || label == lmove.label);
    }
    
    bool test(Move move) const {
        return moves.test(static_cast<size_t>(move));
    }
    
    void allow_all() {
        moves.set();
    }

private:
    std::bitset<static_cast<size_t>(Move::COUNT)> moves;
};

class MoveSet {
public:
    void set(Move move, bool value=true) {
        moves.set(static_cast<size_t>(move), value);
    }
    std::bitset<static_cast<size_t>(Move::COUNT)> moves;
    bool test(Move move) const {
        return moves.test(static_cast<size_t>(move));
    }
};

void perform_move(LabeledMove move, ParseState &state, const std::vector<Token> &tokens);

struct TransitionSystem {
    virtual LabeledMoveSet oracle(const ParseState & state, const Sentence & sent) = 0;
    virtual std::vector<LabeledMove> moves(size_t num_labels) = 0;
    virtual LabeledMoveSet allowed_labeled_moves(const ParseState & state, const Sentence & sent) = 0;
};

struct ArcEager : TransitionSystem {
    LabeledMoveSet oracle(const ParseState & state, const Sentence & sent) override;
    std::vector<LabeledMove> moves(size_t num_labels) override;
    LabeledMoveSet allowed_labeled_moves(const ParseState & state, const Sentence & sent) override;
};

struct ConstrainedArcEager : ArcEager {
    LabeledMoveSet allowed_labeled_moves(const ParseState & state, const Sentence & sent) override;
};



#endif