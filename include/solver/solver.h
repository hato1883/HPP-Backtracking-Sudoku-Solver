#ifndef SOLVER_SOLVER_H
#define SOLVER_SOLVER_H

// TODO: implement this
void step(void);

// TODO: implement this
void probe(void);

// TODO: implement this Importnat to not move any number placed by the board generator
// Maybe make a first pass to find all "unlocked" cells and store the for neighbourhood creation for each tick.
// Technicaly the neighbourhood never changes if we allow swaps in a row, for all rows
void create_neighbourhood(void);

#endif