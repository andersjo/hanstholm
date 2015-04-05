#include <assert.h>
#include <memory.h>
#include <math.h>
#include <vector>
#include <iostream>

#include "hashtable_block.h"


/**
* CAVEAT: Zero cannot be used as a key.
*/

bool is_power_of_two(size_t number) {
    return (number & (number - 1)) == 0;
};

HashTableBlock::HashTableBlock(size_t initial_size, size_t value_block_size)
{
    assert(is_power_of_two(initial_size));
    // assert(value_block_size / sizeof(Cell::value_type) == 0);
    keys.resize(initial_size, 0);
    aligned_value_block_size = value_block_size + value_block_size / sizeof(Cell::value_type);
    values.resize(aligned_value_block_size * initial_size, 0);
    inserts_before_resize = static_cast<size_t>(initial_size * 0.75);
}


Cell::value_type * HashTableBlock::lookup(size_t key)
{
    size_t candidate_index = hash_key(key);
    // Forward search
    for (int i = candidate_index; i < keys.size(); i++) {
        if (keys[i] == key) return &values[aligned_value_block_size * i];
        if (keys[i] == 0) return nullptr;
    }

    // Search from the beginning
    for (int i = 0; i < candidate_index; i++) {
        if (keys[i] == key) return &values[aligned_value_block_size * i];
        if (keys[i] == 0) return nullptr;
    }

    return nullptr;
};


Cell::value_type * HashTableBlock::insert(size_t key)
{
    size_t candidate_index = hash_key(key);
    // Forward search
    for (size_t i = candidate_index; i < keys.size(); i++) {
        if (keys[i] == key) return &values[aligned_value_block_size * i];
        if (keys[i] == 0) return insert_at(key, i);
    }

    // Search from the beginning
    for (size_t i = 0; i < candidate_index; i++) {
        if (keys[i] == key) return &values[aligned_value_block_size * i];
        if (keys[i] == 0) return insert_at(key, i);
    }

    throw std::out_of_range("No empty slot for key found");

}


Cell::value_type * HashTableBlock::insert_at(size_t key, size_t index) {
    if (inserts_before_resize == 0) {
        resize(keys.size() * 2);
        return insert(key);
    } else {
        inserts_before_resize--;
        keys[index] = key;
        return &values[aligned_value_block_size * index];
    }
}

void HashTableBlock::resize(size_t new_size)
{
    std::cout << "Repopulating. Old size was " << keys.size() << ", new size will be " << new_size << "\n";
    assert(is_power_of_two(new_size));
    assert(new_size >= (0.75 * keys.size()));

    // Initialize higher capacity data structures.
    // Their names are prefixed with "old", because they will be swapped with the actual old data structures shortly
    std::vector<size_t> old_keys(new_size, 0);
    std::vector<Cell::value_type> old_values(new_size * aligned_value_block_size, 0);

    // There...
    keys.swap(old_keys);
    values.swap(old_values);
    inserts_before_resize = static_cast<size_t>(new_size * 0.75);

    // Re-insert everything.
    // std::cout << "Re-inserting ";
    for (size_t i = 0; i < old_keys.size(); i++) {
        if (old_keys[i] != 0) {
            // std::cout << old_keys[i] << " ";
            auto * new_vals = insert(old_keys[i]);
            auto * old_vals = &old_values[i * aligned_value_block_size];
            memcpy(new_vals, old_vals, aligned_value_block_size * sizeof(Cell::value_type));
        }
    }
    std::cout << "Repopulating done...\n";
}

