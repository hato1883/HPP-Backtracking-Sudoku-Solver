
#include "solver/sudoku.h"

#include <assert.h>
#include <stdlib.h>

Board* create_board(size_t size)
{
    assert(size > 0 && "Board Size must be larger than 0!");

    Board* new_board = (Board*)malloc(sizeof(Board) * 1);
    new_board->size  = size;
    for (size_t row = 0; row < size; row++)
    {
        new_board->board = (Number**)malloc(sizeof(Number*) * 1);
        for (size_t col = 0; col < size; col++)
        {
            new_board->board[row] = (Number*)malloc(sizeof(Number) * 1);
        }
    }
    return new_board;
}

void destroy_board(Board **board)
{
    assert(board != NULL && "Invalid refrence to Board!");
    assert(*board != NULL && "Board is already destoyed!");

    Board *to_destroy = *board;
    size_t size = to_destroy->size;

    for (size_t row = 0; row < size; row++)
    {
        free(to_destroy->board[row]);
    }
    free((void *)to_destroy->board);
    free(to_destroy);

    *board = NULL;
}