#include <iostream>
#include "learn.h"
#include "input.h"
#include <algorithm>
#include "hashtable_block.h"

using namespace std;

void train_test_parser() {
    // Read corpus
    string train_filename = "/users/anders/data/treebanks/udt_1/rungsted/en-universal-train.conll";
    string test_filename = "/users/anders/data/treebanks/udt_1/rungsted/en-universal-small.conll";
//    string filename = "/users/anders/data/treebanks/udt_1/rungsted/en-universal-small.conll";
    auto dict = CorpusDictionary {};
    auto train_sents = VwSentenceReader(train_filename, dict).read();
    auto test_sents  = VwSentenceReader(test_filename, dict).read();
    cout << "Sentences loaded\n";


/*
        for (auto token : first_sent.tokens) {
            cout << "Token (" << token.id << ") " << token.index << " with head " << token.head << " and rel " << token.label << "\n";
            cout << " got " << token.attributes.size() << " features\n";
        }
        */
    auto parser = TransitionParser<ArcEager>(dict, nivre_feature_set(), 5);
    parser.fit(train_sents);






    /*
    string filename = "/users/anders/data/treebanks/udt_1/rungsted/en-universal-dev.conll";
    auto dict = CorpusDictionary {};
    // auto sents = read_sentences(filename, dict);
    auto reader = VwSentenceReader(filename, dict);
    auto sents = reader.read();

    cout << "Read " << sents.size() << " sentences ";
    int num_tokens = std::accumulate(sents.cbegin(), sents.cend(), 0, [](int sum, Sentence sent) { return sum + sent.tokens.size(); });
    cout << "with " << num_tokens << " tokens ";
     */

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