#include <istream>
#include "learn.h"
#include "input.h"
#include "output.h"
#include <algorithm>
#include "hashtable_block.h"
#include "feature_set_parser.h"

#include <boost/program_options.hpp>
#include <fstream>
#include <iomanip>


using namespace std;


void train_test_parser(string data_file, string eval_file, string pred_file, string template_file, int num_passes) {
    // Read corpus
    auto dict = CorpusDictionary {};
    auto train_sents = VwSentenceReader(data_file, dict).read();
    auto test_sents  = VwSentenceReader(eval_file, dict).read();
    cerr << "Data set loaded\n";
    cerr << "\tTrain:" << train_sents.size() << " sentences\n";
    cerr << "\tTest:" << test_sents.size() << " sentences\n";

    cerr << "Using " << num_passes << " passes\n";

    // FIXME Find a good to pass ownership
    auto feature_set = read_feature_file(template_file, dict);
    cerr << "Using feature definition:\n";
    cerr << feature_set->name << "\n";


    auto strategy = ArcEager();
    auto parser = TransitionParser(dict, feature_set, strategy, num_passes);
    parser.fit(train_sents);

    ParseResult parsed_sentence;
    auto id_to_label = invert_map(dict.label_to_id);

    std::ofstream ofs;
    if (pred_file.size() > 0) {
        ofs.open(pred_file, std::ofstream::trunc);
        if (!ofs.good())
            throw std::runtime_error("Could not open prediction file " + pred_file + " for writing");
    } else {
        // FIXME does not work on windows
        ofs.open("/dev/null");
    }

    bool first_sent = true;
    ParseScore parse_score {};
    for (const auto & sent : test_sents) {
        if (first_sent)
            first_sent = false;
        else
            ofs << "\n";

        parsed_sentence = parser.parse(sent);
        output_parse_result(ofs, sent, parsed_sentence, id_to_label);
        sent.score(parsed_sentence, parse_score);
    }

    cerr << "Test set results (" << test_sents.size() << " sentences" << ")\n";
    cerr << "   UAS: " << parse_score.num_correct_unlabeled << "/" << parse_score.num_total;
    cerr << " = "  << (parse_score.uas() * 100) << "\n";
    cerr << "   LAS: " << parse_score.num_correct_labeled << "/" << parse_score.num_total;
    cerr << " = " << (parse_score.las() * 100) << "\n";


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

void test_feature_set_parser() {

// S0:p + N0:p + ( S0:z @ N0:i )
// N0:p + f(S0:p, x)
// head ( N0 ) :p

    // "S0:p + N0:p + (S0:z @ N0:i)"
    // "N0:p + f(S0:p, x)"
    // auto tokens = infix_to_prefix(tokenize_line("N0:p + S0:x"));
    // auto tokens = infix_to_prefix(tokenize_line("S0:p + N0:p + (S0:z @ N0:i)"));
    // auto tokens = infix_to_prefix(tokenize_line("f(S0:z, N0:i + X:x)"));

    try {
        auto dict = CorpusDictionary {};
        auto prefix_tokenized = tokenize_line("N0:p ++ (S0:w ++ S0:p)", dict);
        auto tokens = infix_to_prefix(prefix_tokenized);

        for (auto & token : tokens)
            cout <<  "'" << token->content << "' ";
        cout << "\n";

        auto combined = make_feature_combiner(tokens);
        cout << "Combined: " << combined->name << "\n";

        read_feature_file("/users/anders/code/hanstholm/src/nivre_small.txt", dict);

    } catch (std::runtime_error * e) {
        std::cerr << "Exception: " << e->what() << "\n" << endl;
    }

    // auto combined = make_feature_combiner(tokens);
    // cout << "\nCombined " << combined.name << "\n";

    // tokenize_line("N0:p + f(S0:p, x)");
}

namespace po = boost::program_options;

void print_usage(po::options_description desc) {
    cout << "Hanstholm parser\n";
    cout << desc;

}

int main(int argc, const char* argv[]) {

    try {
        string data_file;
        string eval_file;
        string pred_file;
        string template_file;
        int num_passes = 5;

        po::options_description desc("Allowed options");
        desc.add_options()
                ("help", "produce help message")
                ("data,d", po::value<std::string>(&data_file)->required(), "input datafile")
                ("eval,e", po::value<std::string>(&eval_file)->required(), "evaluation file")
                ("template", po::value<std::string>(&template_file)->required(), "template file (e.g. nivre.txt)")
                ("passes", po::value<int>(&num_passes), "number of passes over the training set")
                ("predictions,p", po::value<string>(&pred_file), "write predictions to this file")
                ("feature_parser", "test feature parser")
                ;

        // What options to support
        // - Input file
        // - Test file
        // - Number of rounds
        // - Average or not
        // - Ignore namespace
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.size() == 0 || vm.count("help")) {
            print_usage(desc);
            return 0;
        }

        if (vm.count("feature_parser")) {
            // test_feature_set_parser();
        } else {
            po::notify(vm);

            // Find better way to pass parameters into the program
            train_test_parser(data_file, eval_file, pred_file, template_file, num_passes);
        }


    } catch (input_parse_error *e) {
        cerr << "Error while parsing input file: " << e->what() << "\n";
        return 1;

    } catch(exception  *e) {
        cerr << "error: " << e->what() << "\n";
        return 1;
    }

    return 0;
}

