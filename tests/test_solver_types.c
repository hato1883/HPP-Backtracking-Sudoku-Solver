#include "solver/sink.h"
#include "test_helpers.h"

#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <stdint.h>

typedef struct ProgressProbe
{
    uint64_t calls;
    uint64_t last_sequence;
    uint64_t last_iteration;
    uint8_t  last_value;
} hpp_progress_probe;

static int probe_progress_callback(const hpp_solver_progress* progress, void* userdata)
{
    hpp_progress_probe* probe = (hpp_progress_probe*)userdata;
    if (probe == NULL || progress == NULL)
    {
        return 1;
    }

    probe->calls++;
    probe->last_sequence  = progress->sequence;
    probe->last_iteration = progress->iteration;
    probe->last_value     = progress->latest_move.payload.assign.new_value;
    return 0;
}

// Verify that move assignment correctly copies all payload fields
static void test_move_assign_payload_layout(void)
{
    hpp_move move = {
        .kind = MOVE_ASSIGN,
        .payload.assign =
            {
                .row       = 7,
                .col       = 5,
                .old_value = 2,
                .new_value = 9,
            },
    };

    CU_ASSERT_EQUAL(move.kind, MOVE_ASSIGN);
    CU_ASSERT_EQUAL(move.payload.assign.row, 7);
    CU_ASSERT_EQUAL(move.payload.assign.col, 5);
    CU_ASSERT_EQUAL(move.payload.assign.old_value, 2);
    CU_ASSERT_EQUAL(move.payload.assign.new_value, 9);
}

static void test_move_swap_payload_layout(void)
{
    hpp_move move = {
        .kind = MOVE_SWAP,
        .payload.swap =
            {
                .row1 = 1,
                .col1 = 2,
                .row2 = 3,
                .col2 = 4,
            },
    };

    CU_ASSERT_EQUAL(move.kind, MOVE_SWAP);
    CU_ASSERT_EQUAL(move.payload.swap.row1, 1);
    CU_ASSERT_EQUAL(move.payload.swap.col1, 2);
    CU_ASSERT_EQUAL(move.payload.swap.row2, 3);
    CU_ASSERT_EQUAL(move.payload.swap.col2, 4);
}

static void test_progress_snapshot_tracks_latest_move(void)
{
    hpp_solver_progress progress = {
        .sequence        = 99,
        .iteration       = 321,
        .latest_move     = {.kind           = MOVE_ASSIGN,
                            .payload.assign = {.row = 0, .col = 1, .new_value = 4}},
        .elapsed_seconds = 0.125,
    };

    CU_ASSERT_EQUAL(progress.sequence, 99);
    CU_ASSERT_EQUAL(progress.iteration, 321);
    CU_ASSERT_EQUAL(progress.latest_move.kind, MOVE_ASSIGN);
    CU_ASSERT_EQUAL(progress.latest_move.payload.assign.row, 0);
    CU_ASSERT_EQUAL(progress.latest_move.payload.assign.col, 1);
    CU_ASSERT_EQUAL(progress.latest_move.payload.assign.new_value, 4);
    CU_ASSERT_DOUBLE_EQUAL(progress.elapsed_seconds, 0.125, 0.000001);
}

static void test_progress_sink_callback_configuration(void)
{
    hpp_progress_probe       probe = {0};
    hpp_progress_sink_config sink  = {
         .callback         = probe_progress_callback,
         .userdata         = &probe,
         .progress_every_n = 128,
    };

    hpp_solver_progress progress = {
        .sequence  = 12,
        .iteration = 256,
        .latest_move =
            {
                .kind = MOVE_ASSIGN,
                .payload.assign =
                    {
                        .row       = 3,
                        .col       = 3,
                        .old_value = 0,
                        .new_value = 7,
                    },
            },
        .elapsed_seconds = 1.75,
    };

    CU_ASSERT_PTR_NOT_NULL(sink.callback);
    CU_ASSERT_EQUAL(sink.progress_every_n, 128);
    CU_ASSERT_EQUAL(sink.callback(&progress, sink.userdata), 0);

    CU_ASSERT_EQUAL(probe.calls, 1);
    CU_ASSERT_EQUAL(probe.last_sequence, 12);
    CU_ASSERT_EQUAL(probe.last_iteration, 256);
    CU_ASSERT_EQUAL(probe.last_value, 7);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
    {
        return (int)CU_get_error();
    }

    hpp_test_disable_logging();

    CU_pSuite suite = CU_add_suite("Solver Types", NULL, NULL);
    if (suite == NULL)
    {
        CU_cleanup_registry();
        return (int)CU_get_error();
    }

    if (CU_add_test(suite, "move assign payload layout", test_move_assign_payload_layout) == NULL ||
        CU_add_test(suite, "move swap payload layout", test_move_swap_payload_layout) == NULL ||
        CU_add_test(suite,
                    "progress snapshot tracks latest move",
                    test_progress_snapshot_tracks_latest_move) == NULL ||
        CU_add_test(suite,
                    "progress sink callback configuration",
                    test_progress_sink_callback_configuration) == NULL)
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
