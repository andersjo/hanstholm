#include <iostream>
#include "learn.h"
#include "parse.h"
#include "input.h"

using namespace std;

    int main() {
        // Read corpus
        string filename = "/users/anders/data/treebanks/udt_1/rungsted/en-universal-dev.conll";
//    string filename = "/users/anders/data/treebanks/udt_1/rungsted/en-universal-small.conll";
        auto dict = CorpusDictionary {};
        auto reader = VwSentenceReader(filename, dict);
        auto sents = reader.read();

        auto first_sent = sents[0];
        for (auto token : first_sent.tokens) {
            cout << "Token (" << token.id << ") " << token.index << " with head " << token.head << " and rel " << token.relation << "\n";
            cout << " got " << token.attributes.size() << " features\n";
        }
        auto parser = TransitionParser<ArcEager>(dict, nivre_feature_set(), 5);
        parser.fit(sents);
        /*
        string filename = "/users/anders/data/treebanks/udt_1/rungsted/en-universal-dev.conll";
        auto dict = CorpusDictionary {};
        // auto sents = read_sentences(filename, dict);
        auto reader = VwSentenceReader(filename, dict);
        auto sents = reader.read();

        cout << "Read " << sents.size() << " sentences ";
        int num_tokens = std::accumulate(sents.cbegin(), sents.cend(), 0, [](int sum, CSentence sent) { return sum + sent.tokens.size(); });
        cout << "with " << num_tokens << " tokens ";
         */


        return 0;
    }