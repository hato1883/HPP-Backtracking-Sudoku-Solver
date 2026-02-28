#!/usr/bin/python3

#
# This code is taken from https://stackoverflow.com/questions/45471152/how-to-create-a-sudoku-puzzle-in-python
# Original author Alain T. 

# Usage: python generate_board.py <Board base length 3 => 9x9 8 => 64x64 etc> <Number of empty(unassigned) squares

import sys
from random import sample

base  = int(sys.argv[1])
side  = base*base

# pattern for a baseline valid solution
def pattern(r,c): 
    return (base*(r%base)+r//base+c)%side

# randomize rows, columns and numbers (of valid base pattern)
def shuffle(s): 
    return sample(s,len(s)) 

rBase = range(base) 
rows  = [ g*base + r for g in shuffle(rBase) for r in shuffle(rBase) ] 
cols  = [ g*base + c for g in shuffle(rBase) for c in shuffle(rBase) ]
nums  = shuffle(range(1,base*base+1))

# produce board using randomized baseline pattern
board = [ [nums[pattern(r,c)] for c in cols] for r in rows ]

squares = side*side
empties = int(sys.argv[2])
for p in sample(range(squares),empties):
    board[p//side][p%side] = 0

byte_array = []

for row in board:
    for e in row:
        byte_array.append(e.to_bytes(1, "little"))
        print(e,end='')
    print('')

with open('board.dat', 'wb') as f:
    f.write(base.to_bytes(1, "little"))
    f.write(side.to_bytes(1, "little"))
    print(base,side)
    for b in byte_array:
        f.write(b)
        
