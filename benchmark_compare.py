#!/usr/bin/env python3
"""
Benchmark comparison and scoring tool.
Compares two benchmark databases and scores them objectively.
"""

import json
import sys
from pathlib import Path
import statistics
import argparse

# Default timeout in nanoseconds (used when a test is missing from one database)
DEFAULT_TIMEOUT_NS = 60 * 1_000_000_000  # 60 seconds

# ANSI colors for terminal output
ANSI_RED = "\033[31m"
ANSI_GREEN = "\033[32m"
ANSI_RESET = "\033[0m"


def colorize(text, color, enabled=True):
    """Apply ANSI color to text when color output is enabled."""
    if not enabled or not color:
        return text
    return f"{color}{text}{ANSI_RESET}"


def load_benchmark_db(db_path):
    """Load benchmark database from results.json."""
    results_file = Path(db_path) / "results.json"
    if not results_file.exists():
        print(f"Error: {results_file} not found")
        return None

    with open(results_file, 'r') as f:
        return json.load(f)


def calculate_difficulty_weight(board_info):
    """
    Calculate difficulty weight based on board size and empty cells.
    Larger boards and more empty cells = harder = higher weight.
    """
    size = board_info['size']
    empty_cells = board_info['empty_cells']
    total_cells = board_info['size'] ** 2

    # Weight formula: (size_factor) * (emptiness_factor)
    # Favor larger boards more (quadratic in size since search space grows exponentially)
    size_weight = (size / 9.0) ** 2  # 9x9 is baseline
    emptiness_weight = empty_cells / total_cells

    return size_weight * emptiness_weight


def calculate_score(benchmark_db, all_tests=None, board_info_by_test=None):
    """
    Calculate score for a benchmark database.
    Lower is better (nanoseconds for lower weighted problems).

    If all_tests is provided, every listed test is scored. Tests missing from
    the database are treated as timeout runs (DEFAULT_TIMEOUT_NS), using board
    info from board_info_by_test.
    """
    if not benchmark_db or 'benchmarks' not in benchmark_db:
        return None

    if all_tests is not None and board_info_by_test is None:
        raise ValueError("board_info_by_test is required when all_tests is provided")

    weighted_times = []
    assumed_timeout_count = 0
    measured_count = 0

    benchmarks = benchmark_db.get('benchmarks', {})
    test_names = all_tests if all_tests is not None else benchmarks.keys()

    for filename in test_names:
        bench_data = benchmarks.get(filename)

        if bench_data:
            board_info = bench_data.get('board_info')
            stats = bench_data.get('statistics')
        else:
            board_info = board_info_by_test.get(filename)
            stats = None

        if not board_info or not stats:
            # Missing tests (or tests without timing stats) count as timeout.
            mean_time_ns = DEFAULT_TIMEOUT_NS
            assumed_timeout_count += 1
            assumed_timeout = True
        else:
            # Use mean time (with outlier rejection already done)
            mean_time_ns = stats.get('mean_ns', DEFAULT_TIMEOUT_NS)
            measured_count += 1
            assumed_timeout = False

        # Calculate difficulty weight
        difficulty = calculate_difficulty_weight(board_info)

        # Weighted score (lower is better)
        # Smaller inputs count less, larger inputs count more
        weighted_time = mean_time_ns * difficulty

        weighted_times.append({
            'filename': filename,
            'mean_ns': mean_time_ns,
            'difficulty': difficulty,
            'weighted_time': weighted_time,
            'assumed_timeout': assumed_timeout,
        })

    if not weighted_times:
        return None

    # Calculate aggregate score
    total_weighted_time = sum(w['weighted_time'] for w in weighted_times)
    avg_difficulty = statistics.mean([w['difficulty'] for w in weighted_times])

    return {
        'total_weighted_time_ns': total_weighted_time,
        'average_difficulty': avg_difficulty,
        'num_benchmarks': len(weighted_times),
        'num_measured': measured_count,
        'num_assumed_timeout': assumed_timeout_count,
        'details': weighted_times,
    }


