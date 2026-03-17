#!/usr/bin/env python3
"""
Sudoku solution validator.
Reads answer.dat (format: block_side + side + board bytes) and validates the solution.
"""

import sys
import struct

def validate_sudoku(filename="answer.dat"):
    """
    Validate a Sudoku solution from a binary file.
    Collects ALL conflicts instead of stopping at the first one.
    
    File format:
    - 1 byte: block_size (e.g., 3 for 9x9, 4 for 16x16, 5 for 25x25, etc.)
    - 1 byte: side (size of board, e.g., 9, 16, 25, etc.)
    - side*side bytes: the board values (1 to side, 0 means empty)
    
    Returns: (is_valid, errors)
    """
    try:
        with open(filename, 'rb') as f:
            # Read block_size and side
            block_size = struct.unpack('B', f.read(1))[0]
            side = struct.unpack('B', f.read(1))[0]
            
            # Read board
            board_data = f.read(side * side)
            if len(board_data) != side * side:
                return False, [f"Expected {side*side} bytes, got {len(board_data)}"]
            
            board = list(board_data)
            
            print(f"Validating {side}x{side} Sudoku (block size: {block_size}x{block_size})")
            print(f"Block dimensions: {side // block_size} x {side // block_size} blocks\n")
            
            errors = []
            
            # Validate block_size
            if block_size * block_size != side:
                errors.append(f"Invalid block_size: {block_size}x{block_size} = {block_size*block_size}, expected {side}")
            
            # Check all cells are filled (no zeros) and valid range
            for i, val in enumerate(board):
                row, col = divmod(i, side)
                if val == 0:
                    errors.append(f"Empty cell at ({row}, {col})")
                if val < 1 or val > side:
                    errors.append(f"Invalid value {val} at ({row}, {col}), must be 1-{side}")
            
            # Collect ALL row duplicates
            row_errors = []
            for row in range(side):
                row_vals = {}
                for col in range(side):
                    val = board[row * side + col]
                    if val in row_vals:
                        row_errors.append(f"Duplicate {val} in row {row} (columns {row_vals[val]} and {col})")
                    else:
                        row_vals[val] = col
            errors.extend(row_errors)
            
            # Collect ALL column duplicates
            col_errors = []
            for col in range(side):
                col_vals = {}
                for row in range(side):
                    val = board[row * side + col]
                    if val in col_vals:
                        col_errors.append(f"Duplicate {val} in column {col} (rows {col_vals[val]} and {row})")
                    else:
                        col_vals[val] = row
            errors.extend(col_errors)
            
            # Collect ALL block duplicates
            block_errors = []
            num_blocks = side // block_size
            for block_row in range(num_blocks):
                for block_col in range(num_blocks):
                    block_vals = {}
                    for sub_row in range(block_size):
                        for sub_col in range(block_size):
                            row = block_row * block_size + sub_row
                            col = block_col * block_size + sub_col
                            val = board[row * side + col]
                            key = (row, col)
                            if val in block_vals:
                                prev_row, prev_col = block_vals[val]
                                block_errors.append(f"Duplicate {val} in block ({block_row},{block_col}) at ({prev_row},{prev_col}) and ({row},{col})")
                            else:
                                block_vals[val] = key
            errors.extend(block_errors)
            
            if not errors:
                print("✓ All rows valid (no duplicates, all values present)")
                print("✓ All columns valid (no duplicates, all values present)")
                print("✓ All blocks valid (no duplicates, all values present)")
                print("✓ All cells filled\n")
                return True, []
            else:
                return False, errors
    
    except FileNotFoundError:
        return False, [f"File '{filename}' not found"]
    except Exception as e:
        return False, [f"Error reading file: {e}"]

def print_board(filename="answer.dat"):
    """Pretty-print the board from answer.dat"""
    try:
        with open(filename, 'rb') as f:
            block_size = struct.unpack('B', f.read(1))[0]
            side = struct.unpack('B', f.read(1))[0]
            board_data = f.read(side * side)
            board = list(board_data)
            
            print(f"\n{side}x{side} Board:\n")
            for row in range(side):
                if row % block_size == 0 and row != 0:
                    print()
                row_str = ""
                for col in range(side):
                    if col % block_size == 0 and col != 0:
                        row_str += "  "
                    val = board[row * side + col]
                    row_str += f"{val:2d} " if val > 9 else f"{val:2d} "
                print(row_str)
    except Exception as e:
        print(f"Error printing board: {e}")

if __name__ == "__main__":
    # Determine input source
    if not sys.stdin.isatty():
        # Data is being piped to stdin - read binary data and validate
        import tempfile
        stdin_data = sys.stdin.buffer.read()
        
        # Write to temporary file for validation
        with tempfile.NamedTemporaryFile(delete=False, suffix='.dat') as tmp:
            tmp.write(stdin_data)
            tmp_filename = tmp.name
        
        try:
            is_valid, errors = validate_sudoku(tmp_filename)
            
            if is_valid:
                print("Solution is VALID!")
                print_board(tmp_filename)
                sys.exit(0)
            else:
                print(f"\n❌ Solution is INVALID - found {len(errors)} error(s):\n")
                for error in errors:
                    print(f"  - {error}")
                print()
                sys.exit(1)
        finally:
            import os
            os.unlink(tmp_filename)
    else:
        # Normal file argument mode
        filename = "answer.dat" if len(sys.argv) < 2 else sys.argv[1]
        
        is_valid, errors = validate_sudoku(filename)
        
        if is_valid:
            print("Solution is VALID!")
            print_board(filename)
            sys.exit(0)
        else:
            print(f"\n❌ Solution is INVALID - found {len(errors)} error(s):\n")
            for error in errors:
                print(f"  - {error}")
            print()
            sys.exit(1)
