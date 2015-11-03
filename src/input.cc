//
//  input.cc
//  rungsted-parser
//
//  Created by Anders Johannsen on 20/02/15.
//  Copyright (c) 2015 Anders Johannsen. All rights reserved.
//


#include <iostream>
#include <fstream>
#include <sstream> // stringstream
#include <string> // getline
#include <vector>
#include <regex>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>
#include <cstdlib>

#include "input.h"


using namespace std;

VwSentenceReader::VwSentenceReader(string filename, CorpusDictionary &dictionary)
        : dictionary(dictionary), filename(filename) {

}


void VwSentenceReader::parse_instance(string::const_iterator instance_begin, string::const_iterator instance_end) {
    auto first_bar_pos = find(instance_begin, instance_end, '|');
    if (first_bar_pos != instance_end) {
        token = Token();
        parse_header(instance_begin, first_bar_pos);
        parse_body(first_bar_pos, instance_end);
        sent.tokens.push_back(token);
    } else {
        throw input_parse_error("Bar '|' not found", 0);
    }
}


void VwSentenceReader::parse_constraint(string::const_iterator constraint_begin, string::const_iterator constraint_end) {
    // FIXME slow with lots of copying
    std::stringstream ss(string(constraint_begin, constraint_end));
    string constraint_type;
    ss >> constraint_type;


    if (constraint_type == "arc") {
        while (!ss.eof()) {
            sent.arc_constraints.emplace_back();
            ArcConstraint &arc_constraint = sent.arc_constraints.back();
            arc_constraint.label = -1;
            ss >> arc_constraint.head;
            if (ss.peek() != '-')
                throw input_parse_error("Invalid constraint format", 0);
            ss.seekg(1, ss.cur);
            ss >> arc_constraint.dep;
            ss >> std::ws;
        }

    } else if (constraint_type == "span") {
        while (!ss.eof()) {
            sent.span_constraints.emplace_back();
            SpanConstraint &span_constraint = sent.span_constraints.back();
            ss >> span_constraint.span_start;
            if (ss.peek() != '-')
                throw input_parse_error("Invalid constraint format", 0);
            ss.seekg(1, ss.cur);
            ss >> span_constraint.span_end;
            ss >> std::ws;
        }
    } else {
        throw input_parse_error("Line expected to specify constraints and start with one of {arc, span}", 0);
    }



}

void VwSentenceReader::parse_header(string::const_iterator header_begin, string::const_iterator header_end) {
    // Header example:
    // nsubj-5 'id-of-content
    static const regex header_regex("(-?\\d+)-(.*)\\s'(.*)");
    smatch header_match{};
    bool match = regex_match(header_begin, header_end, header_match, header_regex);

    if (match) {
        token.head = stoi(header_match[1].str());
        token.label = dictionary.map_label(header_match[2].str());
        token.id = header_match[3].str();
    } else {
        throw input_parse_error("Ill-formatted header", 0);
    }
}

void VwSentenceReader::parse_body(string::const_iterator body_begin, string::const_iterator body_end) {
    // Feature section
    ns_name = "*";

    auto start_of_token = body_begin;
    for (auto str_pos = body_begin; str_pos < body_end; str_pos++) {
        // Tokens are delimited by whitespace. The line is padded with an extra whitespace.
        if (*str_pos == ' ') {
            if (*start_of_token == '|') {
                parse_namespace_decl(start_of_token, str_pos);
            } else {
                parse_feature_decl(start_of_token, str_pos);
            }
            start_of_token = str_pos + 1;
        }
    }
}

void VwSentenceReader::parse_feature_decl(string::const_iterator start_of_token, string::const_iterator str_pos) {// Feature declaration
    weight_t val = 1;

    // Find the last colon in the string if any
    // Features have optional numerical values, which are marked by a colon
    auto colon_pos = str_pos;
    for (; colon_pos > start_of_token && (*colon_pos != ':'); colon_pos--);

    if (colon_pos != start_of_token) {
        val = get_number_or_default(str_pos, colon_pos);
    } else {
        colon_pos = str_pos;
    }

    feature.assign(start_of_token, colon_pos);

    auto attribute_id = dictionary.map_attribute(feature);
    auto & current_ns = token.namespaces_ng.back();
    current_ns.attributes.emplace_back(attribute_id, val);
}

