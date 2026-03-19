#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include "data/board.h"
#include "utils/logger.h"

#include <CUnit/CUnit.h>
#include <stddef.h>
#include <string.h>

static inline hpp_board* hpp_test_create_board_from_cells(const hpp_cell* cells, size_t side_length)
{
    hpp_board* board = create_board(side_length);
    if (board == NULL)
    {
        return NULL;
    }

    memcpy(board->cells, cells, board->cell_count * sizeof(*cells));
    return board;
}

static inline void hpp_test_assert_board_cells(const hpp_board* board, const hpp_cell* expected)
{
    if (board == NULL || expected == NULL)
    {
        CU_FAIL_FATAL("Board comparison received NULL input");
        return;
    }

    for (size_t idx = 0; idx < board->cell_count; ++idx)
    {
        CU_ASSERT_EQUAL(board->cells[idx], expected[idx]);
    }
}

static inline void hpp_test_disable_logging(void)
{
    logger_set_level(NONE);
}

#endif
