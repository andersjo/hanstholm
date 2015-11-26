//
// Created by Anders Johannsen on 26/11/2015.
//

#ifndef HANSTHOLM_NONPROJ_H
#define HANSTHOLM_NONPROJ_H

#include "feature_handling.h"

//bool is_projective(std::vector<token_index_t> heads);
class Projectivizer {
public:
    token_index_t longest_nonprojective_edge(std::vector<token_index_t> &heads);
    void lift_longest(std::vector<token_index_t> &heads);
    bool is_nonprojective(std::vector<token_index_t> &heads);


private:
    std::vector<token_index_t> longest_edges;
};




#endif //HANSTHOLM_NONPROJ_H

