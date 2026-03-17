
# ============================================================
# Sudoku Solver Build System (Makefile)
# ------------------------------------------------------------
# Quick start:
#   make              -> build default (release)
#   make debug        -> build debug binary
#   make test         -> build and run all unit tests
#   make coverage     -> run tests and generate text coverage report
#   make valgrind     -> run binary under valgrind
#   make profile      -> run callgrind profiling and open KCachegrind
# ============================================================

CC = clang

# Set DEBUG=1 for debug build, otherwise release build is used.
# Example: make DEBUG=1
DEBUG ?= 0

# Compiler/linker base flags used by normal builds.
INCLUDES = -Iinclude
CFLAGS = -std=c17 -Wall -Wextra -MMD -MP $(INCLUDES) -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm

# Build profile selection.
# Debug profile keeps symbols and enables sanitizers.
# Release profile enables optimizations and LTO.
ifeq ($(DEBUG),1)
    CFLAGS += -g -O0 -DLOG_VERBOSITY=0 -DLOG_LEVEL=0\
             -fsanitize=address,undefined \
             -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=address,undefined
	BUILD_TYPE = debug
else
    CFLAGS +=  -DNDEBUG -DLOG_VERBOSITY=0 -DLOG_LEVEL=2 -O3 -march=native -flto 
    LDFLAGS += -flto
	BUILD_TYPE = release
endif

# Output directories for normal and test builds.
BUILD = build
TARGET_DIR = $(BUILD)/$(BUILD_TYPE)
TEST_BUILD = $(BUILD)/test/$(BUILD_TYPE)
TEST_BIN = $(TEST_BUILD)/bin

# Coverage-specific output and flags.
COVERAGE_BUILD = $(BUILD)/coverage
COVERAGE_HTML = $(COVERAGE_BUILD)/coverage.html
COVERAGE_CFLAGS = -std=c17 -Wall -Wextra -MMD -MP $(INCLUDES) -D_POSIX_C_SOURCE=200809L -fopenmp -g -O0 --coverage -DLOG_VERBOSITY=0 -DLOG_LEVEL=1
COVERAGE_LDFLAGS = -lm -fopenmp --coverage

# Valgrind-specific output and flags.
VALGRIND_TARGET_DIR = $(BUILD)/valgrind
VALGRIND_TARGET = $(VALGRIND_TARGET_DIR)/bin/sudoku-solver
VALGRIND_OBJ = $(patsubst src/%.c, $(VALGRIND_TARGET_DIR)/obj/%.o, $(SRC))
VALGRIND_CFLAGS = -std=c17 -Wall -Wextra -MMD -MP $(INCLUDES) -D_POSIX_C_SOURCE=200809L -fopenmp -g -gdwarf-4 -O0 -DLOG_VERBOSITY=0 -DLOG_LEVEL=1
VALGRIND_LDFLAGS = -lm -fopenmp
VALGRIND_FLAGS ?= --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1
VALGRIND_SCENARIO ?= benchmarks/scenarios/board_5x5_medium_156_1.dat

# Source/object/dependency discovery for application sources.
SRC := $(shell find src -name "*.c")
OBJ := $(patsubst src/%.c, $(TARGET_DIR)/obj/%.o, $(SRC))
DEP := $(OBJ:.o=.d)

# Coverage (core only: solver, parser, utils, main; excludes graphics)
COVERAGE_CORE_SRC := $(filter-out src/graphics/%.c, $(SRC))
COVERAGE_CORE_OBJ := $(patsubst src/%.c, $(COVERAGE_BUILD)/obj/src/%.o, $(COVERAGE_CORE_SRC))
COVERAGE_TEST_OBJS = $(patsubst tests/%.c, $(COVERAGE_BUILD)/obj/tests/%.o, $(TEST_SOURCES))
COVERAGE_TEST_BINS = $(patsubst tests/%.c, $(COVERAGE_BUILD)/bin/tests/%, $(TEST_SOURCES))
COVERAGE_TEST_LINK_OBJ = $(filter-out $(COVERAGE_BUILD)/obj/src/main.o, $(COVERAGE_CORE_OBJ))

