#ifndef PARSER_PARSER_H
#define PARSER_PARSER_H

#include "solver/sudoku.h"

enum
{
    BUFFER_LIMIT = 255,
    NUMBER_BASE  = 10,
};

hpp_board* parse_file(char* file_name);

/**
 * Write a solved board to a binary file.
 * Format: block_size (1 byte) + side (1 byte) + newline + board bytes
 *
 * @param filename output file path (typically "answer.dat")
 * @param board the solved board to write
 * @return 0 on success, -1 on failure
 */
int write_solution(const char* filename, const hpp_board* board);

#endif
