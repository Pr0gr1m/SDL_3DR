#ifndef SDL1_INT2_H
#define SDL1_INT2_H

#include "Vector.h"
#include <functional>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_set>

//TODO: I need to find a solution for operator< for Int3 to use std::map (Red-black tree), instead of using std::unordered_map (hash table)

/**
 * @class Int3
 * @brief A class containing 3 integer fields
 */
class Int3 {
public:
    int a, b, c;

    Int3() : a(0), b(0), c(0) {
    }

    Int3(int a, int b, int c) : a(a), b(b), c(c) {
    }

    /**
     *Returns a vector containing same fields as this Int3 instance
     *@returns A vector
     */
    Vector toVector() const;
    
    bool operator==(const Int3 &other) const {
        return (a == other.a) && (b == other.b) && (c == other.c);
    }
};

//For unordered map (hash table) i need hash function for Int3 (https://en.cppreference.com/cpp/utility/hash/operator())
///Custom hash class for Int3 class
template<>
class std::hash<Int3> {
public:
    std::size_t operator()(const Int3 &int3) const noexcept {
        std::size_t hashA = std::hash<int>{}(int3.a);
        std::size_t hashB = std::hash<int>{}(int3.b);
        std::size_t hashC = std::hash<int>{}(int3.c);

        //I dont got boost libraries so i need to combine manually, but im not sure if it cannot be just random/other equation
        return (hashA ^ (hashB << 1) ^ (hashC << 1)); // << 1 is just multiplying by 2 and ^ is just power
    }
};

#endif //SDL1_INT2_H
