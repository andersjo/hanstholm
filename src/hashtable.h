#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stddef.h>
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

template <typename Cell>
class HashTable
{
public:

private:
    Cell* m_cells;
    size_t m_arraySize;
    size_t m_population;
    bool m_zeroUsed;
    Cell m_zeroCell;
    
    void Repopulate(size_t desiredSize);

public:
    HashTable(size_t initialSize = 8);
    ~HashTable();

    // Basic operations
    Cell* Lookup(size_t key);
    Cell* Insert(size_t key);
    void Delete(Cell* cell);
    void Clear();
    void Compact();

    void Delete(size_t key)
    {
        Cell* value = Lookup(key);
        if (value)
            Delete(value);
    }
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

#include "hashtable_impl.h"

#endif