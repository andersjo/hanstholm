#ifndef HASHTABLE_BLOCK_H
#define HASHTABLE_BLOCK_H

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <list>
#include "hash.h"

//----------------------------------------------
//  HashTable
//
//  Maps pointer-sized integers to pointer-sized integers.
//  Uses open addressing with linear probing.
//  In the m_cells array, key = 0 is reserved to indicate an unused cell.
//  Actual value for key 0 (if any) is stored in m_zeroCell.
//  The hash table automatically doubles in size when it becomes 75% full.
//  The hash table never shrinks in size, even after Clear(), unless you explicitly call Compact().
//----------------------------------------------



/*
struct Cell_
{
    size_t key;
    size_t value;
};
*/

struct Cell
{
    using value_type = float;
    size_t key;
    value_type * value;
};


class HashTableBlock
{
public:
    HashTableBlock() = default;
    HashTableBlock(size_t initial_size, size_t num_values_per_key);

    // Basic operations
    Cell::value_type *lookup(size_t key);
    Cell::value_type *insert(size_t key);

    // Keys temporarily made public
    std::vector<size_t> keys;

private:
    inline size_t hash_key(size_t key) {
        return (integerHash(key)) & (keys.size() - 1);
    }
    Cell::value_type *insert_at(size_t key, size_t index);
    std::vector<Cell::value_type > values;
    size_t inserts_before_resize;
    size_t aligned_value_block_size;

    void resize(size_t desiredSize);
};

/*
inline uint32_t upper_power_of_two(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}
*/



/*
// from code.google.com/p/smhasher/wiki/MurmurHash3
inline uint32_t integerHash(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}
*/

// from code.google.com/p/smhasher/wiki/MurmurHash3




#endif