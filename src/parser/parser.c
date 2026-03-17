
#include "parser/parser.h"

#include "data/board.h"
#include "utils/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void parse_header(FILE* file, unsigned char* base, unsigned char* side);
static void parse_board_cells(FILE* file, hpp_board* board);

hpp_board* parse_file(const char* file_name)
{
    FILE* file = fopen(file_name, "rb");
    if (file == NULL)
    {
        LOG_ERROR("Not able to open the file: \"%s\"", file_name);
        exit(EXIT_FAILURE);
    }

    unsigned char base = 0;
    unsigned char side = 0;

    parse_header(file, &base, &side);

    if ((size_t)base * (size_t)base != side)
    {
        LOG_ERROR("Inconsistent board header: base=%hhu side=%hhu", base, side);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // This board will act as a starting point,
    // any created board must match filled elements with this one
    hpp_board* board = create_board(side);

    parse_board_cells(file, board);

    fclose(file);
    return board;
}

/**
 * Read and validate the board header (base and side values)
 *
 * @param file File pointer to read from
 * @param base Output parameter for the base value
 * @param side Output parameter for the side value
 */
static void parse_header(FILE* file, unsigned char* base, unsigned char* side)
{
    if (fread(base, sizeof(unsigned char), 1, file) != 1 ||
        fread(side, sizeof(unsigned char), 1, file) != 1)
    {
        LOG_ERROR("Failed to read header bytes.");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    else
    {
        LOG_INFO("Read value %hhu and %hhu", *base, *side);
    }

    if (*base == 0)
    {
        LOG_ERROR("Invalid value for size: %hhu", *base);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    if (*side == 0)
    {
        LOG_ERROR("Invalid value for side count: %hhu", *side);
        fclose(file);
        exit(EXIT_FAILURE);
    }
}

/**
 * Read board cells from file and populate the board
 *
 * @param file File pointer to read from
 * @param board Board structure to populate
 */
static void parse_board_cells(FILE* file, hpp_board* board)
{
    for (size_t i = 0; i < board->cell_count; i++)
    {
        unsigned char value;
        if (fread(&value, sizeof(unsigned char), 1, file) != 1)
        {
            LOG_ERROR("Unexpected end of file while reading board.");
            fclose(file);
            exit(EXIT_FAILURE);
        }

        board->cells[i] = value;
    }
}

/**
 * @brief Write a board to a FILE* stream (can be stdout or a file).
 *
 * Format: block_size (1 byte) + side (1 byte) + board bytes.
 *
 * @param file   Output stream.
 * @param board  The board to write.
 * @param silent If true, suppress error logging.
 * @return 0 on success, -1 on failure.
 */
int write_board_to_stream(FILE* file, const hpp_board* board, int silent)
{
    if (file == NULL || board == NULL)
    {
        if (!silent)
        {
            LOG_ERROR("Invalid arguments to write_board_to_stream");
        }
        return -1;
    }
    const unsigned char block_size_byte = (unsigned char)board->block_length;
    const unsigned char side_byte       = (unsigned char)board->side_length;
    if (fwrite(&block_size_byte, sizeof(unsigned char), 1, file) != 1)
    {
        if (!silent)
        {
            LOG_ERROR("Failed to write block_size");
        }
        return -1;
    }
    if (fwrite(&side_byte, sizeof(unsigned char), 1, file) != 1)
    {
        if (!silent)
        {
            LOG_ERROR("Failed to write side");
        }
        return -1;
    }
    /* Write the entire flat cell slab in one call. */
    if (fwrite(board->cells, sizeof(hpp_cell), board->cell_count, file) != board->cell_count)
    {
        if (!silent)
        {
            LOG_ERROR("Failed to write board cells");
        }
        return -1;
    }
    return 0;
}

/**
 * Write a solved board to a binary file.
 * Format: block_size (1 byte) + side (1 byte) + board bytes
 *
 * @param filename output file path
 * @param board the solved board to write
 * @return 0 on success, -1 on failure
 */
int write_solution(const char* filename, const hpp_board* board)
{
    if (filename == NULL || board == NULL)
    {
        LOG_ERROR("Invalid arguments to write_solution");
        return -1;
    }

    FILE* file = fopen(filename, "wb");
    if (file == NULL)
    {
        LOG_ERROR("Failed to open file for writing: \"%s\"", filename);
        return -1;
    }

    // Write block_size
    unsigned char block_size = (unsigned char)board->block_length;
    if (fwrite(&block_size, sizeof(unsigned char), 1, file) != 1)
    {
        LOG_ERROR("Failed to write block_size to solution file");
        fclose(file);
        return -1;
    }

    // Write side
    unsigned char side = (unsigned char)board->side_length;
    if (fwrite(&side, sizeof(unsigned char), 1, file) != 1)
    {
        LOG_ERROR("Failed to write side to solution file");
        fclose(file);
        return -1;
    }

    // Write board cells
    for (size_t i = 0; i < board->cell_count; ++i)
    {
        unsigned char value = board->cells[i];
        if (fwrite(&value, sizeof(unsigned char), 1, file) != 1)
        {
            LOG_ERROR("Failed to write board cell at (%zu, %zu)",
                      i / board->side_length,
                      i % board->side_length);
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    LOG_INFO("Solution written to: %s", filename);
    return 0;
}
