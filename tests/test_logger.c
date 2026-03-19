#include "test_helpers.h"
#include "utils/logger.h"

#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool capture_log_line(char*         buffer,
                             size_t        buffer_size,
                             hpp_log_level level,
                             const char*   file,
                             int           line,
                             const char*   func,
                             const char*   message,
                             hpp_log_level runtime_level,
                             int           runtime_verbosity)
{
    if (buffer == NULL || buffer_size == 0U)
    {
        return false;
    }

    buffer[0] = '\0';

    FILE* capture = tmpfile();
    if (capture == NULL)
    {
        return false;
    }

    const int stderr_fd = fileno(stderr);
    const int saved_fd  = dup(stderr_fd);
    if (saved_fd < 0)
    {
        fclose(capture);
        return false;
    }

    fflush(stderr);
    if (dup2(fileno(capture), stderr_fd) < 0)
    {
        close(saved_fd);
        fclose(capture);
        return false;
    }

    logger_set_level(runtime_level);
    logger_set_verbosity(runtime_verbosity);
    log_message(level, file, line, func, "%s", message);

    fflush(stderr);
    (void)dup2(saved_fd, stderr_fd);
    close(saved_fd);

    rewind(capture);
    const size_t bytes_read = fread(buffer, 1, buffer_size - 1U, capture);
    buffer[bytes_read]      = '\0';

    fclose(capture);
    return true;
}

// Verify that logger configuration settings can be set and retrieved correctly
static void test_logger_setters_and_getters_round_trip(void)
{
    const hpp_log_level original_level     = logger_get_level();
    const int           original_verbosity = logger_get_verbosity();

    logger_set_level(WARN);
    CU_ASSERT_EQUAL(logger_get_level(), WARN);

    logger_set_verbosity(-10);
    CU_ASSERT_EQUAL(logger_get_verbosity(), 0);

    logger_set_verbosity(99);
    CU_ASSERT_EQUAL(logger_get_verbosity(), 2);

    logger_set_level(original_level);
    logger_set_verbosity(original_verbosity);
}

// Verify that log messages are filtered according to runtime log level
static void test_log_message_respects_runtime_level(void)
{
    const hpp_log_level original_level     = logger_get_level();
    const int           original_verbosity = logger_get_verbosity();

    char output[256] = {0};
    CU_ASSERT_TRUE(
        capture_log_line(output, sizeof(output), INFO, "a.c", 10, "f", "suppressed", WARN, 0));
    CU_ASSERT_EQUAL(strlen(output), 0);

    CU_ASSERT_TRUE(
        capture_log_line(output, sizeof(output), ERROR, "a.c", 10, "f", "visible", WARN, 0));
    CU_ASSERT_TRUE(strstr(output, "visible") != NULL);

    logger_set_level(original_level);
    logger_set_verbosity(original_verbosity);
}

// Verify that verbose logging includes file, line, and function context
static void test_log_message_verbosity_formats_context(void)
{
    const hpp_log_level original_level     = logger_get_level();
    const int           original_verbosity = logger_get_verbosity();

    char output[512] = {0};

    CU_ASSERT_TRUE(capture_log_line(
        output, sizeof(output), DEBUG, "unit_file.c", 42, "unit_func", "v0", DEBUG, 0));
    CU_ASSERT_TRUE(strstr(output, "[DEBG]") != NULL);
    CU_ASSERT_TRUE(strstr(output, "v0") != NULL);
    CU_ASSERT_TRUE(strstr(output, "unit_file.c:42") == NULL);

    CU_ASSERT_TRUE(capture_log_line(
        output, sizeof(output), DEBUG, "unit_file.c", 42, "unit_func", "v1", DEBUG, 1));
    CU_ASSERT_TRUE(strstr(output, "unit_file.c:42") != NULL);
    CU_ASSERT_TRUE(strstr(output, "v1") != NULL);

    CU_ASSERT_TRUE(capture_log_line(
        output, sizeof(output), DEBUG, "unit_file.c", 42, "unit_func", "v2", DEBUG, 2));
    CU_ASSERT_TRUE(strstr(output, "unit_file.c:42") != NULL);
    CU_ASSERT_TRUE(strstr(output, "(unit_func)") != NULL);
    CU_ASSERT_TRUE(strstr(output, "v2") != NULL);

    logger_set_level(original_level);
    logger_set_verbosity(original_verbosity);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    hpp_test_disable_logging();

    CU_pSuite suite = CU_add_suite("Logger", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite,
                    "logger setters and getters",
                    test_logger_setters_and_getters_round_trip) == NULL ||
        CU_add_test(suite,
                    "log message respects runtime level",
                    test_log_message_respects_runtime_level) == NULL ||
        CU_add_test(suite,
                    "log message verbosity formats context",
                    test_log_message_verbosity_formats_context) == NULL)
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
