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
        error_pos = &instance_begin;
        throw std::runtime_error("Bar '|' not found");
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
        error_pos = &header_begin;
        throw std::runtime_error("Ill-formatted header");
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
    char *last_char_converted = nullptr;
    val = strtof(&(*colon_pos) + 1, &last_char_converted);
    if (last_char_converted != (&(*str_pos) - 1) || errno != 0) {
        // Warn, or be silent?
        cout << "Conversion failed. Falling back to default value (1.0): " << string(colon_pos + 1, str_pos) << "\n";
        val = 1;
    }
    return val;
}


vector<Sentence> VwSentenceReader::read() {
    std::ifstream infile(filename);
    if (!infile.good())
        throw std::runtime_error("File " + filename + " cannot be read");

    corpus.clear();
    line_no = 1;

    string line;
    while (std::getline(infile, line)) {
        if (line.size() == 0) {
            finish_sentence();
        } else {
            // End line with a blank character
            line.push_back(' ');
            parse_instance(line.cbegin(), line.cend());
        }
        line_no++;
    }

    if (sent.tokens.size() > 0)
        finish_sentence();

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

    // Add finished sentence to corpus
    corpus.push_back(sent);
    sent = Sentence();
}

