#include "features.h"

#include <iostream>
#include <fstream>
#include <sstream> // stringstream
#include <string> // getline
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <cassert>


using namespace std;

std::array<std::string, static_cast<size_t>(Move::COUNT)> move_names = { "SHIFT", "REDUCE", "LEFT_ARC", "RIGHT_ARC" };

std::ostream& operator <<(std::ostream& os, const CParseStatePositions& s) {
    os << "CStatePositions"
    << "\n\tS0: " << s.S0
    << "\n\tS0_head: " << s.S0_head
    << "\n\tS0_left: " << s.S0_left
    << "\n\tS0_left2: " << s.S0_left2
    << "\n\tS0_right: " << s.S0_right
    << "\n\tS0_right2: " << s.S0_right2
    << "\n\tN0: " << s.N0
    << "\n\tN0_left: " << s.N0_left
    << "\n\tN0_left2: " << s.N0_left2
    << "\n\tN1: " << s.N1
    << "\n\tN2: " << s.N2
    << '\n';
    return os;
};

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




bool CSentence::has_edge(token_index_t i, token_index_t j) const {
	return tokens[i].head == j || tokens[j].head == i;
}

CParseState::CParseState(size_t length) : length(length) {
    assert(length >= 2);
    assert(length < 1000);
	stack.reserve(length);
	stack.push_back(0);
	n0 = 1;
    heads = vector<token_index_t> ( length, -1 );

}

void CParseState::add_edge(token_index_t head, token_index_t dep, relation_feature_t label) {
	heads[dep] = head;
    // cout << "Add edge: H=" << head << " D= " << dep << endl;
}

bool CParseState::items_remaining() {
	// Maybe better to check for n0->head == -1
	return stack.size() > 0 || (n0 + 1) < length;
}

bool CParseState::is_terminal() {
    return !items_remaining();
}


unsigned int valid_moves(const CParseState & state) {
    unsigned int moves = 0;

    if ((state.n0 + 1) < state.length) 
    	moves |= SHIFT;
    if (state.stack.size() >= 2)       
    	moves |= RIGHT_ARC;
    if (state.stack.size() >= 1)       
    	moves |= LEFT_ARC;

    return moves;
}


