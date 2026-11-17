/**
 * @file test_batterymonitor_review.c
 * @brief Host tests for review points 6-8 of PR #332 in src/batteryMonitorUI/batteryMonitorUI.c.
 *
 * The app cannot be included on the host (SDL, Miyoo headers), so the Makefile cuts the functions
 * under test and their constants out of the real source into REVIEW_FNS.
 *
 *   6. the newest entry's live time leaves a sleep out;
 *   7. only SIGCONT (or a very long gap) is a wake, not a slow pass;
 *   8. the battery test's saved state is read back as written, and a bad one is not used.
 *
 * Build and run: make -f Makefile.unit test_batterymonitor_review
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include REVIEW_FNS

/* ---- 6. Session time of the newest entry ---- */

TEST(live_time_adds_what_batmon_has_not_written)
{
    /* awake 5 min since batmon last wrote the log: the entry is 5 min longer */
    ASSERT_EQ(live_duration(1200, 1794900000LL, 1794900000LL - 300), 1500);
    ASSERT_EQ(live_duration(0, 1794900000LL, 1794900000LL - 1), 1);
}

TEST(live_time_leaves_a_long_sleep_out)
{
    /* 8 h asleep since batmon last wrote: at most what batmon can hold unwritten is added,
     * not the 8 h (the clock since the entry began counted all of it) */
    ASSERT_EQ(live_duration(1200, 1794900000LL, 1794900000LL - 8 * 3600), 1200 + BATMON_MAX_UNWRITTEN_S);
    /* days switched off */
    ASSERT_EQ(live_duration(60, 1794900000LL, 1794900000LL - 5 * 86400), 60 + BATMON_MAX_UNWRITTEN_S);
    /* the cap is above batmon's 600 s update period */
    ASSERT_TRUE(BATMON_MAX_UNWRITTEN_S >= 600);
}

TEST(live_time_keeps_the_written_duration_on_a_bad_clock)
{
    ASSERT_EQ(live_duration(1200, 1794900000LL, 0), 1200);           /* no file time */
    ASSERT_EQ(live_duration(1200, 1794900000LL, 1794900000LL), 1200); /* just written */
    ASSERT_EQ(live_duration(1200, 1000LL, 1794900000LL), 1200);       /* clock behind the file */
}

/* ---- 7. Wake from sleep ---- */

TEST(sigcont_is_a_wake)
{
    ASSERT_TRUE(is_wake(1, 0));
    ASSERT_TRUE(is_wake(1, 16));
}

TEST(a_slow_pass_is_not_a_wake)
{
    /* a pass of a few seconds (database, cpuclock, the battery test's load) aborted the test */
    ASSERT_FALSE(is_wake(0, 16));
    ASSERT_FALSE(is_wake(0, 2500));
    ASSERT_FALSE(is_wake(0, 10000));
    ASSERT_TRUE(WAKE_GAP_MS >= 10000);
}

TEST(a_long_gap_is_a_wake_when_sigcont_was_missed)
{
    ASSERT_TRUE(is_wake(0, WAKE_GAP_MS + 1));
    ASSERT_TRUE(is_wake(0, 8 * 3600 * 1000));
}

/* ---- 8. Battery test killed ---- */

TEST(test_state_is_read_back_as_saved)
{
    char line[64], gov[32];
    int mhz = 0;
    unsigned brightness = 0;
    bt_format_state(line, sizeof(line), 1200, "ondemand", 37);
    ASSERT_EQ(bt_parse_state(line, &mhz, gov, sizeof(gov), &brightness), 0);
    ASSERT_EQ(mhz, 1200);
    ASSERT_STREQ(gov, "ondemand");
    ASSERT_EQ((int)brightness, 37);
}

TEST(test_state_without_a_governor)
{
    char line[64], gov[32] = "x";
    int mhz = 0;
    unsigned brightness = 0;
    bt_format_state(line, sizeof(line), 1000, "", 3);
    ASSERT_EQ(bt_parse_state(line, &mhz, gov, sizeof(gov), &brightness), 0);
    ASSERT_STREQ(gov, "");
    ASSERT_EQ((int)brightness, 3);
}

TEST(a_bad_test_state_is_not_used)
{
    char gov[32];
    int mhz;
    unsigned brightness;
    ASSERT_EQ(bt_parse_state("", &mhz, gov, sizeof(gov), &brightness), -1);
    ASSERT_EQ(bt_parse_state("1200 ondemand", &mhz, gov, sizeof(gov), &brightness), -1);
    ASSERT_EQ(bt_parse_state("-1 ondemand 37", &mhz, gov, sizeof(gov), &brightness), -1); /* cpuclock failed */
    ASSERT_EQ(bt_parse_state("99999 ondemand 37", &mhz, gov, sizeof(gov), &brightness), -1);
}

int main(void)
{
    printf("\n=== batteryMonitorUI review 6-8 tests ===\n\n");
    RUN_TEST(live_time_adds_what_batmon_has_not_written);
    RUN_TEST(live_time_leaves_a_long_sleep_out);
    RUN_TEST(live_time_keeps_the_written_duration_on_a_bad_clock);
    RUN_TEST(sigcont_is_a_wake);
    RUN_TEST(a_slow_pass_is_not_a_wake);
    RUN_TEST(a_long_gap_is_a_wake_when_sigcont_was_missed);
    RUN_TEST(test_state_is_read_back_as_saved);
    RUN_TEST(test_state_without_a_governor);
    RUN_TEST(a_bad_test_state_is_not_used);
    TEST_REPORT();
    return test_failures;
}
