#ifndef PARSE_H
#define PARSE_H

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
using relation_feature_t = short;
using feature_type_t = signed char;
using lexical_feature_t = int;
using attribute_t = int;
using namespace_t = uint8_t;

/**
 * Maps words, part-of-speech tags, and relations to integers.
 */
class CorpusDictionary {
public:
    std::unordered_map<std::string, pos_feature_t> pos_to_id;
    pos_feature_t map_pos(std::string);

    std::unordered_map<std::string, lexical_feature_t> lexical_to_id;
    lexical_feature_t map_lexical(std::string);

    std::unordered_map<std::string, relation_feature_t> relation_to_id;
    relation_feature_t map_relation(std::string);
    
    std::unordered_map<std::string, namespace_t> namespace_to_id;
    namespace_t map_namespace(std::string);
    
    // FIXME separate mapping by namespace?
    std::unordered_map<std::string, namespace_t> attribute_to_id;
    attribute_t map_attribute(std::string);
    
    
    bool frozen = false;
private:
    template <typename T>
    T map_any(std::unordered_map<std::string, T> &, std::string);
};


struct PosTag {
    pos_feature_t tag;
    weight_t weight;
};

struct Attribute {
    size_t index;
    lexical_feature_t value;
    namespace_t ns;
    Attribute(size_t index, lexical_feature_t value) : index(index), value(value) {};
    Attribute(size_t index, lexical_feature_t value, uint8_t ns) : index(index), value(value), ns(ns) {};
};

// Printable characters are in the range 32 - 126, both inclusive
constexpr size_t first_printable_char = 32;
constexpr size_t last_printable_char = 126;
constexpr size_t num_printable_chars = last_printable_char - first_printable_char + 1;

using attribute_vector = std::vector<Attribute>;
struct CToken {
    lexical_feature_t lexical = -1;
    std::vector<PosTag> tags;
    std::vector<std::vector<Attribute>> namespaces;
    std::array<attribute_vector*, num_printable_chars> namespaces_;
    std::list<attribute_vector> namespace_list;
    attribute_vector & find_namespace(char);
    bool has_namespace(char ns);
    // attribute_vector get_namespace(char);

    std::string id;
    relation_feature_t relation = -1;
    std::vector<Attribute> attributes {};

    token_index_t index;
    token_index_t head;
};



struct CSentence {
	std::vector <CToken> tokens;
	bool has_edge(token_index_t, token_index_t) const;
    std::vector<PosTag> & pos(token_index_t);
    lexical_feature_t word(token_index_t);
    unsigned int unlabeled_score(std::vector<token_index_t>);

    std::vector<PosTag> _empty_tags {};
};

struct ParseResult {
    std::vector<token_index_t> heads;
    std::vector<relation_feature_t> relations;
    ParseResult() = delete;
    ParseResult(size_t num_tokens) : heads(num_tokens, -1), relations(num_tokens, -1) {};
};
/*
enum StatePosition {
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
    N0_left2 // The second leftmost modifier of N0
};
 */

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
        COUNT
    };
}

using state_location_t = std::array<token_index_t, state_location::LocationName::COUNT>;

struct CParseStatePositions {
    /// Top of stack (S0)
    token_index_t S0 = -1;
    /// Head of S0
    token_index_t S0_head = -1;
    /// Leftmost modifier of S0
    token_index_t S0_left = -1;
    /// The second leftmost modifier of S0
    token_index_t S0_left2 = -1;

    /// The rightmost modifier of S0
    token_index_t S0_right = -1;
    /// The second rightmost modifier of S0
    token_index_t S0_right2 = -1;

    /// Front element in buffer
    token_index_t N0 = -1;
    /// Second element in buffer
    token_index_t N1 = -1;
    /// Third element in buffer
    token_index_t N2 = -1;
    /// Leftmost modifier of N0
    token_index_t N0_left = -1;
    /// The second leftmost modifier of N0
    token_index_t N0_left2 = -1;
};

std::ostream& operator <<(std::ostream& os, const CParseStatePositions& s);

struct CParseState {
	size_t length;
	std::vector<token_index_t> stack;
    token_index_t n0;
	std::vector<token_index_t> heads;
    CParseState(size_t length);
	bool items_remaining();
	void add_edge(token_index_t head, token_index_t dep, relation_feature_t label);
    
    const state_location_t locations() const;
    
    

    int find_left_dep(token_index_t middle, token_index_t start) const;
    int find_right_dep(token_index_t middle, token_index_t end) const;

    bool has_head_in_buffer(token_index_t, const CSentence &) const;
    bool has_head_in_stack(token_index_t, const CSentence &) const;
    bool has_dep_in_buffer(token_index_t, const CSentence &) const;
    bool has_dep_in_stack(token_index_t, const CSentence &) const;

    bool is_terminal();
};



enum class Move {
	SHIFT, REDUCE, LEFT_ARC, RIGHT_ARC, COUNT
};

size_t move_index(Move move);


struct LabeledMove {
    Move move;
    relation_feature_t label;
    size_t index {};
    LabeledMove(Move move, relation_feature_t label) : move(move), label(label) {};
    LabeledMove() = delete;
    bool operator ==(const LabeledMove &other) const {
        return (move == other.move) && (label == other.label);
    }
    bool operator !=(const LabeledMove &other) const {
        return !(*this == other);
    }
    
};

std::vector<LabeledMove> arc_eager_labeled_moves(size_t num_labels);

class LabeledMoveSet {
public:
    relation_feature_t label = -1;
    
