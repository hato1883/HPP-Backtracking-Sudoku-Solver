#!/usr/bin/env python3
"""
Benchmark suite generator for sudoku-solver.
Creates a database of benchmark results for the current commit/tag.
"""

import os
import sys
import json
import subprocess
import glob
import argparse
import shutil
import tempfile
from pathlib import Path
from datetime import datetime
import struct
import statistics
import io
from contextlib import redirect_stdout

import validate_solution


def get_git_info():
    """Get current commit hash and tag."""
    try:
        # Get commit hash
        commit_hash = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            stderr=subprocess.DEVNULL
        ).decode().strip()

        # Try to get tag
        try:
            tag = subprocess.check_output(
                ["git", "describe", "--tags", "--exact-match"],
                stderr=subprocess.DEVNULL
            ).decode().strip()
            return commit_hash, tag
        except subprocess.CalledProcessError:
            # No tag found
            return commit_hash, None
    except subprocess.CalledProcessError:
        print("Warning: not a git repository", file=sys.stderr)
        return "unknown", None


def get_board_info(dat_file):
    """Extract board size and number of hints from .dat file."""
    try:
        with open(dat_file, 'rb') as f:
            block_size = struct.unpack('B', f.read(1))[0]
            side = struct.unpack('B', f.read(1))[0]
            board_data = f.read(side * side)

            # Count non-zero cells (hints/fixed cells)
            hints = sum(1 for b in board_data if b != 0)
            empty_cells = side * side - hints

            return {
                'block_size': block_size,
                'size': side,
                'hints': hints,
                'empty_cells': empty_cells,
            }
    except Exception as e:
        print(f"Error reading {dat_file}: {e}", file=sys.stderr)
        return None


def extract_benchmark_json(stdout_bytes):
    """Extract benchmark JSON object from mixed binary/text stdout."""
    import re

    stdout_text = stdout_bytes.decode('utf-8', errors='ignore')
    matches = re.findall(r'\{[^{}]*\}', stdout_text)

    # Prefer the last valid JSON object because benchmark stats are printed at the end.
    for raw_json in reversed(matches):
        try:
            data = json.loads(raw_json)
        except json.JSONDecodeError:
            continue

        if isinstance(data, dict) and 'status' in data and 'elapsed_ns' in data:
            return data

    return None


def extract_solution_blob(stdout_bytes):
    """Extract binary board payload from solver stdout."""
    if len(stdout_bytes) < 2:
        return None

    side = stdout_bytes[1]

    expected_size = 2 + side * side
    if len(stdout_bytes) < expected_size:
        return None

    return stdout_bytes[:expected_size]


def validate_answer_file(answer_file):
    """Validate a generated answer.dat file using validate_solution logic."""
    try:
        # Suppress validator prints during benchmark runs.
        with redirect_stdout(io.StringIO()):
            is_valid, errors = validate_solution.validate_sudoku(str(answer_file))
        return is_valid, errors
    except Exception as e:
        return False, [f"Validation error: {e}"]


def run_benchmark(solver_path, dat_file, answer_file, timeout=60):
    """Run solver with timeout and extract timing data."""
    try:
        result = subprocess.run(
            [solver_path, dat_file, "--benchmark"],
            capture_output=True,
            text=False,
            timeout=timeout
        )

        data = extract_benchmark_json(result.stdout)
        if not data:
            return None

        benchmark_result = {
            'elapsed_ns': data.get('elapsed_ns'),
            'elapsed_s': data.get('elapsed_s'),
            'status': data.get('status'),
        }

        if benchmark_result['status'] == 'SUCCESS':
            solution_blob = extract_solution_blob(result.stdout)
            if not solution_blob:
                benchmark_result['status'] = 'OUTPUT_PARSE_ERROR'
                benchmark_result['error'] = 'Could not extract solution bytes from solver output'
                return benchmark_result

            try:
                with open(answer_file, 'wb') as f:
                    f.write(solution_blob)
            except Exception as e:
                benchmark_result['status'] = 'OUTPUT_WRITE_ERROR'
                benchmark_result['error'] = f'Could not write {answer_file}: {e}'

        return benchmark_result
    except subprocess.TimeoutExpired:
        return {'status': 'TIMEOUT', 'elapsed_ns': timeout * 1_000_000_000}
    except Exception as e:
        print(f"Error running benchmark on {dat_file}: {e}", file=sys.stderr)
        return None


