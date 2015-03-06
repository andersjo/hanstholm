// S0:p + N0:p + ( S0:z @ N0:i )
// N0:p + f(S0:p, x)
// head ( N0 ) :p

#include "parse.h"

template<typename T, typename ...Args>
std::unique_ptr<T> make_unique( Args&& ...args )
{
    return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
}


// Break at whitespace, comma, and paranthesis
enum class LexTokenType {
    LOCATION,
    FUNCTION,
    FUNCTION_ARG_SEP,
    OPERATOR,
    LEFT_PAREN,
    RIGHT_PAREN,
};

struct FeatureCombinerBase {
    FeatureCombinerBase(std::string name) : name(name) {};
    std::string name;
};

using feature_combiner_uptr = std::unique_ptr<FeatureCombinerBase>;

struct Location : FeatureCombinerBase {
    Location(std::string &name, state_location::LocationName location_, namespace_t ns_)
            : FeatureCombinerBase(name), location(location_), ns(ns_) { }

    // feature_combiner_uptr location;
    state_location::LocationName location;
    namespace_t ns;
};

struct CartesianProduct : FeatureCombinerBase {
    CartesianProduct(std::string &name, feature_combiner_uptr lhs_, feature_combiner_uptr rhs_)
            : FeatureCombinerBase(name), lhs(std::move(lhs_)), rhs(std::move(rhs_)) {
    }

    feature_combiner_uptr lhs;
    feature_combiner_uptr rhs;
};

LexTokenType token_string_to_type(std::string token_str);


struct LexToken {
    LexToken() = default;
    LexToken(std::string token_) : content(token_), type(token_string_to_type(token_)) {};
    std::string content;
    LexTokenType type;
    int precedence();
    int arity();
    bool is_left_assoc();
    bool is_right_assoc();

    virtual feature_combiner_uptr apply() {
        throw std::runtime_error("Not implemented");
    };
    virtual feature_combiner_uptr apply(feature_combiner_uptr) {
        throw std::runtime_error("Not implemented");
    };
    virtual feature_combiner_uptr apply(feature_combiner_uptr, feature_combiner_uptr) {
        throw std::runtime_error("Not implemented");
    };
};

using lex_token_uptr = std::unique_ptr<LexToken>;

struct LocationToken : LexToken {
    LocationToken(std::string, CorpusDictionary &);

    state_location::LocationName location;
    namespace_t ns;

    feature_combiner_uptr apply() override;
};

struct FunctionToken : LexToken {
    FunctionToken(std::string content);
    feature_combiner_uptr apply(feature_combiner_uptr, feature_combiner_uptr) override;
};


std::vector<lex_token_uptr> tokenize_line(std::string line, CorpusDictionary & dict);
std::vector<lex_token_uptr> infix_to_prefix(std::vector<lex_token_uptr > & infix_tokens);
feature_combiner_uptr make_feature_combiner(std::vector<lex_token_uptr> & prefix_tokens);