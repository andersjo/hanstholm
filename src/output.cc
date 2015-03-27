#include <iostream>
#include "output.h"

void output_parse_result(std::ostream & out, const Sentence & sentence, const ParseResult & result,
        std::unordered_map<label_type_t, std::string> & id_to_label) {
    assert(sentence.tokens.size() == result.heads.size());
    assert(sentence.tokens.size() == result.labels.size());
    for (int i = 0; i < sentence.tokens.size() - 1; i++) {
        auto token = sentence.tokens[i];
        auto pred_head = result.heads[i];
        auto pred_label = result.labels[i];

        out << token.id << "\t"
                << token.head << "-" << id_to_label[token.label] << "\t"
                << pred_head << "-" << id_to_label[pred_label] << "\n";
    }


    //

}

