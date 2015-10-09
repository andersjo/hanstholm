# Hanstholm - dependency parser with an open feature model


## Building

Building hanstholm requires as working installation of  [CMAKE](http://www.cmake.org/) and [the Boost library](http://www.boost.org/).

Once the requirements are installed, checkout the project. In the top-level directory, issue:

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

This builds the `hanstholm` binary in the build directory. 

## Running

Example. Train the parser on `data/train.txt`, passing over the data `50` times. After training, evaluate the final model on `data/test.txt`, placing the predictions in `out/test_pred.tsv`. The feature model is specified by `nivre.txt`.

```
./hanstholm \
--passes 50 \
--template nivre.txt \
--data data/train.txt \
--eval data/test.txt \
--predictions out/test_pred.tsv
```

## Data format

The input file format borrows the concept of feature namespaces and most of the syntax from Vowpal Wabbit. Here is an example of the input: 

```
1-compmod '1-0|w ms. |p NOUN
2-nsubj '1-1|w haag |p NOUN
-1-ROOT '1-2|w plays |p VERB
2-dobj '1-3|w elianti |p NOUN
2-p '1-4|w . |p .
```

Breaking down the first line, 

```
1-compmod '1-0|w ms. |p NOUN
```

The label `1-compmod` tells us that the head of the first token (index 0) is `1` with the relation `compmod`. The part after the label is the identifier, which can be any string. The identifier is deliminated in one end by the `'` symbol and in the other end by the bar, `|`. The rest are feature definitions. A bar indicates the beginning of a new namespace and the string touching the bar is the namespace identifier. Thus we have two features: `w^ms.` and `p^NOUN`. 

No value is given to the features in the example, so they assume the default of `1.0`. To specify a feature value, place it the end of the feature name, separated by a colon. E.g. `p^NOUN:2.5`. Aside from this use, colons cannot be used as part of feature names. 

## Feature specification 

Transition-based parsing has a classification problem at its core, which is to decide which transition to perform next. During parsing, features are extracted based on the current configuration of the stack **S** and buffer **N**, and the resulting feature vector is scored by taking the dot product with a weight vector. 

In Hanstholm, the features are specified in a plain-text template file. Each line in the file describes a single feature, which may be a conjunction of several *feature primitives*. Again, we go by example. The line,

```
S0:w ++ S0:p
```

is a conjumction of the primitives `S0:w` and `S0:p`, with `++` being the conjunction operator. I.e. each distinct combination of **w**ord and **p**art-of-speech tags for the token at the top of the stack (position `S0`) becomes a separate parameter in the model. The numeric value of the feature conjunction is the product of the feature primitives. 

Note that the names `w` and `p` have no special meaning; they simply refer to the namespaces of the input file. In general, a namespace may contain more than one feature. For instance, a part-of-speech namespace could include the top-*k* tags, weighted by their probability. Another use-case is word embeddings, which have multiple dimensions. In those cases, the feature template expands to the Cartesian product of the namespaces. Concretely, assume that the token represented by the input line below is in the `S0` position.

```
|w tail |p NOUN:0.7 VERB:0.3
```

Then the `S0:w ++ S0:p` conjunction feature template would generate the following two features:

```
tail+NOUN:0.7 tail+VERB:0.3
```