def compare_databases(db1_path, db2_path, warn_missing=True):
    """Compare two benchmark databases and print results."""
    use_color = sys.stdout.isatty()

    print(f"Loading database 1: {db1_path}")
    db1 = load_benchmark_db(db1_path)
    if not db1:
        return 1

    print(f"Loading database 2: {db2_path}")
    db2 = load_benchmark_db(db2_path)
    if not db2:
        return 1

    # Extract metadata
    meta1 = db1.get('metadata', {})
    meta2 = db2.get('metadata', {})

    print(f"\n{'Field':<20} | {'Database 1':<20} | {'Database 2':<20}")
    print(f"{'-'*20}-+-{'-'*20}-+-{'-'*20}")
    print(f"{'Commit:':<20} | {meta1.get('commit', 'unknown'):<20} | {meta2.get('commit', 'unknown'):<20}")
    tag1 = meta1.get('tag') or '-'
    tag2 = meta2.get('tag') or '-'
    print(f"{'Tag:':<20} | {tag1:<20} | {tag2:<20}")
    print(f"{'Tests:':<20} | {meta1.get('total_tests', 0):<20} | {meta2.get('total_tests', 0):<20}")

    # Build a common test universe so both DBs are scored fairly.
    db1_benchmarks = db1.get('benchmarks', {})
    db2_benchmarks = db2.get('benchmarks', {})
    common_test_universe = sorted(set(db1_benchmarks.keys()) | set(db2_benchmarks.keys()))

    board_info_by_test = {}
    for test_name in common_test_universe:
        bench1 = db1_benchmarks.get(test_name)
        bench2 = db2_benchmarks.get(test_name)

        if bench1 and bench1.get('board_info'):
            board_info_by_test[test_name] = bench1['board_info']
        elif bench2 and bench2.get('board_info'):
            board_info_by_test[test_name] = bench2['board_info']

    common_test_universe = [
        test_name for test_name in common_test_universe
        if test_name in board_info_by_test
    ]

    # Calculate comparable scores across the same test set.
    score1 = calculate_score(db1, all_tests=common_test_universe, board_info_by_test=board_info_by_test)
    score2 = calculate_score(db2, all_tests=common_test_universe, board_info_by_test=board_info_by_test)

    if not score1 or not score2:
        print("Error: Could not calculate scores")
        return 1

    print(f"\n{'='*60}")
    print(f"SCORES (missing tests count as timeout, lower is better)")
    print(f"{'='*60}")

    print(f"\nDatabase 1 Score:")
    print(f"  Total Weighted Time: {score1['total_weighted_time_ns']:,.0f} ns")
    print(f"  Average Difficulty:  {score1['average_difficulty']:.3f}")
    print(f"  Benchmarks:          {score1['num_benchmarks']}")
    print(f"  Measured:            {score1['num_measured']}")
    print(f"  Assumed Timeouts:    {score1['num_assumed_timeout']}")

    print(f"\nDatabase 2 Score:")
    print(f"  Total Weighted Time: {score2['total_weighted_time_ns']:,.0f} ns")
    print(f"  Average Difficulty:  {score2['average_difficulty']:.3f}")
    print(f"  Benchmarks:          {score2['num_benchmarks']}")
    print(f"  Measured:            {score2['num_measured']}")
    print(f"  Assumed Timeouts:    {score2['num_assumed_timeout']}")

    # Compare
    print(f"\n{'='*60}")
    print(f"COMPARISON")
    print(f"{'='*60}")

    improvement = ((score1['total_weighted_time_ns'] - score2['total_weighted_time_ns']) /
                   score1['total_weighted_time_ns'] * 100)

    if score2['total_weighted_time_ns'] < score1['total_weighted_time_ns']:
        print(f"✓ Database 2 is FASTER by {abs(improvement):.2f}%")
    elif score2['total_weighted_time_ns'] > score1['total_weighted_time_ns']:
        print(f"✗ Database 2 is SLOWER by {abs(improvement):.2f}%")
    else:
        print(f"≈ Database 2 is equivalent to Database 1")

    # Detailed comparison
    print(f"\n{'='*60}")
    print(f"DETAILED COMPARISON (sorted by difficulty)")
    print(f"{'='*60}")

    details1 = {d['filename']: d for d in score1['details']}
    details2 = {d['filename']: d for d in score2['details']}

    # Get all tests from both databases.
    all_tests = set(details1.keys()) | set(details2.keys())
    only_in_db1 = set(db1_benchmarks.keys()) - set(db2_benchmarks.keys())
    only_in_db2 = set(db2_benchmarks.keys()) - set(db1_benchmarks.keys())

    # Report missing tests
    if warn_missing and (only_in_db1 or only_in_db2):
        print(f"\n⚠ WARNING: Test set mismatch detected")
        if only_in_db1:
            print(f"  Tests only in DB1 ({len(only_in_db1)}): {', '.join(sorted(only_in_db1)[:3])}{'...' if len(only_in_db1) > 3 else ''}")
        if only_in_db2:
            print(f"  Tests only in DB2 ({len(only_in_db2)}): {', '.join(sorted(only_in_db2)[:3])}{'...' if len(only_in_db2) > 3 else ''}")
        print(f"  Missing tests are assumed to have timed out ({DEFAULT_TIMEOUT_NS/1_000_000_000:.0f}s)\n")

    # Get board info for missing tests (need for difficulty calculation)
    # We'll get it from the database that has it
    def get_test_data(filename, db1_details, db2_details):
        """Get test data, using timeout values for missing tests."""
        if filename in db1_details:
            d1 = db1_details[filename]
        else:
            # Not in db1, get board_info from db2 and use timeout
            d2 = db2_details[filename]
            d1 = {
                'filename': filename,
                'mean_ns': DEFAULT_TIMEOUT_NS,
                'difficulty': calculate_difficulty_weight(db2['benchmarks'][filename]['board_info']),
                'weighted_time': DEFAULT_TIMEOUT_NS * calculate_difficulty_weight(db2['benchmarks'][filename]['board_info']),
            }
        
        if filename in db2_details:
            d2 = db2_details[filename]
        else:
            # Not in db2, get board_info from db1 and use timeout
            d2 = {
                'filename': filename,
                'mean_ns': DEFAULT_TIMEOUT_NS,
                'difficulty': calculate_difficulty_weight(db1['benchmarks'][filename]['board_info']),
                'weighted_time': DEFAULT_TIMEOUT_NS * calculate_difficulty_weight(db1['benchmarks'][filename]['board_info']),
            }
        
        return d1, d2

    # Sort all tests by difficulty (using whichever db has the test)
    all_tests_sorted = sorted(all_tests, key=lambda f: 
        details1[f]['difficulty'] if f in details1 else details2[f]['difficulty'], 
        reverse=True)

    rows = []

    for test_file in all_tests_sorted:
        d1, d2 = get_test_data(test_file, details1, details2)

        ms1 = d1['mean_ns'] / 1_000_000
        ms2 = d2['mean_ns'] / 1_000_000
        diff_pct = ((ms1 - ms2) / ms1 * 100) if ms1 > 0 else 0

        # Format with special markers for timeouts/missing
        ms1_str = f"{ms1:.3f}"
        ms2_str = f"{ms2:.3f}"
        
        if d1.get('assumed_timeout', False):
            ms1_str += "* "
        elif d1['mean_ns'] >= DEFAULT_TIMEOUT_NS * 0.99:  # Close to timeout
            ms1_str += "T "
        else:
            ms1_str += "  "
        
        if d2.get('assumed_timeout', False):
            ms2_str += "* "
        elif d2['mean_ns'] >= DEFAULT_TIMEOUT_NS * 0.99:  # Close to timeout
            ms2_str += "T "
        else:
            ms2_str += "  "

        diff_str = f"{abs(diff_pct):+.1f}%"

        if ms2 < ms1:
            # DB2 is faster (better)
            diff_color = ANSI_GREEN
        elif ms2 > ms1:
            # DB1 is faster (better)
            diff_color = ANSI_RED
        else:
            diff_color = None

        rows.append({
            'test_file': test_file,
            'db1': ms1_str,
            'db2': ms2_str,
            'diff': diff_str,
            'diff_color': diff_color,
            'difficulty': d1['difficulty'],
        })

    file_col_width = len('Test File')
    db1_col_width = len('DB1 (ms)')
    db2_col_width = len('DB2 (ms)')
    diff_col_width = len('Diff %')
    difficulty_col_width = len('Difficulty')

    if rows:
        file_col_width = max(file_col_width, max(len(row['test_file']) for row in rows))
        db1_col_width = max(db1_col_width, max(len(row['db1']) for row in rows))
        db2_col_width = max(db2_col_width, max(len(row['db2']) for row in rows))
        diff_col_width = max(diff_col_width, max(len(row['diff']) for row in rows))
        difficulty_col_width = max(
            difficulty_col_width,
            max(len(f"{row['difficulty']:.3f}") for row in rows),
        )

    print(
        f"\n{'Test File':<{file_col_width}} "
        f"{'DB1 (ms)':>{db1_col_width}} "
        f"{'DB2 (ms)':>{db2_col_width}} "
        f"{'Diff %':>{diff_col_width}} "
        f"{'Difficulty':>{difficulty_col_width}}"
    )
    print(
        f"{'-' * file_col_width} "
        f"{'-' * db1_col_width} "
        f"{'-' * db2_col_width} "
        f"{'-' * diff_col_width} "
        f"{'-' * difficulty_col_width}"
    )

    for row in rows:
        diff_cell = f"{row['diff']:>{diff_col_width}}"
        diff_display = colorize(diff_cell, row['diff_color'], use_color)

        print(
            f"{row['test_file']:<{file_col_width}} "
            f"{row['db1']:>{db1_col_width}} "
            f"{row['db2']:>{db2_col_width}} "
            f"{diff_display} "
            f"{row['difficulty']:>{difficulty_col_width}.3f}"
        )
    
    print(f"\nLegend: T=Timeout, *=Missing (assumed timeout)")
    if use_color:
        print(f"Diff colors: {colorize('red', ANSI_RED)}=DB1 better, {colorize('green', ANSI_GREEN)}=DB2 better, default=equal")

    return 0


def main():
    parser = argparse.ArgumentParser(
        description='Compare two benchmark databases',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples:
  python3 benchmark_compare.py benchmarks/baseline benchmarks/v2.0
  python3 benchmark_compare.py benchmarks/v1.0.0 benchmarks/v1.1.0 --no-warn-missing
"""
    )
    parser.add_argument('db1', help='Path to first benchmark database')
    parser.add_argument('db2', help='Path to second benchmark database')
    parser.add_argument(
        '--no-warn-missing',
        action='store_true',
        help='Suppress warnings about tests present in only one database'
    )
    
    args = parser.parse_args()

    return compare_databases(args.db1, args.db2, warn_missing=not args.no_warn_missing)


if __name__ == "__main__":
    sys.exit(main())
