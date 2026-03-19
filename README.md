# HPP Backtracking Sudoku Solver

High-performance Sudoku solver in C with backtracking, constraint propagation, and OpenMP parallel search.

## Requirements

- GCC 11+ (C17 + OpenMP support)
- make
- Python 3 (for helper scripts)
- CUnit (for unit tests)

Typical Ubuntu/Debian packages:

```bash
sudo apt-get update
sudo apt-get install -y gcc make python3 libcunit1-dev libgomp1
```

## Build

```bash
# Release build with GCC
make
# or
make release

# Debug build with GCC (sanitizers + debug symbols)
make debug
# or
make DEBUG=1
```

### Binary paths

- Release: `build/release/bin/sudoku-solver`
- Debug: `build/debug/bin/sudoku-solver`

## Run the solver

By default the solver reads `board.dat` and writes `answer.dat`.

```bash
# Solve board.dat -> answer.dat
./build/release/bin/sudoku-solver

# Solve a specific puzzle file
./build/release/bin/sudoku-solver benchmarks/scenarios/board_3x3_medium_20_1.dat

# Set thread count
./build/release/bin/sudoku-solver --threads 8 benchmarks/scenarios/board_3x3_medium_20_1.dat

# Benchmark mode (prints JSON timing summary)
./build/release/bin/sudoku-solver --benchmark benchmarks/scenarios/board_3x3_medium_20_1.dat

# Log moves to a file
./build/release/bin/sudoku-solver --moves-log moves.log benchmarks/scenarios/board_3x3_medium_20_1.dat
```

Supported CLI flags from `src/main.c`:

- `--ui` (Not implemented)
- `--benchmark`
- `--moves-log <file>` (Not implemented)
- `--threads <n>` or `--threads=<n>`
- positional input file (defaults to `board.dat`)

## Board format (`.dat`)

Binary format used by generator, solver, and validator:

- byte 0: block size (base)
- byte 1: side length (`base * base`)
- remaining bytes: `side * side` cell values (`0` means empty)

Example:

- base `3` -> side `9` -> 81 cells
- base `6` -> side `36` -> 1296 cells

## Generate boards

`generate_board.py` creates a random puzzle and writes `board.dat`.

```bash
python3 generate_board.py <base> <empty_cells>
```

Examples:

```bash
# 9x9 board, 50 empty cells
python3 generate_board.py 3 50

# 36x36 board, 700 empty cells
python3 generate_board.py 6 700
```

## Validate solutions

`validate_solution.py` validates a solution file. It defaults to `answer.dat` if no argument is provided.

```bash
# Validate answer.dat
python3 validate_solution.py

# Validate a specific file
python3 validate_solution.py answer.dat
```

The script exits with `0` for valid solutions and `1` for invalid solutions.

## Tests

```bash
# Build and run all tests
make test

# Verbose tests
make test-verbose

# Clean test artifacts only
make test-clean
```

## Benchmarking

The benchmark harness is `benchmark.py`.

It:

- uses scenarios from `benchmarks/scenarios`
- runs each scenario up to 20 times
- validates successful outputs using `validate_solution.py` logic
- stores results in `benchmarks/<tag-or-short-commit>/results.json`
- resumes from existing `results.json` unless restarted

### `benchmark.py` CLI (actual)

```bash
python3 benchmark.py [--no-skip] [--consecutive-fails N] [--continue-on-invalid] [--restart]
```

Options:

- `--no-skip`
  Disable early forfeit; do not skip larger boards after repeated failures.
- `--consecutive-fails N`
  Number of consecutive failures (same base size) before skipping the rest of that size. Default: `2`.
- `--continue-on-invalid`
  If a validation failure happens, mark that scenario invalid and continue instead of aborting the full run.
- `--restart`
  Ignore existing `results.json` for the current commit/tag and start a fresh run.

Examples:

```bash
# Normal run (resume if results already exist)
python3 benchmark.py

# Force fresh benchmark for current commit/tag
python3 benchmark.py --restart

# Disable skip logic entirely
python3 benchmark.py --no-skip

# Be more strict on skips
python3 benchmark.py --consecutive-fails 1

# Continue even if one scenario produces invalid output
python3 benchmark.py --continue-on-invalid
```

Note: the benchmark script currently invokes the solver with `--threads 12` internally.

### Scenarios

Scenario files live in `benchmarks/scenarios/` and follow naming like:

`board_<base>x<base>_<difficulty>_<empty_cells>_<id>.dat`

Examples:

- `board_3x3_veryhard_56_1.dat` (9x9 board)
- `board_6x6_veryhard_777_1.dat` (36x36 board)
- `board_15x15_hard_20250_1.dat` (225x225 board)

## Compare benchmark runs

Use `benchmark_compare.py` to compare two benchmark result directories.

### `benchmark_compare.py` CLI (actual)

```bash
python3 benchmark_compare.py <db1> <db2> [--no-warn-missing]
```

Arguments:

- `<db1>`: first benchmark directory (must contain `results.json`)
- `<db2>`: second benchmark directory (must contain `results.json`)

Option:

- `--no-warn-missing`
  Suppress warnings when tests appear in only one database.

Examples:

```bash
python3 benchmark_compare.py benchmarks/v1.3.1 benchmarks/v1.4.1
python3 benchmark_compare.py benchmarks/09567c9 benchmarks/e42b2c0
python3 benchmark_compare.py benchmarks/v1.3.1 benchmarks/v1.4.1 --no-warn-missing
```

Scoring notes:

- lower weighted time is better
- missing tests are treated as timeout in scoring
- default timeout assumption for missing tests is 60s

## Useful Make targets

```bash
make release          # release binary
make debug            # debug binary
make format           # clang-format source/tests
make tidy             # clang-tidy check
make tidy-fix         # clang-tidy with fixes
make valgrind         # valgrind run target
make profile          # perf-based profiling target
make coverage         # text coverage summary
make coverage-html    # html coverage report
make clean            # full clean
```
