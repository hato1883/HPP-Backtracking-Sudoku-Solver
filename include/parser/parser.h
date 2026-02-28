#ifndef PARSER_PARSER_H
#define PARSER_PARSER_H

#include "solver/sudoku.h"

enum
{
    BUFFER_LIMIT = 255,
    NUMBER_BASE  = 10,
};

hpp_board* parse_file(char* file_name);

#endif