/**
 * @file data/bitvector_ops.h
 * @brief Shared inline helpers for fixed-size bitvector operations.
 */

#ifndef DATA_BITVECTOR_OPS_H
#define DATA_BITVECTOR_OPS_H

#include "data/bitvector.h"

#include <stdbool.h>

static inline void hpp_bitvector_position(size_t value, size_t* word_idx, uint8_t* bit_offset)
{
    *word_idx   = value / HPP_BITVECTOR_WORD_BITS;
    *bit_offset = (uint8_t)(value % HPP_BITVECTOR_WORD_BITS);
}

static inline void hpp_bitvector_set(hpp_bitvector_word* bitvector, size_t value)
{
    size_t  word_idx   = 0;
    uint8_t bit_offset = 0;
    hpp_bitvector_position(value, &word_idx, &bit_offset);
    bitvector[word_idx] |= (UINT64_C(1) << bit_offset);
}

static inline void hpp_bitvector_clear(hpp_bitvector_word* bitvector, size_t value)
{
    size_t  word_idx   = 0;
    uint8_t bit_offset = 0;
    hpp_bitvector_position(value, &word_idx, &bit_offset);
    bitvector[word_idx] &= ~(UINT64_C(1) << bit_offset);
}

static inline bool hpp_bitvector_test(const hpp_bitvector_word* bitvector, size_t value)
{
    size_t  word_idx   = 0;
    uint8_t bit_offset = 0;
    hpp_bitvector_position(value, &word_idx, &bit_offset);
    return (bitvector[word_idx] & (UINT64_C(1) << bit_offset)) != 0;
}

#endif /* DATA_BITVECTOR_OPS_H */