# Test sources and objects (exclude main.c from linking with tests)
TEST_SOURCES := $(shell find tests -name "test_*.c")
TEST_BINS := $(patsubst tests/%.c, $(TEST_BIN)/%, $(TEST_SOURCES))
TEST_OBJS := $(patsubst tests/%.c, $(TEST_BUILD)/obj/%.o, $(TEST_SOURCES))
TEST_OBJ := $(filter-out $(TARGET_DIR)/obj/main.o, $(OBJ))

# Final binary and compile database names.
TARGET = $(TARGET_DIR)/bin/sudoku-solver
COMPILE_DB = compile_commands.json

debug:

# =========================
# Build and Run Targets
# =========================

.PHONY: all
all: $(TARGET)


.PHONY: release
release: $(TARGET_DIR)/bin/sudoku-solver

.PHONY: debug
debug: build-debug

.PHONY: build-debug
build-debug:
	$(MAKE) --no-print-directory DEBUG=1 all

.PHONY: run-debug
run-debug: debug
	./build/debug/bin/sudoku-solver --ui

.PHONY: run-release
run-release: release
	./build/release/bin/sudoku-solver

# =========================
# Formatting and Tooling
# =========================

.PHONY: format
format:
	clang-format -i $(shell find src include -name "*.c" -o -name "*.h")
	clang-format -i $(shell find tests include -name "*.c" -o -name "*.h")

$(COMPILE_DB): $(SRC) $(TEST_SOURCES) Makefile
	bear -- make -B DEBUG=1 all test-objs

.PHONY: tidy
tidy: $(COMPILE_DB)
	run-clang-tidy -p . -quiet $(SRC)
	run-clang-tidy -p . -quiet -config-file=tests/.clang-tidy $(TEST_SOURCES)

.PHONY: tidy-fix
tidy-fix: $(COMPILE_DB)
	run-clang-tidy -p . -quiet -fix $(SRC)
	run-clang-tidy -p . -quiet -fix -config-file=tests/.clang-tidy $(TEST_SOURCES)

# Link application binary.
$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

# Compile regular application object files.
$(TARGET_DIR)/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile valgrind-specific object files.
$(VALGRIND_TARGET_DIR)/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(VALGRIND_CFLAGS) -c $< -o $@