MoveSet allowed_moves(const CParseState & state) {
    MoveSet moves {};
    auto & stack = state.stack;

    if (state.stack.size() == 0) {
        moves.set(Move::SHIFT);
        return moves;
    } else {
        // Allow everything initially, then remove
        // invalid moves
        moves.moves.set();

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


LabeledMoveSet allowed_labeled_moves(const CParseState & state) {
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
MoveSet dynamic_oracle(const CParseState & state, const CSentence & sent) {
    auto & stack = state.stack;
    auto & tokens = sent.tokens;
    auto n0 = state.n0;
    
    MoveSet moves = allowed_moves(state);
    
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
    
    assert(moves.moves.any());
    
    return moves;
}


// Zero cost moves from current state in Arc Eager parsing
LabeledMoveSet labeled_dynamic_oracle(const CParseState & state, const CSentence & sent) {
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
        moves.label = tokens[s0].relation;
    } else if (moves.test(Move::RIGHT_ARC)) {
        // Arc (s, b)
        moves.label = tokens[n0].relation;
    }
    
    return moves;
}

// Zero cost moves from current state in Arc Eager parsing
LabeledMoveSet ArcEager::oracle(const CParseState & state, const CSentence & sent) {
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
        moves.label = tokens[s0].relation;
    } else if (moves.test(Move::RIGHT_ARC)) {
        // Arc (s, b)
        moves.label = tokens[n0].relation;
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



vector<LabeledMove> arc_eager_labeled_moves(size_t num_labels) {
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





unsigned int gold_moves(const CParseState & state, CSentence & sent) {
    unsigned int valid = valid_moves(state);
    auto & stack = state.stack;
    auto & tokens = sent.tokens;
    auto n0 = state.n0;

    // cout << "l(S)=" << stack.size() << " S0=" << stack.back() << " N0=" << n0 << endl;
    // cout << "n0 is " << n0 << endl;

    if (state.stack.size() == 0)
    	return SHIFT;

	// Front of buffer is dependent on the top of the stack
	if (SHIFT & valid && tokens[n0].head == stack.back())
		return SHIFT;

    // Top of stack is a dependent of front element in buffer
	if (tokens[stack.back()].head == n0)
		return LEFT_ARC;

    // If the word behind s0 is its gold head, Left is incorrect
    if (stack.size() >= 2 && tokens[stack.back()].head == stack[stack.size() - 2])
    	valid &= ~LEFT_ARC;

    // If there are any dependencies between n0 and the stack,
    // pushing n0 will lose them.
    if (any_of(stack.cbegin(), stack.cend(), [=](int si) -> int { return sent.has_edge(n0, si); })) {
    	valid &= ~SHIFT;
    }

    // If there are any dependencies between s0 and the buffer, popping
    // s0 will lose them.
    for (int i = n0; i < tokens.size(); i++) {
    	if (sent.has_edge(stack.back(), i)) {
			valid &= ~LEFT_ARC;    	
			valid &= ~RIGHT_ARC;
			break;
    	}
    }

    return valid;
}

void perform_move(LabeledMove lmove, CParseState &state, vector<CToken> &tokens) {
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
}

/*
void do_gold_parse(CSentence & sent) {
	vector<CToken> & tokens = sent.tokens;
	auto state = CParseState(tokens.size());


    while (state.items_remaining()) {
        cout << "----- New round -------\n";
        cout << "\tStack: ";
        for (auto s : state.stack)
            cout << s << " ";
        cout << "\n\tN0: " << state.n0;
        cout << "\n";
        cout << "Heads\n";
        for (int h : state.heads) cout << h << " ";
        cout << "\n";




        auto possible_moves = dynamic_oracle(state, sent);
        assert(possible_moves.moves.count() >= 1);
        cout << "Possible moves " << possible_moves;

		if (possible_moves.test(Move::LEFT_ARC))
            perform_move(Move::LEFT_ARC, state, tokens);
		else if (possible_moves.test(Move::RIGHT_ARC))
            perform_move(Move::RIGHT_ARC, state, tokens);
        else if (possible_moves.test(Move::REDUCE))
            perform_move(Move::REDUCE, state, tokens);
		else if (possible_moves.test(Move::SHIFT))
            perform_move(Move::SHIFT, state, tokens);
        else
			throw std::runtime_error("No move could be selected");

        auto feat_builder = FeatureBuilder();
        //feat_builder.extract(state, sent);
        // cout << feat_builder.features.size() << " features constructed\n";

        // cout << "*******\n";
//        auto loc = state.positions();
        // cout << loc;

    }

    cout << "\n********** Final parse\nHeads:\n\t";
    for (int h : state.heads) cout << h << " ";

}
*/

CSentence parse_conll_sentence(vector<string> & lines, CorpusDictionary & dictionary) {
    CSentence sent {};
    string column;

    vector <string> parts;

    auto word_ns = dictionary.map_namespace("w");
    auto pos_ns = dictionary.map_namespace("p");
    
    for (string line : lines) {
        stringstream linestream(line);
        while (std::getline(linestream, column, '\t')) parts.push_back(column);
        if (parts.size() < 8) {
            return sent;
        }

        // Check that the number of parts match
        CToken token;
        token.head = stoi(parts[6]) - 1;

        token.index = stoi(parts[0]) - 1;
        token.relation = dictionary.map_relation(parts[7]);
        token.lexical = dictionary.map_lexical(parts[1]);
        
        if (token.namespaces.size() != 2)
            token.namespaces.resize(2);

        /*
        auto lex_id = dictionary.map_lexical(parts[1]);
        cout << lex_id;
        auto pos_id = dictionary.map_pos(parts[3]);
        cout << pos_id;
         */
        
        auto word_attribute = Attribute(dictionary.map_attribute(parts[1]), 1.0);
        auto pos_attribute = Attribute(dictionary.map_attribute(parts[3]), 1.0);
        
        token.namespaces[word_ns].push_back(word_attribute);
        token.namespaces[pos_ns].push_back(pos_attribute);
        
        PosTag tag = PosTag { dictionary.map_pos(parts[3]), 1.0 };
        token.tags.push_back(tag);

        sent.tokens.push_back(token);
        // cout << token.form << ", " << token.tag << ", " << token.head << ", " << token.rel_type << ", " << token.index << endl;
        parts.clear();
    }


    int root_index = static_cast<int>(sent.tokens.size());
    // FIXME update to use namespaces
    auto root_token = CToken();
    

    // Fix references to the ROOT token ƒrom -1 to the index of the last element
    for (auto & token : sent.tokens)
        if (token.head == -1) token.head = root_index;


    return sent;
}


vector<CSentence> read_conll_sentences(string filename, CorpusDictionary & dictionary) {
    std::ifstream infile(filename);
    vector<CSentence> corpus;

    string line;
    vector<string> chunk;
    while (std::getline(infile, line)) {
        if (line.size() == 0) {
            corpus.push_back(parse_conll_sentence(chunk, dictionary));
            chunk.clear();
        } else {
            chunk.push_back(line);
        }
    }

    if (chunk.size() > 0)
        corpus.push_back(parse_conll_sentence(chunk, dictionary));

    return corpus;
}

/*
void benchmark_hash() {
	combined_feature &my_combo = *(new combined_feature());

	cout << sizeof(combined_feature) << endl;
	
	std::size_t cum_hash = 0;
	std::unordered_map<combined_feature, int, combined_feature_hash, combined_feature_equal> m6;
	int rounds = 1E6;
	auto t1 = std::chrono::high_resolution_clock::now();

	for (int i = 0; i < rounds; i++) {
		my_combo.lexical[0] = i + 1;
		
		cum_hash += combined_feature_hash()(my_combo);
		// auto hi = make_pair(my_combo, i);
		// cout << hi.second << endl;
		// m6.insert(hi);
	}

	cout << cum_hash << endl;
	auto t2 = std::chrono::high_resolution_clock::now();
	cout << "Took " << std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count() << " ms " << endl;
	double mb_hashed = sizeof(combined_feature) * rounds / 1E6;
	double seconds_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count() / 1000.0;

	cout << "MB hashed per second: " << mb_hashed / seconds_elapsed << endl;
}
*/

int old_main() {
	string filename = "/users/anders/code/rungsted-parser/src/john_loves_mary.conll";
    // string filename = "/users/anders/data/treebanks/english/gweb-reviews-dev.conll";
    CorpusDictionary dict = CorpusDictionary {};
    auto sents = read_conll_sentences(filename, dict);
    cout << sents.size() << " sentences read\n" << endl;
	auto sent = sents[1];
    // Print out the heads
    for (auto token : sent.tokens)
        cout << token.head << " ";
    cout << endl;


    // do_gold_parse(sent);




    // string animal = "abe";
    // hello(2, 5, animal, 100);
	return 0;
};

int CParseState::find_left_dep(int middle, int start) const {
    for (int i = start; i < middle; i++) {
        if (heads[i] == middle) return i;
    }
    return -1;
}

int CParseState::find_right_dep(int middle, int last) const {
    for (int i = last; i > middle; i--) {
        if (heads[i] == middle) return i;
    }
    return -1;
}


const state_location_t CParseState::locations() const {
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

std::vector<PosTag> & CSentence::pos(token_index_t index) {
    if (index == -1)
        return _empty_tags;
    else
        return tokens[index].tags;
}

lexical_feature_t CSentence::word(token_index_t index) {
    if (index == -1)
        return -1;
    else
        return tokens[index].lexical;
}

pos_feature_t CorpusDictionary::map_pos(string pos) {
    return map_any(pos_to_id, pos);
}

lexical_feature_t CorpusDictionary::map_lexical(string lexical) {
    return map_any(lexical_to_id, lexical);
}

relation_feature_t CorpusDictionary::map_relation(string relation) {
    return map_any(relation_to_id, relation);
}

namespace_t CorpusDictionary::map_namespace(string ns) {
    return map_any(namespace_to_id, ns);
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

bool CParseState::has_head_in_buffer(token_index_t index, CSentence const & sent) const {
    return sent.tokens[index].head >= n0;
}

bool CParseState::has_head_in_stack(token_index_t index, CSentence const & sent) const {
    for (auto i : stack) {
        if (sent.tokens[index].head == i)
            return true;
    }
    return false;
}

bool CParseState::has_dep_in_buffer(token_index_t index, CSentence const & sent) const {
    for (int i = n0; i < length; i++) {
        if (sent.tokens[i].head == index)
            return true;
    }
    return false;
}

bool CParseState::has_dep_in_stack(token_index_t index, CSentence const & sent) const {
    for (auto i : stack) {
        if (sent.tokens[i].head == index)
            return true;
    }
    return false;
}


size_t move_index(Move move) {
    return static_cast<size_t>(move);
};

unsigned int CSentence::unlabeled_score(vector<token_index_t> pred_heads) {
    assert(pred_heads.size() == tokens.size());
    unsigned int uas = 0;
    for (int i = 0; i < tokens.size(); i++) {
        uas += (pred_heads[i] == tokens[i].head);
    }

    return uas;
}

bool CToken::has_namespace(char ns) {
    return namespaces_[ns - first_printable_char] != nullptr;
}

attribute_vector & CToken::find_namespace(char ns) {
    assert(ns >= first_printable_char && ns <= last_printable_char);
    attribute_vector * vec = namespaces_[ns - first_printable_char];
    
    if (vec == nullptr) {
        namespace_list.emplace_front();
        return namespace_list.front();
    } else {
        return *vec;
    }
}