#include <iostream>
#include "output.h"

void output_parse_result(std::ostream & out, const Sentence & sentence, const ParseResult & result) {
    std::cout << "Hello from output_parse_result()\n" << std::endl;

    // assert(sentence.tokens.size() == result.heads.size() == result.labels.size());
    out << "Parsed sentence with " << sentence.tokens.size() << " tokens. ParseResult has" << result.heads.size() << "\n";
}

