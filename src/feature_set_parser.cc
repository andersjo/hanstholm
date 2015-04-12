#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <assert.h>
#include "feature_set_parser.h"

#include <boost/algorithm/string.hpp>


LexTokenType token_string_to_type(std::string token_str) {
    // Refine this with list of allowed operators,
    if (token_str == "(")
        return LexTokenType::LEFT_PAREN;
    else if (token_str == ")")
        return LexTokenType::RIGHT_PAREN;
    else if (token_str == ",")
        return LexTokenType::FUNCTION_ARG_SEP;
    else if (token_str.find(':') != std::string::npos)
        return LexTokenType::LOCATION;
    else if (token_str.find_first_of("@+-*") == 0)
        return LexTokenType::OPERATOR;
    else
        return LexTokenType::FUNCTION;

    throw std::runtime_error("Could not assign a type to the content '" + token_str + "'");

};


int LexToken::precedence() {
    switch (content[0]) {
        case '@':
        case '*':
            return 2;
        case '+':
        case '-':
            return 1;
        default:
            throw std::runtime_error("Invalid operator '" + content + "'");
    }
}

bool LexToken::is_left_assoc() {
    return true;

}

bool LexToken::is_right_assoc() {
    return false;
}


std::vector<lex_token_uptr> tokenize_line(std::string line, CorpusDictionary & dict) {
    size_t start_at = 0;
    size_t found = 0;
    std::vector<lex_token_uptr> tokens;

    // Ensure that the last content is detected by adding whitespace
    line.push_back(' ');

    while ((found = line.find_first_of(" (),", start_at)) != std::string::npos) {
        // Skip over whitespace
        for (; isspace(line[start_at]) && start_at < found; start_at++);

        if ((found - start_at) > 0) {
            auto token_str = line.substr(start_at, found - start_at);
            auto token_type = token_string_to_type(token_str);
            if (token_type == LexTokenType::LOCATION) {
                tokens.push_back(make_unique<LocationToken>(token_str, dict));
            } else if (token_type == LexTokenType::OPERATOR || token_type == LexTokenType::FUNCTION) {
                tokens.push_back(make_unique<FunctionToken>(token_str));
            } else {
                tokens.push_back(make_unique<LexToken>(token_str));
            }
        }

        start_at = found + 1;
    }
    return tokens;
}

std::vector<lex_token_uptr> infix_to_prefix(std::vector<lex_token_uptr> & infix_tokens) {
    std::vector<lex_token_uptr> output;
    std::vector<lex_token_uptr> operator_stack;

    for (auto & token : infix_tokens) {
        switch (token->type) {
            case LexTokenType::LOCATION:
                output.push_back(std::move(token));
                break;

            case LexTokenType::LEFT_PAREN:
            case LexTokenType::FUNCTION:
                operator_stack.push_back(std::move(token));
                break;

            case LexTokenType::FUNCTION_ARG_SEP:
                while (!operator_stack.empty() && operator_stack.back()->type != LexTokenType::LEFT_PAREN) {
                    output.push_back(std::move(operator_stack.back()));
                    operator_stack.pop_back();
                }

                if (operator_stack.back()->type != LexTokenType::LEFT_PAREN)
                    throw std::runtime_error("Unbalanced parenthesis or misplaced comma in function argument");

                break;

            case LexTokenType::OPERATOR:
                while (!operator_stack.empty() && operator_stack.back()->type == LexTokenType::OPERATOR) {
                    auto &op_from_stack = operator_stack.back();
                    if (token->is_left_assoc() && token->precedence() <= op_from_stack->precedence()) {
                        output.push_back(std::move(op_from_stack));
                        operator_stack.pop_back();
                    } else if (token->is_right_assoc() && token->precedence() < op_from_stack->precedence()) {
                        output.push_back(std::move(op_from_stack));
                        operator_stack.pop_back();
                    }
                }
                operator_stack.push_back(std::move(token));
                break;

            case LexTokenType::RIGHT_PAREN:
                while (!operator_stack.empty() && operator_stack.back()->type != LexTokenType::LEFT_PAREN) {
                    output.push_back(std::move(operator_stack.back()));
                    operator_stack.pop_back();
                }

                if (operator_stack.empty())
                    throw std::runtime_error("Unmatched parenthesis");

                // Pop the left parenthesis of the stack
                operator_stack.pop_back();
                break;

            default:
                throw std::runtime_error("Invalid content type for content'" + token->content + "'");
        }
    }

    while (!operator_stack.empty()) {
        // FIXME check for parenthesis errors
        output.push_back(std::move(operator_stack.back()));
        operator_stack.pop_back();
    }

    return output;
}

