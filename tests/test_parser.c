#include "parser/parser.h"
#include "test_helpers.h"

#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static bool write_bytes_to_file(const char* path, const unsigned char* bytes, size_t length)
{
    FILE* file = fopen(path, "wb");
    if (file == NULL)
    {
        return false;
    }

    const bool ok = fwrite(bytes, sizeof(unsigned char), length, file) == length;
    fclose(file);
    return ok;
}

// Verify that board serialization produces correct binary layout to stream
static void test_write_board_to_stream_serializes_expected_layout(void)
{
    // clang-format off
    const hpp_cell cells[] = {
            1, 0, 3, 0,
            0, 4, 0, 2,
            2, 0, 4, 0,
            0, 3, 0, 1,
    };
    // clang-format on

    hpp_board* board = hpp_test_create_board_from_cells(cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    FILE* stream = tmpfile();
    if (stream == NULL)
    {
        CU_FAIL_FATAL("Failed to open temporary stream");
        destroy_board(&board);
        return;
    }

    CU_ASSERT_EQUAL(write_board_to_stream(stream, board, 1), 0);
    rewind(stream);

    unsigned char block = 0;
    unsigned char side  = 0;
    CU_ASSERT_EQUAL(fread(&block, sizeof(unsigned char), 1, stream), 1);
    CU_ASSERT_EQUAL(fread(&side, sizeof(unsigned char), 1, stream), 1);
    CU_ASSERT_EQUAL(block, 2);
    CU_ASSERT_EQUAL(side, 4);

    hpp_cell roundtrip_cells[16] = {0};
    CU_ASSERT_EQUAL(fread(roundtrip_cells, sizeof(hpp_cell), board->cell_count, stream),
                    board->cell_count);

    for (size_t idx = 0; idx < board->cell_count; ++idx)
    {
        CU_ASSERT_EQUAL(roundtrip_cells[idx], cells[idx]);
    }

    fclose(stream);
    destroy_board(&board);
}
// Verify that solution writing rejects NULL filename or board pointers// Verify that board
// serialization rejects NULL file or board pointers
static void test_write_board_to_stream_rejects_invalid_arguments(void)
{
    hpp_board* board = create_board(4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    FILE* stream = tmpfile();
    if (stream == NULL)
    {
        CU_FAIL_FATAL("Failed to open temporary stream");
        destroy_board(&board);
        return;
    }

    CU_ASSERT_EQUAL(write_board_to_stream(NULL, board, 1), -1);
    CU_ASSERT_EQUAL(write_board_to_stream(stream, NULL, 1), -1);

    fclose(stream);
    destroy_board(&board);
}

// Verify that writing and parsing solutions produces identical results
static void test_write_solution_and_parse_file_round_trip(void)
{
    // clang-format off
    const hpp_cell cells[] = {
            1, 0, 3, 0,
            0, 4, 0, 2,
            2, 0, 4, 0,
            0, 3, 0, 1,
    };
    // clang-format on

    hpp_board* board = hpp_test_create_board_from_cells(cells, 4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    char template_path[] = "/tmp/hpp_parser_roundtrip_XXXXXX";
    int  fd              = mkstemp(template_path);
    if (fd == -1)
    {
        CU_FAIL_FATAL("Failed to create temporary file path");
        destroy_board(&board);
        return;
    }
    close(fd);

    CU_ASSERT_EQUAL(write_solution(template_path, board), 0);

    hpp_board* parsed = parse_file(template_path);
    CU_ASSERT_PTR_NOT_NULL(parsed);
    if (parsed != NULL)
    {
        CU_ASSERT_EQUAL(parsed->side_length, board->side_length);
        CU_ASSERT_EQUAL(parsed->block_length, board->block_length);
        hpp_test_assert_board_cells(parsed, cells);
    }

    destroy_board(&parsed);
    destroy_board(&board);
    (void)remove(template_path);
}

static void test_write_solution_rejects_invalid_arguments(void)
{
    hpp_board* board = create_board(4);
    if (board == NULL)
    {
        CU_FAIL_FATAL("Failed to allocate board");
        return;
    }

    CU_ASSERT_EQUAL(write_solution(NULL, board), -1);
    CU_ASSERT_EQUAL(write_solution("/tmp/hpp_parser_unused", NULL), -1);

    destroy_board(&board);
}

static void test_parse_file_exits_on_inconsistent_header(void)
{
    const unsigned char bytes[] = {
        2,
        9,
        1,
        2,
        3,
        4,
    };

    char template_path[] = "/tmp/hpp_parser_bad_header_XXXXXX";
    int  fd              = mkstemp(template_path);
    if (fd == -1)
    {
        CU_FAIL_FATAL("Failed to create temporary file path");
        return;
    }
    close(fd);

    if (!write_bytes_to_file(template_path, bytes, sizeof(bytes)))
    {
        CU_FAIL_FATAL("Failed to seed temporary parser file");
        (void)remove(template_path);
        return;
    }

    const pid_t child = fork();
    if (child == -1)
    {
        CU_FAIL_FATAL("fork() failed");
        (void)remove(template_path);
        return;
    }

    if (child == 0)
    {
        (void)parse_file(template_path);
        _exit(0);
    }

    int status = 0;
    CU_ASSERT_NOT_EQUAL(waitpid(child, &status, 0), -1);
    CU_ASSERT_TRUE(WIFEXITED(status));
    CU_ASSERT_NOT_EQUAL(WEXITSTATUS(status), 0);

    (void)remove(template_path);
}

static void test_parse_file_exits_on_truncated_payload(void)
{
    const unsigned char bytes[] = {
        2,
        4,
        1,
        2,
        3,
    };

    char template_path[] = "/tmp/hpp_parser_truncated_XXXXXX";
    int  fd              = mkstemp(template_path);
    if (fd == -1)
    {
        CU_FAIL_FATAL("Failed to create temporary file path");
        return;
    }
    close(fd);

    if (!write_bytes_to_file(template_path, bytes, sizeof(bytes)))
    {
        CU_FAIL_FATAL("Failed to seed temporary parser file");
        (void)remove(template_path);
        return;
    }

    const pid_t child = fork();
    if (child == -1)
    {
        CU_FAIL_FATAL("fork() failed");
        (void)remove(template_path);
        return;
    }

    if (child == 0)
    {
        (void)parse_file(template_path);
        _exit(0);
    }

    int status = 0;
    CU_ASSERT_NOT_EQUAL(waitpid(child, &status, 0), -1);
    CU_ASSERT_TRUE(WIFEXITED(status));
    CU_ASSERT_NOT_EQUAL(WEXITSTATUS(status), 0);

    (void)remove(template_path);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    hpp_test_disable_logging();

    CU_pSuite suite = CU_add_suite("Parser", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite,
                    "write board to stream serializes expected layout",
                    test_write_board_to_stream_serializes_expected_layout) == NULL ||
        CU_add_test(suite,
                    "write board to stream rejects invalid arguments",
                    test_write_board_to_stream_rejects_invalid_arguments) == NULL ||
        CU_add_test(suite,
                    "write solution and parse file round trip",
                    test_write_solution_and_parse_file_round_trip) == NULL ||
        CU_add_test(suite,
                    "write solution rejects invalid arguments",
                    test_write_solution_rejects_invalid_arguments) == NULL ||
        CU_add_test(suite,
                    "parse file exits on inconsistent header",
                    test_parse_file_exits_on_inconsistent_header) == NULL ||
        CU_add_test(suite,
                    "parse file exits on truncated payload",
                    test_parse_file_exits_on_truncated_payload) == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    const unsigned int failures = CU_get_number_of_failures();
    CU_cleanup_registry();

    return (failures == 0U) ? 0 : 1;
}
