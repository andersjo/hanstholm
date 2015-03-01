#ifndef OUTPUT_H
#define OUTPUT_H

#include <ostream>
#include "parse.h"

void output_parse_result(std::ostream & ostream, const Sentence & sentence, const ParseResult & result);

#endif