    void set(LabeledMove lmove, bool value=true) {
        moves.set(static_cast<size_t>(lmove.move), value);
        label = lmove.label;
    }
    
    void set(Move move, bool value=true) {
        moves.set(static_cast<size_t>(move), value);
    }

    bool test(LabeledMove lmove) const {
        return test(lmove.move) && label == lmove.label;
    }
    
    
    bool test(Move move) const {
        return moves.test(static_cast<size_t>(move));
    }
    
    void allow_all() {
        moves.set();
    }
    
    LabeledMove first_valid() {
        for (size_t i = 0; i < moves.size(); i++) {
            auto lmove = LabeledMove(static_cast<Move>(i), label);
            if (test(lmove)) return lmove;
        }
        throw std::runtime_error("No valid moves");
    }
private:
    std::bitset<static_cast<size_t>(Move::COUNT)> moves;
};

LabeledMoveSet labeled_dynamic_oracle(const CParseState & state, const CSentence & sent);


// assert(move_names.size() == Move::COUNT - 1);

class MoveSet {
public:
    void set(Move move, bool value=true) {
        moves.set(static_cast<size_t>(move), value);
    }
    std::bitset<static_cast<size_t>(Move::COUNT)> moves;
    bool test(Move move) const {
        return moves.test(static_cast<size_t>(move));
    }

    /*
    template <typename Value>
    Move best(Value value) {
        int best = -1;
        weight_t best_weight = 0;
        for (int i = 0; i < moves.size(); i++) {
            if (moves.test(i)) {
                weight_t val = value(static_cast<Move>(i));
                if (val >= best_weight) {

                }



            }
        }
    }
    */
    Move first_valid() {
        for (size_t i = 0; i < moves.size(); i++) {
            if (moves.test(i)) return static_cast<Move>(i);
        }
        throw std::runtime_error("No valid moves");
    }

};

MoveSet dynamic_oracle(const CParseState & state, const CSentence & sent);



enum LegacyMove {
    SHIFT = 1, REDUCE = 2, LEFT_ARC = 4, RIGHT_ARC = 8
};



unsigned int gold_moves(CParseState &, std::vector<CToken> &);
void perform_move(LabeledMove move, CParseState &state, std::vector<CToken> &tokens);


struct ArcEager {
    using ParseState = CParseState;
    static LabeledMoveSet oracle(const CParseState & state, const CSentence & sent);
    static std::vector<LabeledMove> moves(size_t num_labels);
};


struct CArcEager {
	CParseState & parse_state;
	CArcEager(CParseState & parse_state) : parse_state(parse_state) { }
	bool shift_allowed();
	bool reduce_allowed();
	bool left_arc_allowed();
	bool right_arc_allowed();
};


std::vector<CSentence> read_conll_sentences(std::string filename, CorpusDictionary & dictionary);

/*
struct combined_feature {
	std::array<lexical_feature_t, 2> lexical;
	std::array<feature_type_t, 2> lexical_types;
	std::array<pos_feature_t, 3> pos;
	std::array<feature_type_t, 3> pos_types;

	unsigned short distance;
	unsigned short label;

	std::bitset<max_labels> label_set_left;
	std::bitset<max_labels> label_set_right;
};

struct combined_feature_hash {
 std::size_t operator()(const combined_feature& c) const
 {
     std::size_t seed = 0;
     hash_range(seed, c.lexical.cbegin(), c.lexical.cend());
     hash_range(seed, c.lexical_types.cbegin(), c.lexical_types.cend());
     hash_range(seed, c.pos.cbegin(), c.pos.cend());
     hash_range(seed, c.pos_types.cbegin(), c.pos_types.cend());

     hash_combine(seed, c.distance);
     hash_combine(seed, c.label);
     hash_combine(seed, c.label_set_left);
     hash_combine(seed, c.label_set_right);

     return seed;
 }
};
 
struct combined_feature_equal {
 bool operator()(const combined_feature& lhs, const combined_feature& rhs) const
 {
    return 	lhs.lexical == rhs.lexical &&
    		lhs.lexical_types == rhs.lexical_types &&
    		lhs.pos == rhs.pos && 
    		lhs.pos_types == rhs.pos_types &&
    		lhs.distance == rhs.distance &&
    		lhs.label == rhs.label && 
    		lhs.label_set_left == rhs.label_set_left &&
    		lhs.label_set_right == rhs.label_set_right;

 }
};
*/

// The Zhang and Nivre feature set
//
// The paper defines a set of feature templates, which are combinations of 
// atomic features. Each atomic feature is characterized by a relative position (e.g. N_0, S_1, ...)
// in the current parser state, a value type (e.g. word form or part of speech), and a value (e.g. NOUN).
//
// S0wpN0wp : 4 atoms
// S0pN0pd : 
// wp w p wp w p wp w pwpwp

// wpwp wpw wwp S0wpN0p S0pN0wp S0wN0w S0pN0p N0pN1p
// N0pN1pN2p S0pN0pN1p S0hpS0pN0p S0pS0lpN0p S0pS0rpN0p S0pN0pN0lp
// S0wd S0pd N0wd N0pd S0wN0wd S0pN0pd
// S0wvr S0pvr S0wvl S0pvl N0wvl N0pvl
// S0hw S0hp S0l S0lw S0lp S0l 

// 2w  2p = 2*4 + 2*1 = 10 bytes

// - distance (between S_0 and N_0)
// - valency for a given head
// - single label
// - label set of dependents to the left or right of the head (takes N bits)
// - word form
// - pos
//
// What should be the representation of a combined feature? A string? A byte array?

#endif