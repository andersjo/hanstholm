#ifndef OUTPUT_H
#define OUTPUT_H

#include <ostream>
#include "feature_handling.h"

void output_parse_result(std::ostream & ostream, const Sentence & sentence, const ParseResult & result,
        std::unordered_map<label_type_t, std::string> & id_to_label);

#endif