def find_test_scenarios(scenarios_dir):
    """Find all .dat files in scenarios directory, sorted by board size then by difficulty."""
    if not os.path.isdir(scenarios_dir):
        print(f"Error: scenarios directory not found: {scenarios_dir}")
        return []

    files = glob.glob(os.path.join(scenarios_dir, "*.dat"))
    
    def parse_board_size(filename):
        """Extract board base size from filename like 'board_2x2_...' or 'board_15x15_...'"""
        basename = os.path.basename(filename)
        # Extract the base number from board_NxN format
        if basename.startswith('board_'):
            parts = basename.split('_')
            if 'x' in parts[1]:
                base = int(parts[1].split('x')[0])
                # Extract difficulty rating: count empty cells from filename
                # Format: board_NxN_difficulty_EMPTY_NUM.dat
                if len(parts) >= 4:
                    try:
                        empty_cells = int(parts[3])
                        return (base, empty_cells)
                    except ValueError:
                        pass
        return (999, 0)
    
    # Sort by board base size first (small to large), then by empty cells (easy to hard)
    files = sorted(files, key=parse_board_size)
    return files


def calculate_statistics(times_ns, outlier_std=3.0):
    """Calculate mean, std dev, and min, rejecting outliers."""
    if not times_ns:
        return None

    times = list(times_ns)
    if len(times) == 1:
        return {
            'mean_ns': float(times[0]),
            'std_ns': 0.0,
            'min_ns': float(times[0]),
            'count': 1,
        }

    mean = statistics.mean(times)
    stdev = statistics.stdev(times) if len(times) > 1 else 0.0

    # Reject outliers (values more than outlier_std standard deviations away)
    if stdev > 0:
        filtered_times = [t for t in times if abs(t - mean) <= outlier_std * stdev]
    else:
        filtered_times = times

    if not filtered_times:
        filtered_times = times

    mean = statistics.mean(filtered_times)
    stdev = statistics.stdev(filtered_times) if len(filtered_times) > 1 else 0.0

    return {
        'mean_ns': float(mean),
        'std_ns': float(stdev),
        'min_ns': float(min(filtered_times)),
        'count': len(filtered_times),
    }


def load_results(results_file):
    """Load existing benchmark results for resume, if available."""
    if not results_file.exists():
        return None

    try:
        with open(results_file, 'r') as f:
            data = json.load(f)
    except Exception as e:
        print(f"Warning: could not load {results_file}: {e}", file=sys.stderr)
        return None

    if not isinstance(data, dict):
        print(f"Warning: invalid results format in {results_file}; expected object", file=sys.stderr)
        return None

    metadata = data.get('metadata')
    benchmarks = data.get('benchmarks')
    if not isinstance(metadata, dict) or not isinstance(benchmarks, dict):
        print(f"Warning: invalid results format in {results_file}; expected metadata/benchmarks", file=sys.stderr)
        return None

    return data


def save_results(results, results_file):
    """Atomically save benchmark results so interruptions do not corrupt checkpoints."""
    temp_file = results_file.with_suffix(results_file.suffix + '.tmp')
    with open(temp_file, 'w') as f:
        json.dump(results, f, indent=2)
        f.flush()
        os.fsync(f.fileno())
    os.replace(temp_file, results_file)


def checkpoint_results(results, results_file):
    """Update checkpoint metadata and persist results to disk."""
    metadata = results.setdefault('metadata', {})
    metadata['last_updated'] = datetime.now().isoformat()
    metadata['completed_tests'] = len(results.get('benchmarks', {}))
    save_results(results, results_file)


