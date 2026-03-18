/**
 * @file data/bitvector.h
 * @brief Shared 256-bit bitvector data model.
 *
 * The solver tracks values in the fixed domain `0..255`, represented as
 * four contiguous `uint64_t` words.
 */

#ifndef DATA_BITVECTOR_H
#define DATA_BITVECTOR_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Storage word used by all solver bitvectors. */
typedef uint64_t hpp_bitvector_word;

enum
{
    HPP_BITVECTOR_VALUE_COUNT = UINT8_MAX + 1U,
    HPP_BITVECTOR_WORD_BITS   = sizeof(hpp_bitvector_word) * CHAR_BIT,
    HPP_BITVECTOR_WORD_COUNT  = HPP_BITVECTOR_VALUE_COUNT / HPP_BITVECTOR_WORD_BITS,
    HPP_BITVECTOR_BYTES       = HPP_BITVECTOR_WORD_COUNT * sizeof(hpp_bitvector_word),
};

_Static_assert((UINT8_MAX + 1U) % (sizeof(hpp_bitvector_word) * CHAR_BIT) == 0,
               "Bitvector value domain must align to uint64_t words.");

#endif /* DATA_BITVECTOR_H */