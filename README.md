# Hanstholm - dependency parser with an open feature model


## Building

Checkout the project. In the top-level directory, issue:

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

Again, we go by example. 

```
S0:w ++ S0:p
S0:w ++ S0:p
S0:w
S0:p
N0:w ++ N0:p
N0:w
N0:p
N1:w ++ N1:p
N1:w
N1:p
N2:w ++ N2:p
N2:w
N2:p

S0:w ++ S0:p ++ N0:w ++ N0:p
S0:w ++ S0:p ++ N0:w
S0:w ++ N0:w ++ N0:p
S0:w ++ S0:p ++ N0:p
S0:p ++ N0:w ++ N0:p
S0:w ++ N0:w
S0:p ++ N0:p
N0:p ++ N1:p

N0:p ++ N1:p ++ N2:p
S0:p ++ N0:p ++ N1:p
S0_head:p ++ S0:p ++ N0:p
S0:p ++ S0_left:p ++ N0:p
S0:p ++ S0_right:p ++ N0:p
S0:p ++ N0:p ++ N0_left:p
```
