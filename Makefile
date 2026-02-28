CC = clang

DEBUG ?= 0

INCLUDES = -Iinclude
CFLAGS = -std=c17 -Wall -Wextra -MMD -MP $(INCLUDES) -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm

ifeq ($(DEBUG),1)
    CFLAGS += -g -O0 \
             -fsanitize=address,undefined \
             -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=address,undefined
	BUILD_TYPE = debug
else
    CFLAGS +=  -O3 -march=native -flto 
    LDFLAGS += -flto
	BUILD_TYPE = release
endif

BUILD = build
TARGET_DIR = $(BUILD)/$(BUILD_TYPE)

SRC := $(shell find src -name "*.c")
OBJ := $(patsubst src/%.c, $(TARGET_DIR)/obj/%.o, $(SRC))
DEP := $(OBJ:.o=.d)

TARGET = $(TARGET_DIR)/bin/sudoku-solver
COMPILE_DB = compile_commands.json

all: $(TARGET)

format:
	clang-format -i $(shell find src include -name "*.c" -o -name "*.h")

$(COMPILE_DB): $(SRC) Makefile
	bear -- make -B DEBUG=1

tidy: $(COMPILE_DB)
	run-clang-tidy -p . -quiet

tidy-fix: $(COMPILE_DB)
	run-clang-tidy -p . -quiet -fix

$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(TARGET_DIR)/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -r $(BUILD) $(OBJ) $(TARGET) $(COMPILE_DB)

.PHONY: all clean

-include $(DEP)