#include "parser/parser.h"
#include "solver/sudoku.h"
#include "utils/logger.h"
#include "utils/timing.h"

#include <stdlib.h>

int main()
{
    logger_set_level(LOG_LEVEL_INFO);
    hpp_timer timer;
    timer_start(&timer);

    hpp_board* initial_board = parse_file("board.dat");
    print_board(initial_board);

    destroy_board(&initial_board);

    timer_stop(&timer);
    LOG_INFO("code ran in %2.4lf secounds\n", timer_s(&timer));
    return 0;
}