def main():
    # Parse command-line arguments
    parser = argparse.ArgumentParser(
        description='Benchmark suite generator for sudoku-solver',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        '--no-skip',
        action='store_true',
        help='Disable early forfeit - run all tests even if previous boards of same size timed out'
    )
    parser.add_argument(
        '--consecutive-fails',
        type=int,
        default=2,
        metavar='N',
        help='Number of consecutive failures before skipping remaining boards of that size (default: 2, use 0 with --no-skip)'
    )
    parser.add_argument(
        '--continue-on-invalid',
        action='store_true',
        help='When solution validation fails, only fail that scenario and continue (default: abort full benchmark)'
    )
    parser.add_argument(
        '--restart',
        action='store_true',
        help='Ignore existing results.json for this commit/tag and start from scratch'
    )
    args = parser.parse_args()
    
    # Handle --no-skip flag
    if args.no_skip:
        consecutive_fails_threshold = 0  # Never skip
    else:
        consecutive_fails_threshold = args.consecutive_fails
    
    # Get project root
    project_root = Path(__file__).parent.absolute()
    solver_path = project_root / "build" / "release" / "bin" / "sudoku-solver"
    scenarios_dir = project_root / "benchmarks" / "scenarios"
    temp_answers_dir = Path(tempfile.mkdtemp(prefix="benchmark-answers-"))

    if not solver_path.exists():
        print(f"Error: solver not found at {solver_path}")
        print("Please build the project first with: make")
        shutil.rmtree(temp_answers_dir, ignore_errors=True)
        return 1
    
    # Create a temporary copy of the solver to avoid hot-swap issues during recompilation
    print("Creating temporary copy of solver executable...")
    temp_solver_fd, temp_solver_path = tempfile.mkstemp(suffix="-sudoku-solver", prefix="benchmark-")
    os.close(temp_solver_fd)  # Close the file descriptor, we just need the path
    
    try:
        # Copy the executable
        shutil.copy2(solver_path, temp_solver_path)
        # Make sure it's executable
        os.chmod(temp_solver_path, 0o755)
        solver_path = temp_solver_path
        print(f"Using temporary solver at: {temp_solver_path}")

        # Get git info
        commit_hash, tag = get_git_info()
        db_name = tag if tag else commit_hash
        db_dir = project_root / "benchmarks" / db_name

        print(f"Benchmarking commit {commit_hash}{f' (tag: {tag})' if tag else ''}")
        print(f"Database directory: {db_dir}")

        # Create database directory
        db_dir.mkdir(parents=True, exist_ok=True)
        results_file = db_dir / "results.json"

        # Find test scenarios (sorted by board size, then difficulty)
        test_files = find_test_scenarios(scenarios_dir)
        if not test_files:
            print(f"No test scenarios found in {scenarios_dir}")
            return 1

        print(f"Found {len(test_files)} test scenarios (ordered by board size)")

        # Run benchmarks (resume existing results unless --restart is provided)
        results = None
        if not args.restart:
            results = load_results(results_file)
            if results is not None:
                print(f"Resuming from existing results: {len(results['benchmarks'])} scenario(s) already recorded")

        if results is None:
            results = {
                'metadata': {
                    'timestamp': datetime.now().isoformat(),
                },
                'benchmarks': {},
            }
            if args.restart and results_file.exists():
                print("Ignoring previous results due to --restart")

        metadata = results.setdefault('metadata', {})
        metadata['commit'] = commit_hash
        metadata['tag'] = tag
        metadata['total_tests'] = len(test_files)
        metadata['invalid_policy'] = 'continue' if args.continue_on_invalid else 'abort'
        checkpoint_results(results, results_file)

        # Track board sizes and their consecutive failures
        board_sizes_consecutive_fails = {}  # base -> count of consecutive failures
        
        def get_board_base(filename):
            """Extract board base size from filename."""
            basename = os.path.basename(filename)
            if basename.startswith('board_'):
                parts = basename.split('_')
                if 'x' in parts[1]:
                    try:
                        return int(parts[1].split('x')[0])
                    except ValueError:
                        pass
            return 999

        # Timeout value in seconds (used for both running and recording)
        TIMEOUT_SECONDS = 60
        TIMEOUT_NS = TIMEOUT_SECONDS * 1_000_000_000
        aborted_due_to_invalid = False
        
        for i, dat_file in enumerate(test_files, 1):
            filename = os.path.basename(dat_file)
            board_base = get_board_base(filename)

            # Resume support: skip scenarios that already have persisted results.
            if filename in results['benchmarks']:
                existing_status = results['benchmarks'][filename].get('status', 'UNKNOWN')
                if existing_status == 'SUCCESS':
                    board_sizes_consecutive_fails[board_base] = 0
                else:
                    board_sizes_consecutive_fails[board_base] = board_sizes_consecutive_fails.get(board_base, 0) + 1

                print(f"[{i}/{len(test_files)}] Reusing {filename}... ({existing_status})")
                continue
            
            # Check if we should skip this test based on earlier failures
            if consecutive_fails_threshold > 0:
                consecutive_fails = board_sizes_consecutive_fails.get(board_base, 0)
                if consecutive_fails >= consecutive_fails_threshold:
                    # Too many consecutive failures for this size
                    print(f"[{i}/{len(test_files)}] Skipping {filename}... ({consecutive_fails} consecutive failure(s) for {board_base}x{board_base})")
                    continue
            
            print(f"[{i}/{len(test_files)}] Benchmarking {filename}...", end='', flush=True)

            # Get board info
            board_info = get_board_info(dat_file)
            if not board_info:
                print(" FAILED (board info)")
                results['benchmarks'][filename] = {
                    'board_info': None,
                    'statistics': None,
                    'runs': [],
                    'status': 'BOARD_INFO_ERROR',
                }
                board_sizes_consecutive_fails[board_base] = board_sizes_consecutive_fails.get(board_base, 0) + 1
                checkpoint_results(results, results_file)
                continue

            # Run benchmark (multiple times for averaging)
            # Use 20 iterations: good balance between statistical significance and timeout risk
            times_ns = []
            failed_status = None
            failed_error = None
            invalid_errors = []
            answer_file = temp_answers_dir / "answer.dat"

            for _ in range(20):  # 20 runs per test
                if answer_file.exists():
                    answer_file.unlink()

                bench_result = run_benchmark(
                    str(solver_path),
                    dat_file,
                    answer_file,
                    timeout=TIMEOUT_SECONDS
                )
                
                if not bench_result:
                    failed_status = 'OUTPUT_PARSE_ERROR'
                    failed_error = 'Could not parse benchmark JSON from solver output'
                    break

                status = bench_result.get('status')
                if status != 'SUCCESS':
                    # Any failure or timeout forfeits the entire test
                    failed_status = status
                    failed_error = bench_result.get('error')
                    break

                elapsed_ns = bench_result.get('elapsed_ns')
                if elapsed_ns is None:
                    failed_status = 'OUTPUT_PARSE_ERROR'
                    failed_error = 'Missing elapsed_ns in benchmark output'
                    break

                # Validate every non-timeout successful scenario run.
                is_valid, validation_errors = validate_answer_file(answer_file)
                if not is_valid:
                    failed_status = 'INVALID'
                    invalid_errors = validation_errors
                    failed_error = '; '.join(validation_errors[:3]) if validation_errors else 'Unknown validation error'
                    break

                times_ns.append(elapsed_ns)

            if failed_status is None:
                # All 20 runs succeeded
                stats = calculate_statistics(times_ns)
                if stats:
                    results['benchmarks'][filename] = {
                        'board_info': board_info,
                        'statistics': stats,
                        'runs': times_ns,
                        'status': 'SUCCESS',
                    }
                    print(f" OK ({stats['mean_ns']/1_000_000:.3f}ms)")
                    # Reset consecutive failure counter for this board size
                    board_sizes_consecutive_fails[board_base] = 0
                else:
                    print(f" FAILED (stat calc)")
                    results['benchmarks'][filename] = {
                        'board_info': board_info,
                        'statistics': None,
                        'runs': times_ns,
                        'status': 'STATISTICS_ERROR',
                        'error': 'Could not calculate timing statistics',
                    }
                    # Increment consecutive failure counter
                    board_sizes_consecutive_fails[board_base] = board_sizes_consecutive_fails.get(board_base, 0) + 1
            else:
                if failed_status == 'TIMEOUT':
                    print(" TIMEOUT/FORFEIT")
                    results['benchmarks'][filename] = {
                        'board_info': board_info,
                        'statistics': {
                            'mean_ns': float(TIMEOUT_NS),
                            'std_ns': 0.0,
                            'min_ns': float(TIMEOUT_NS),
                            'count': 0,
                        },
                        'runs': times_ns,
                        'status': 'TIMEOUT',
                    }
                elif failed_status == 'INVALID':
                    print(" INVALID")
                    results['benchmarks'][filename] = {
                        'board_info': board_info,
                        'statistics': None,
                        'runs': times_ns,
                        'status': 'INVALID',
                        'validation_errors': invalid_errors,
                    }

                    if not args.continue_on_invalid:
                        aborted_due_to_invalid = True
                else:
                    print(f" FAILED ({failed_status})")
                    results['benchmarks'][filename] = {
                        'board_info': board_info,
                        'statistics': None,
                        'runs': times_ns,
                        'status': failed_status,
                        'error': failed_error,
                    }

                # Increment consecutive failure counter
                board_sizes_consecutive_fails[board_base] = board_sizes_consecutive_fails.get(board_base, 0) + 1

            checkpoint_results(results, results_file)
            if aborted_due_to_invalid:
                break

        print(f"\nBenchmark complete. Results saved to: {results_file}")
        successful_count = sum(
            1 for benchmark in results['benchmarks'].values() if benchmark.get('status') == 'SUCCESS'
        )
        invalid_count = sum(
            1 for benchmark in results['benchmarks'].values() if benchmark.get('status') == 'INVALID'
        )
        print(f"Total successful benchmarks: {successful_count}")
        print(f"Total invalid benchmarks: {invalid_count}")

        if aborted_due_to_invalid:
            print("Benchmark aborted: invalid solution encountered.")
            print("Use --continue-on-invalid to keep running remaining scenarios.")
            return 2

        return 0
    
    finally:
        # Clean up temporary solver executable
        if os.path.exists(temp_solver_path):
            print(f"Cleaning up temporary solver: {temp_solver_path}")
            os.remove(temp_solver_path)
        if temp_answers_dir.exists():
            shutil.rmtree(temp_answers_dir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
