#ifndef HASH_H
#define HASH_H

// Copied from the boost library 1.55
template <class T>
inline void hash_combine(std::size_t & seed, const T & v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <class It>
inline void hash_range(std::size_t& seed, It first, It last)
{
    for(; first != last; ++first)
    {
        hash_combine(seed, *first);
    }
}

#endif