int LexToken::arity() {
    if (type == LexTokenType::FUNCTION || type == LexTokenType::OPERATOR) {
        return 2;
    } else {
        return 0;
    }
}


feature_combiner_uptr make_feature_combiner(std::vector<lex_token_uptr> & prefix_tokens) {
    std::vector<feature_combiner_uptr > stack;
    for (auto & token : prefix_tokens) {
        assert(token->arity() <= 2);
        if (token->arity() > stack.size())
            throw std::runtime_error("Not enough arguments for content '" + token->content + "'");

        switch (token->arity()) {
            case 0:
                stack.push_back(std::move(token->apply()));
                break;
//            case 1:
//                stack.push_back(std::move(apply_token(token, std::move(*(stack.end() - 1)) )));
//                stack.pop_back();
//                break;
            case 2:
                auto arg1 = std::move(stack.back());
                stack.pop_back();
                auto arg2 = std::move(stack.back());
                stack.pop_back();
                stack.push_back(std::move(token->apply(std::move(arg1), std::move(arg2))));
                break;
        }
    }

    if (stack.size() != 1)
        throw std::runtime_error("Parsing failed");

    return std::move(stack.back());

}

LocationToken::LocationToken(std::string content, CorpusDictionary & dict) : LexToken(content) {
    auto colon_pos = content.find(":");
    if (colon_pos == std::string::npos)
        throw std::runtime_error("Missing colon in location: " + content);

    auto location_str = content.substr(0, colon_pos);
    auto ns_str = content.substr(colon_pos + 1);

    auto search = state_location::name_to_id.find(location_str);

    if (search == state_location::name_to_id.end())
        throw std::runtime_error("Location '" + location_str + "' not supported. Location names are case sensitive.");

    location = search->second;
    ns = dict.map_namespace(ns_str);
}


feature_combiner_uptr LocationToken::apply() {
    return make_unique<Location>(content, location, ns);
}

FunctionToken::FunctionToken(std::string content) : LexToken(content) {

}

feature_combiner_uptr FunctionToken::apply(feature_combiner_uptr arg1, feature_combiner_uptr arg2) {
    std::string combined_name;
    if (type == LexTokenType::OPERATOR)
        combined_name = "(" + arg1->name + " " + content + " " + arg2->name + ")";
    else
        combined_name = content + "(" + arg1->name + ", " + arg2->name + ")";

    if (content == "++") {
        return make_unique<CartesianProduct>(combined_name, std::move(arg1), std::move(arg2));
    }

    throw std::runtime_error("Operator or function ' " + content + "' not supported.");

}

std::unique_ptr<UnionList> read_feature_file(std::string filename, CorpusDictionary & dict) {
    std::ifstream infile(filename);
    if (!infile.good())
        throw std::runtime_error("File " + filename + " cannot be read");

    std::string line;
    std::list<feature_combiner_uptr > combiners;
    while (std::getline(infile, line)) {
        // Ignore everything after comments
        line = line.substr(0, line.find_first_of('#'));
        boost::algorithm::trim(line);
        if (line.size() != 0) {
            auto infix_tokens = tokenize_line(line, dict);
            auto prefix_tokens = infix_to_prefix(infix_tokens);
            auto combiner = make_feature_combiner(prefix_tokens);
            combiners.push_back(std::move(combiner));
        }
    }


    auto union_list = make_unique<UnionList>(combiners);
    return union_list;
}
