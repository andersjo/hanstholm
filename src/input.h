//
//  input.h
//  rungsted-parser
//
//  Created by Anders Johannsen on 20/02/15.
//  Copyright (c) 2015 Anders Johannsen. All rights reserved.
//

#ifndef rungsted_parser_input_h
#define rungsted_parser_input_h

#include <regex>
#include "feature_handling.h"

class VwSentenceReader {
public:
    VwSentenceReader(std::string filename, CorpusDictionary & dictionary);
    VwSentenceReader() = delete;
    std::vector<Sentence> read();
private:
    void parse_instance(std::string::const_iterator, std::string::const_iterator);
    void parse_header(std::string::const_iterator, std::string::const_iterator);
    void parse_body(std::string::const_iterator, std::string::const_iterator);
    void finish_sentence();
    weight_t get_number_or_default(std::string::const_iterator, std::string::const_iterator);
    
    std::string::const_iterator * error_pos;
    CorpusDictionary & dictionary;
    std::string filename;
    std::vector<Sentence> corpus;
    size_t line_no;
    Sentence sent {};
    Token token {};
    
    std::regex ws_re{"\\s+", std::regex::optimize};
    std::string ns_name;
    std::string feature;

    int dependent_on_index = -1;


    void parse_namespace_decl(std::string::const_iterator start_of_token, std::string::const_iterator  str_pos);
    void parse_feature_decl(std::string::const_iterator start_of_token, std::string::const_iterator str_pos);
};

#endif
