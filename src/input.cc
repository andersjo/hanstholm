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

VwSentenceReader::VwSentenceReader(string filename, CorpusDictionary & dictionary) : filename(filename), dictionary(dictionary) {
    
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
    // nsubj-5 'id-of-token
    static const regex header_regex("(-?\\d+)-(.*)\\s'(.*)");
    smatch header_match {};
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
    auto str_token_begin = sregex_token_iterator(body_begin, body_end, ws_re, -1);
    auto str_token_end = sregex_token_iterator();
    char ns_prefix = '*';
    
    
    for (auto str_token = str_token_begin; str_token != str_token_end; str_token++) {
        if (str_token->first[0] == '|') {
            // Namespace
            if (str_token->length() > 1) {
                ns_prefix = *(str_token->first + 1);
            } else {
                // default namespace
                ns_prefix = '*';
            }
        } else {
            // Feature declaration
            // Features have optional numerical values, which are marked by a colon
            
            // FIXME search from the back instead of the front
            auto colon_pos = std::find(str_token->first, str_token->second, ':');
            string feature {};
            feature.push_back(ns_prefix);
            feature.push_back('^');
            feature.append(str_token->first, colon_pos);
            
            weight_t val = 1;
            if (colon_pos != str_token->second) {
                // Colon is found. Try to parse the value.
                // We create a temporary string to hold the number.
                // Could be done more efficiently
                auto number_string = string(colon_pos + 1, str_token->second);
                try {
                    val = stof(number_string);
                } catch (invalid_argument &e) {
                    error_pos = &colon_pos;
                    throw std::runtime_error("Invalid number after colon");
                }
            }
            
            // FIXME clear up terminology: what is an attribute, feature, etc?
            auto attribute_id = dictionary.map_attribute(feature);
            token.attributes.emplace_back(attribute_id, val, ns_prefix);
        }
    }
}



vector<Sentence> VwSentenceReader::read() {
    std::ifstream infile(filename);
    corpus.clear();
    line_no = 1;
    
    string line;
    while (std::getline(infile, line)) {
        if (line.size() == 0) {
            finish_sentence();
        } else {
            parse_instance(line.cbegin(), line.cend());
        }
        line_no++;
    }
    
    if (sent.tokens.size() > 0)
        finish_sentence();
    
    return corpus;
    
}

void VwSentenceReader::finish_sentence() {
    // Add the artificial root token to the end of the sentence
    sent.tokens.emplace_back();
    auto & root_token = sent.tokens.back();
    root_token.id = "root";
    root_token.head = -2;
    root_token.label = dictionary.map_label("root");
    
    // Set the token index
    for (int i = 0; i < sent.tokens.size(); i++)
        sent.tokens[i].index = i;
    
    // Correct head entries that refer to the root node as -1
    for (auto & token : sent.tokens) {
        if (token.head == -1)
            token.head = root_token.index;
    }
    
    root_token.head = -1;

    // Add finished sentence to corpus
    corpus.push_back(sent);
    sent = Sentence();
}
