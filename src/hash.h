#ifndef HASH_H
#define HASH_H

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <functional>
#include <iostream>

inline uint64_t integerHash(uint64_t k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccd;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53;
    k ^= k >> 33;
    return k;
}

// Copied from the boost library 1.55
template <class T>
inline void hash_combine(std::size_t & seed, const T & v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <>
inline void hash_combine<size_t>(std::size_t & seed, const size_t & v)
{
    seed ^= integerHash(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}


template <class It>
inline void hash_range(std::size_t& seed, It first, It last)
{
    for(; first != last; ++first)
    {
        hash_combine(seed, *first);
    }
}

inline uint64_t upper_power_of_two(uint64_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}



#endif