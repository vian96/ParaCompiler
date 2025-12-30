#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using uint128_t = __uint128_t;

static inline unsigned get_num_words(uint32_t bit_width) {
    return (bit_width + 63) / 64;
}

static bool is_negative(const uint64_t* buffer, uint32_t bit_width) {
    if (bit_width == 0) return false;
    unsigned word_idx = (bit_width - 1) / 64;
    unsigned bit_idx = (bit_width - 1) % 64;
    return (buffer[word_idx] >> bit_idx) & 1;
}

static void clear_unused_bits(uint64_t* buffer, uint32_t bit_width) {
    unsigned num_words = get_num_words(bit_width);
    unsigned extra_bits = bit_width % 64;
    if (extra_bits != 0) {
        uint64_t mask = ~(~0ULL << extra_bits);
        buffer[num_words - 1] &= mask;
    }
}

static void negate_buffer(uint64_t* buffer, unsigned num_words) {
    uint64_t carry = 1;
    for (unsigned i = 0; i < num_words; ++i) {
        buffer[i] = ~buffer[i];
        uint128_t res = (uint128_t)buffer[i] + carry;
        buffer[i] = (uint64_t)res;
        carry = (uint64_t)(res >> 64);
    }
}

static uint32_t div_mod_10(uint64_t* buffer, unsigned num_words) {
    uint64_t remainder = 0;
    for (int i = num_words - 1; i >= 0; --i) {
        uint128_t cur = ((uint128_t)remainder << 64) | buffer[i];
        buffer[i] = (uint64_t)(cur / 10);
        remainder = (uint64_t)(cur % 10);
    }
    return (uint32_t)remainder;
}

static void mul_10_add(uint64_t* buffer, unsigned num_words, uint32_t digit) {
    uint64_t carry = digit;
    for (unsigned i = 0; i < num_words; ++i) {
        uint128_t res = (uint128_t)buffer[i] * 10 + carry;
        buffer[i] = (uint64_t)res;
        carry = (uint64_t)(res >> 64);
    }
}

static bool is_zero(const uint64_t* buffer, unsigned num_words) {
    for (unsigned i = 0; i < num_words; ++i) {
        if (buffer[i] != 0) return false;
    }
    return true;
}

extern "C" void pcl_output_int__(uint64_t *buffer, uint32_t bit_width) {
    if (bit_width == 0) return;

    unsigned num_words = get_num_words(bit_width);
    std::vector<uint64_t> val(buffer, buffer + num_words);

    clear_unused_bits(val.data(), bit_width);

    bool negative = is_negative(val.data(), bit_width);
    if (negative) {
        negate_buffer(val.data(), num_words);
        clear_unused_bits(val.data(), bit_width);
    }

    std::string s;
    if (is_zero(val.data(), num_words)) {
        s = "0";
    } else {
        while (!is_zero(val.data(), num_words)) {
            uint32_t rem = div_mod_10(val.data(), num_words);
            s.push_back(char('0' + rem));
        }
    }

    if (negative) {
        s.push_back('-');
    }

    std::reverse(s.begin(), s.end());
    std::cout << s << std::endl;
}

extern "C" void pcl_input_int__(uint64_t *buffer, uint32_t bit_width) {
    if (bit_width == 0) return;

    std::string token;
    if (!(std::cin >> token)) return;

    unsigned num_words = get_num_words(bit_width);
    std::memset(buffer, 0, num_words * sizeof(uint64_t));

    if (token.empty()) return;

    bool negative = false;
    size_t idx = 0;
    if (token[0] == '-') {
        negative = true;
        idx = 1;
    } else if (token[0] == '+') {
        idx = 1;
    }

    for (; idx < token.size(); ++idx) {
        if (!std::isdigit(token[idx])) break;
        uint32_t digit = token[idx] - '0';
        mul_10_add(buffer, num_words, digit);
    }

    if (negative) {
        negate_buffer(buffer, num_words);
    }

    clear_unused_bits(buffer, bit_width);
}
