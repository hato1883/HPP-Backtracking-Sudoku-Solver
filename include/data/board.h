#ifndef DATA_BOARD_H
#define DATA_BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum BoardValues
{
    BOARD_CELL_EMPTY = 0, /** Reserves the value 0 as the marker for a unasigned value */
};

/** Cell support values 0 to 255, this limits board size to 15 x 15 = 225 */
typedef uint8_t hpp_cell;

typedef struct Board
{
    size_t side_length;  /** side length of the board, a normal 9x9 board has side_length = 9 */
    size_t block_length; /** block length of the board, a normal 9x9 board has block_length = 3 */
    size_t cell_count;   /** number of cells in the board, a normal 9x9 board has cell_count = 81 */
    hpp_cell* cells;     /** Store all values in the board as a 1d array */
} hpp_board;

hpp_board* create_board(size_t size);
hpp_board* hpp_board_clone(const hpp_board* source);
bool       hpp_board_copy(hpp_board* destination, const hpp_board* source);
void       destroy_board(hpp_board** board);

void print_board(const hpp_board* board);

#endif