# Link valgrind binary.
$(VALGRIND_TARGET): $(VALGRIND_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(VALGRIND_OBJ) -o $@ $(VALGRIND_LDFLAGS)

# Compile test object files.
$(TEST_BUILD)/obj/%.o: tests/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(PWD)/tests -c $< -o $@

# Compile coverage-instrumented application object files.
$(COVERAGE_BUILD)/obj/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(COVERAGE_CFLAGS) -c $< -o $@

# Compile coverage-instrumented test object files.
$(COVERAGE_BUILD)/obj/tests/%.o: tests/%.c
	@mkdir -p $(dir $@)
	$(CC) $(COVERAGE_CFLAGS) -I$(PWD)/tests -c $< -o $@


# =========================
# Testing
# =========================

.PHONY: test-objs
test-objs: $(TEST_OBJS)

.PHONY: test
test: $(TEST_BINS)
	@echo "Running all unit tests..."
	@fail=0; \
	for test_bin in $(TEST_BINS); do \
		echo ""; \
		echo "===== Running $$test_bin ====="; \
		$$test_bin; \
		status=$$?; \
		if [ $$status -eq 0 ]; then \
			echo "PASS: $$test_bin"; \
		else \
			echo "FAIL: $$test_bin (exit code $$status)"; \
			fail=1; \
		fi; \
	done; \
	exit $$fail

$(TEST_BIN)/%: $(TEST_BUILD)/obj/%.o $(TEST_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $< $(TEST_OBJ) -o $@ $(LDFLAGS) -lcunit

$(COVERAGE_BUILD)/bin/tests/%: $(COVERAGE_BUILD)/obj/tests/%.o $(COVERAGE_TEST_LINK_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $< $(COVERAGE_TEST_LINK_OBJ) -o $@ $(COVERAGE_LDFLAGS) -lcunit

.PHONY: test-verbose
test-verbose: CFLAGS += -DTEST_VERBOSE=1
test-verbose: test

.PHONY: test-clean
test-clean:
	$(RM) -r $(TEST_BUILD)


# =========================
# Valgrind and Profiling
# =========================

.PHONY: valgrind-build
valgrind-build: $(VALGRIND_TARGET)

.PHONY: valgrind
valgrind: valgrind-build
	@command -v valgrind >/dev/null 2>&1 || { echo "valgrind not found. Install it first (e.g. sudo apt install valgrind)."; exit 1; }
	valgrind $(VALGRIND_FLAGS) $(VALGRIND_TARGET) --benchmark $(VALGRIND_SCENARIO)

.PHONY: profile
profile: build/release/bin/sudoku-solver
	@command -v valgrind >/dev/null 2>&1 || { echo "valgrind not found. Install it first (e.g. sudo apt install valgrind)."; exit 1; }
	@command -v kcachegrind >/dev/null 2>&1 || { echo "kcachegrind not found. Install it first (e.g. sudo apt install kcachegrind)."; exit 1; }
	@command -v cg_annotate >/dev/null 2>&1 || { echo "cg_annotate not found. Install it first (e.g. sudo apt install valgrind)."; exit 1; }
	@mkdir -p build/callgrind build/cachegrind
	cp benchmarks/scenarios/board_5x5_medium_156_1.dat ./board.dat
	# Callgrind (function-level hotspots)
	valgrind --tool=callgrind --dump-instr=yes --collect-jumps=yes --callgrind-out-file=build/callgrind/callgrind.out.$$(date +%s) ./build/release/bin/sudoku-solver > /dev/null 2>&1 || true
	@latest_call=$$(ls -1t build/callgrind/callgrind.out.* | head -n1); \
	echo "Opening $$latest_call with KCachegrind..."; \
	callgrind_annotate --tree=both --inclusive=yes --auto=yes --show-percs=yes $$latest_call > $$latest_call.tree; \
	kcachegrind $$latest_call &
	# Cachegrind (cache performance)
	valgrind --tool=cachegrind --cachegrind-out-file=build/cachegrind/cachegrind.out.$$(date +%s) ./build/release/bin/sudoku-solver > /dev/null 2>&1 || true
	@latest_cache=$$(ls -1t build/cachegrind/cachegrind.out.* | head -n1); \
	echo "\nCachegrind summary (cache performance):"; \
	cg_annotate $$latest_cache > $$latest_cache.tree; \
	head -40 $$latest_cache.tree


# =========================
# Coverage
# =========================

.PHONY: coverage-build
coverage-build: $(COVERAGE_TEST_BINS)

.PHONY: coverage-run
coverage-run: coverage-build
	@echo "Running unit tests for coverage..."
	@for test_bin in $(COVERAGE_TEST_BINS); do \
		echo ""; \
		$$test_bin || exit 1; \
	done

.PHONY: coverage
coverage: coverage-run
	@command -v gcovr >/dev/null 2>&1 || { echo "gcovr not found. Install it first (e.g. sudo apt install gcovr)."; exit 1; }
	@mkdir -p $(COVERAGE_BUILD)
	gcovr -r . \
		--filter '^src/solver/' \
		--filter '^src/parser/' \
		--filter '^src/utils/' \
		--filter '^src/ds/' \
		--filter '^src/main\.c$$' \
		--exclude '^src/graphics/' \
		--txt $(COVERAGE_BUILD)/coverage.txt \
		--print-summary

.PHONY: coverage-html
coverage-html: coverage-run
	@command -v gcovr >/dev/null 2>&1 || { echo "gcovr not found. Install it first (e.g. sudo apt install gcovr)."; exit 1; }
	@mkdir -p $(COVERAGE_BUILD)
	gcovr -r . \
		--filter '^src/solver/' \
		--filter '^src/parser/' \
		--filter '^src/utils/' \
		--filter '^src/ds/' \
		--filter '^src/main\.c$$' \
		--exclude '^src/graphics/' \
		--html-details $(COVERAGE_HTML) \
		--print-summary
	@echo "HTML coverage report written to $(COVERAGE_HTML)"

.PHONY: coverage-clean
coverage-clean:
	$(RM) -r $(COVERAGE_BUILD)


# =========================
# Cleaning
# =========================

.PHONY: clean
clean:
	$(RM) -r $(BUILD) $(OBJ) $(TARGET) $(COMPILE_DB)

# =========================
# Dependency Tracking
# =========================

# Include auto-generated dependency files so that changes to headers
# trigger recompilation of the affected object files.
-include $(DEP)
-include $(TEST_OBJS:.o=.d)
