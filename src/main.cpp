#include <iostream>
#include "learn.h"
#include "input.h"
#include "output.h"
#include <algorithm>
#include "hashtable_block.h"


using namespace std;

void train_test_parser() {
    // Read corpus
    string train_filename = "/users/anders/data/treebanks/udt_1/rungsted/en-universal-small.conll";
    string test_filename = "/users/anders/data/treebanks/udt_1/rungsted/en-universal-small.conll";
//    string filename = "/users/anders/data/treebanks/udt_1/rungsted/en-universal-small.conll";
    auto dict = CorpusDictionary {};
    auto train_sents = VwSentenceReader(train_filename, dict).read();
    auto test_sents  = VwSentenceReader(test_filename, dict).read();
    cout << "Sentences loaded\n";

    auto feature_set = nivre_feature_set();
    auto parser = TransitionParser<ArcEager>(dict, feature_set, 5);
    parser.fit(train_sents);

    ParseResult parsed_sentence;

    for (const auto & sent : test_sents) {
        cout << "Parsing test sentence\n";
        parsed_sentence = parser.parse(sent);
        output_parse_result(cout, sent, parsed_sentence);
    }




}

void test_hashtable_block() {
    size_t block_size = 10;
    HashTableBlock table(8, block_size);

    for (int i = 1; i < 16; i++) {
        float * vals = table.insert(i);
        fill_n(vals, block_size, static_cast<float>(i));
    }

    cout << "Found ";
    for (int i = 1; i < 50; i++) {
        if (table.lookup(i)) {
            float * vals = table.lookup(i);
            cout << i << "-" << vals[5] << " ";
        }
    }
    cout << "\n";



}

int main() {
    // test_hashtable_block();
    train_test_parser();

    return 0;
}