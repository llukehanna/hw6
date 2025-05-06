#ifndef HASH_H
#define HASH_H

#include <iostream>
#include <cmath>
#include <random>
#include <chrono>
#include <cctype>    // for std::tolower, std::isalpha, std::isdigit

typedef std::size_t HASH_INDEX_T;

struct MyStringHash {
    // debug defaults to the preset rValues
    HASH_INDEX_T rValues[5] { 983132572, 1468777056, 552714139, 984953261, 261934300 };

    MyStringHash(bool debug = true)
    {
        if (!debug) {
            generateRValues();
        }
    }

    // hash entry point
    HASH_INDEX_T operator()(const std::string& k) const
    {
        // break k into up to 5 chunks of 6 chars, right-aligned
        unsigned long long w[5] = {0, 0, 0, 0, 0};
        const size_t len = k.size();

        for (int group = 0; group < 5; ++group) {
            unsigned long long val = 0;
            // compute window [start, end) for this group
            long long end    = static_cast<long long>(len) - group * 6;
            long long start  = end - 6;
            if (end > 0) {
                if (start < 0) start = 0;
                for (long long i = start; i < end; ++i) {
                    val = val * 36 + letterDigitToNumber(k[i]);
                }
            }
            // store into w[0..4] so that w[4] is the last-6-chars chunk
            w[4 - group] = val;
        }

        // combine with rValues
        unsigned long long h = 0;
        for (int i = 0; i < 5; ++i) {
            h += static_cast<unsigned long long>(rValues[i]) * w[i];
        }
        return h;
    }

    // map a–z → 0–25 and 0–9 → 26–35 (case-insensitive)
    HASH_INDEX_T letterDigitToNumber(char c) const
    {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalpha(uc)) {
            return std::tolower(uc) - 'a';
        } else if (std::isdigit(uc)) {
            return 26 + (uc - '0');
        }
        return 0;  // should never happen under the problem constraints
    }

    // generate 5 random rValues at instantiation
    void generateRValues()
    {
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::mt19937 generator(seed);
        for (int i = 0; i < 5; ++i) {
            rValues[i] = generator();
        }
    }
};

#endif
