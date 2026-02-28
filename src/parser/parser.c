
#include "parser/parser.h"

#include "solver/sudoku.h"
#include "utils/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

hpp_board* parse_file(char* file_name)
{
    FILE* file = fopen(file_name, "rb");
    if (file == NULL)
    {
        LOG_ERROR("Not able to open the file: \"%s\"", file_name);
        exit(EXIT_FAILURE);
    }

    unsigned char base = 0;
    unsigned char side = 0;

    if (fread(&base, sizeof(unsigned char), 1, file) != 1 ||
        fread(&side, sizeof(unsigned char), 1, file) != 1)
    {
        LOG_ERROR("Failed to read header bytes.");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    else
    {
        LOG_INFO("Read value %hhu and %hhu", base, side);
    }

    if (base == 0)
    {
        LOG_ERROR("Invalid value for size: %hhu", base);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    if (side == 0)
    {
        LOG_ERROR("Invalid value for side count: %hhu", side);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    hpp_board* board = create_board(side);

    // This board will act as a starting point,
    // any created board must match filled elements with this one,
    size_t linear = 0;
    /*
        size_t word_index = 0;

        uint64_t current_word = 0;
        unsigned bit_pos = 0;
    */

    for (size_t row = 0; row < side; row++)
    {
        for (size_t col = 0; col < side; col++)
        {
            unsigned char value;
            if (fread(&value, sizeof(unsigned char), 1, file) != 1)
            {
                LOG_ERROR("Unexpected end of file while reading board.");
                fclose(file);
                exit(EXIT_FAILURE);
            }

            board->cells[row][col] = value;

            // Branchless update of single bit
            hpp_bitmask bit = (uint64_t)(value > 0);
            set_bit_linear(board->fixed, bit, linear);

            /*
            // Or brnaching update of 64 bits:
            current_word |= (uint64_t)(value > 0) << bit_pos;
            bit_pos++;

            if (bit_pos == 64)
            {
                bulk_set_mask(board->masks, current_word, word_index);
                current_word = 0;
                bit_pos = 0;
            }
            */
            linear++;
        }
    }

    /*
    // Flush remainder
    if (bit_pos != 0)
    {
        bulk_set_mask(board->masks, current_word, word_index);
    }
    */
    fclose(file);
    return board;
}
