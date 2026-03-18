/**
 * @file data/bitvector_ops.h
 * @brief Shared inline helpers for fixed-size bitvector operations.
 */

#ifndef DATA_BITVECTOR_OPS_H
#define DATA_BITVECTOR_OPS_H

#include "data/bitvector.h"

#include <stdbool.h>

/**
 * @brief Convert a logical value index to word/bit coordinates.
 *
 * @note This helper centralizes index arithmetic so all bitvector operations
 *       use identical mapping semantics.
 * @pre `word_idx != NULL`, `bit_offset != NULL`, and `value` is in range.
 * @post Outputs contain the exact storage position for `value`.
 *
 * @param value Logical bit index in the global candidate/value domain.
 * @param word_idx Output word index inside the bitvector array.
 * @param bit_offset Output bit offset inside the selected word.
 */
static inline void hpp_bitvector_position(size_t value, size_t* word_idx, uint8_t* bit_offset)
{
    *word_idx   = value / HPP_BITVECTOR_WORD_BITS;
    *bit_offset = (uint8_t)(value % HPP_BITVECTOR_WORD_BITS);
}

/**
 * @brief Set one bit in a fixed-size bitvector.
 *
 * @note Performs no bounds checks; callers validate `value` beforehand in
 *       hot paths to keep this helper branchless.
 * @pre `bitvector != NULL` and `value` is within representable range.
 * @post Bit corresponding to `value` is set to `1`.
 *
 * @param bitvector Target bitvector storage.
 * @param value Logical bit index to set.
 */
static inline void hpp_bitvector_set(hpp_bitvector_word* bitvector, size_t value)
{
    size_t  word_idx   = 0;
    uint8_t bit_offset = 0;
    hpp_bitvector_position(value, &word_idx, &bit_offset);
    bitvector[word_idx] |= (UINT64_C(1) << bit_offset);
}

/**
 * @brief Clear one bit in a fixed-size bitvector.
 *
 * @note Performs no bounds checks; callers validate `value` beforehand in
 *       hot paths to keep this helper branchless.
 * @pre `bitvector != NULL` and `value` is within representable range.
 * @post Bit corresponding to `value` is set to `0`.
 *
 * @param bitvector Target bitvector storage.
 * @param value Logical bit index to clear.
 */
static inline void hpp_bitvector_clear(hpp_bitvector_word* bitvector, size_t value)
{
    size_t  word_idx   = 0;
    uint8_t bit_offset = 0;
    hpp_bitvector_position(value, &word_idx, &bit_offset);
    bitvector[word_idx] &= ~(UINT64_C(1) << bit_offset);
}

/**
 * @brief Test one bit in a fixed-size bitvector.
 *
 * @note Performs no bounds checks; callers validate `value` beforehand when
 *       traversing dense candidate/constraint loops.
 * @pre `bitvector != NULL` and `value` is within representable range.
 * @post Bitvector contents are unchanged.
 *
 * @param bitvector Target bitvector storage.
 * @param value Logical bit index to test.
 * @return `true` when the bit for `value` is set.
 */
static inline bool hpp_bitvector_test(const hpp_bitvector_word* bitvector, size_t value)
{
    size_t  word_idx   = 0;
    uint8_t bit_offset = 0;
    hpp_bitvector_position(value, &word_idx, &bit_offset);
    return (bitvector[word_idx] & (UINT64_C(1) << bit_offset)) != 0;
}

#endif /* DATA_BITVECTOR_OPS_H */