void VwSentenceReader::parse_namespace_decl(string::const_iterator start_of_token, string::const_iterator str_pos) {
    dependent_on_index = -1;
    if (distance(start_of_token, str_pos) > 1) {
        ns_name.assign(start_of_token + 1, str_pos);

        // Check if ns is an edge-dependent namespace
        auto dash_pos = ns_name.find('-');
        if (dash_pos != string::npos) {
            ns_name = ns_name.substr(0, dash_pos);
            if (ns_name.size() == 0)
                throw input_parse_error("Invalid namespace format", 0);
            dependent_on_index = stoi(ns_name.substr(dash_pos + 1));
        }
    } else {
        // Default namespace
        ns_name = "*";
    }

    // Insert a new namespace
    token.namespaces_ng.emplace_back();
    auto & current_ns = token.namespaces_ng.back();
    current_ns.index = dictionary.map_namespace(ns_name);
    current_ns.token_specific_ns = dependent_on_index;
}


weight_t VwSentenceReader::get_number_or_default(string::const_iterator str_pos, string::const_iterator colon_pos) {
    // Interpret the remainder of the feature as a numeric value.
    // We do this the C-way to avoid making temporary strings for std::stof.
    // Premature optimization?
    weight_t val;
    char *next_char = nullptr;
    val = strtof(&(*colon_pos) + 1, &next_char);
//    cerr << "Read val of " << val << "\n";
    if (next_char != &(*str_pos) || errno != 0) {
        cerr << "Conversion failed. Falling back to default value (1.0): " << string(colon_pos + 1, str_pos) << "\n";
        val = 1;
    }
    return val;
}


class ArcConstraintLineParser {


    std::vector<ArcConstraint> parse_line(string &line) {
        std::vector<ArcConstraint> constraints;
        std::stringstream ss(line);




        return constraints;
    };
};


vector<Sentence> VwSentenceReader::read() {
    std::ifstream infile(filename);
    if (!infile.good())
        throw std::runtime_error("File " + filename + " cannot be read");

    corpus.clear();
    line_no = 1;

    string line;
    try {
        while (std::getline(infile, line)) {
            if (line.size() == 0 && sent.tokens.size() > 0) {
                finish_sentence();
            } else {
                // End line with a blank character
                line.push_back(' ');

                if (line[0] == '#') {
                    parse_constraint(line.cbegin() + 1, line.cend());
                } else {
                    parse_instance(line.cbegin(), line.cend());
                }


            }
            line_no++;
        }

        if (sent.tokens.size() > 0)
            finish_sentence();

    } catch (input_parse_error e) {
        e.line_no = line_no;
        e.filename = filename;
        throw e;
    }

    return corpus;

}

void VwSentenceReader::finish_sentence() {
    // Add the artificial root content to the end of the sentence
    sent.tokens.emplace_back();
    auto &root_token = sent.tokens.back();
    root_token.id = "root";
    root_token.head = -2;
    root_token.label = dictionary.map_label("root");

    // Set the content index
    for (int i = 0; i < sent.tokens.size(); i++)
        sent.tokens[i].index = i;

    // Correct head entries that refer to the root node as -1
    for (auto &token : sent.tokens) {
        if (token.head == -1)
            token.head = root_token.index;
    }

    root_token.head = -1;

    // Check sentence consistency
    assert(sent.tokens.size() >= 2);

    // Add finished sentence to corpus
    corpus.push_back(sent);
    sent = Sentence();
}

const char *input_parse_error::what() const noexcept {
    std::string msg;
    msg.append("Input error in file ");
    msg.append(filename);
    msg.append(" on line ");
    msg.append(to_string(line_no));
    msg.append(": ");
    msg.append(message);

    return msg.c_str();
}
