/*
   Copyright (C) 2015 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define G_LOG_DOMAIN "Spice"

#include <glib.h>
#include <stdlib.h>

#include "common/log.h"

#define OTHER_LOG_DOMAIN "Other"
#define LOG_OTHER_HELPER(suffix, level)                                          \
    G_GNUC_PRINTF(1, 2)                                                          \
    static void G_PASTE(other_, suffix)(const gchar *format, ...)                \
    {                                                                            \
        va_list args;                                                            \
                                                                                 \
        va_start (args, format);                                                 \
        g_logv(OTHER_LOG_DOMAIN, G_PASTE(G_LOG_LEVEL_, level), format, args);    \
        va_end (args);                                                           \
    }

/* Set of helpers to log in a different log domain than "Spice" */
LOG_OTHER_HELPER(debug, DEBUG)
LOG_OTHER_HELPER(info, INFO)
LOG_OTHER_HELPER(message, MESSAGE)
LOG_OTHER_HELPER(warning, WARNING)
LOG_OTHER_HELPER(critical, CRITICAL)

/* Checks that spice_warning() aborts after setting G_DEBUG=fatal-warnings */
static void test_spice_fatal_warning(void)
{
    g_setenv("G_DEBUG", "fatal-warnings", TRUE);
    if (g_test_subprocess()) {
        spice_warning("spice_warning");
        return;
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
    g_test_trap_assert_stderr("*spice_warning*");
    g_unsetenv("G_DEBUG");
}

/* Checks that spice_critical() aborts by default */
static void test_spice_fatal_critical(void)
{
    if (g_test_subprocess()) {
        spice_critical("spice_critical");
        return;
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
    g_test_trap_assert_stderr("*spice_critical*");
}

/* Checks that g_critical() does not abort by default */
static void test_spice_non_fatal_g_critical(void)
{
    if (g_test_subprocess()) {
        g_critical("g_critical");
        return;
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_passed();
    g_test_trap_assert_stderr("*g_critical*");
}

/* Checks that g_critical() aborts after setting G_DEBUG=fatal-criticals */
static void test_spice_fatal_g_critical(void)
{
    g_setenv("G_DEBUG", "fatal-criticals", TRUE);
    if (g_test_subprocess()) {
        g_critical("g_critical");
        return;
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
    g_test_trap_assert_stderr("*g_critical*");
    g_unsetenv("G_DEBUG");
}

/* Checks that spice_return_if_fail() aborts by default */
static void test_spice_fatal_return_if_fail(void)
{
    if (g_test_subprocess()) {
        spice_return_if_fail(FALSE);
        return;
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_failed();
    g_test_trap_assert_stderr("*test_spice_fatal_return_if_fail*");
}

/* Checks that g_return_if_fail() does not abort by default */
static void test_spice_non_fatal_g_return_if_fail(void)
{
    if (g_test_subprocess()) {
        g_return_if_fail(FALSE);
        return;
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_passed();
}

/* Checks that spice_*, g_* and other_* (different log domain as g_*) all
 * go through g_log() with the correct domain/log level. This then checks
 * that only logs with level 'message' or higher are shown by default.
 */
static void test_log_levels(void)
{
    if (g_test_subprocess()) {
        g_test_expect_message(G_LOG_DOMAIN,
                              G_LOG_LEVEL_WARNING,
                              "*spice_warning");
        spice_warning("spice_warning");
        g_test_expect_message(G_LOG_DOMAIN,
                              G_LOG_LEVEL_INFO,
                              "*spice_info");
        spice_info("spice_info");
        g_test_expect_message(G_LOG_DOMAIN,
                              G_LOG_LEVEL_DEBUG,
                              "*spice_debug");
        spice_debug("spice_debug");

        g_test_expect_message(G_LOG_DOMAIN,
                              G_LOG_LEVEL_CRITICAL,
                              "*g_critical");
        g_critical("g_critical");
        g_test_expect_message(G_LOG_DOMAIN,
                              G_LOG_LEVEL_WARNING,
                              "*g_warning");
        g_warning("g_warning");
        g_test_expect_message(G_LOG_DOMAIN,
                              G_LOG_LEVEL_MESSAGE,
                              "*g_message");
        g_message("g_message");
        g_test_expect_message(G_LOG_DOMAIN,
                              G_LOG_LEVEL_INFO,
                              "*g_info");
        g_info("g_info");
        g_test_expect_message(G_LOG_DOMAIN,
                              G_LOG_LEVEL_DEBUG,
                              "*g_debug");
        g_debug("g_debug");

        g_test_expect_message(OTHER_LOG_DOMAIN,
                              G_LOG_LEVEL_CRITICAL,
                              "*other_critical");
        other_critical("other_critical");
        g_test_expect_message(OTHER_LOG_DOMAIN,
                              G_LOG_LEVEL_WARNING,
                              "*other_warning");
        other_warning("other_warning");
        g_test_expect_message(OTHER_LOG_DOMAIN,
                              G_LOG_LEVEL_MESSAGE,
                              "*other_message");
        other_message("other_message");
        g_test_expect_message(OTHER_LOG_DOMAIN,
                              G_LOG_LEVEL_INFO,
                              "*other_info");
        other_info("other_info");
        g_test_expect_message(OTHER_LOG_DOMAIN,
                              G_LOG_LEVEL_DEBUG,
                              "*other_debug");
        other_debug("other_debug");

        g_test_assert_expected_messages();


        /* g_test_expected_message only checks whether the appropriate messages got up to g_log()
         * The following calls will be caught by the parent process to check what was (not) printed
         * to stdout/stderr
         */
        spice_warning("spice_warning");
        spice_info("spice_info");
        spice_debug("spice_debug");

        g_critical("g_critical");
        g_warning("g_warning");
        g_message("g_message");
        g_info("g_info");
        g_debug("g_debug");

        other_critical("other_critical");
        other_warning("other_warning");
        other_message("other_message");
        other_info("other_info");
        other_debug("other_debug");

        return;
    }
    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_passed();
    g_test_trap_assert_stderr("*spice_warning\n*g_critical\n*g_warning\n*g_message\n*other_critical\n*other_warning\n*other_message\n");
    g_test_trap_assert_stdout_unmatched("*spice_info*");
    g_test_trap_assert_stdout_unmatched("*spice_debug*");
    g_test_trap_assert_stdout_unmatched("*g_info*");
    g_test_trap_assert_stdout_unmatched("*g_debug*");
    g_test_trap_assert_stdout_unmatched("*other_info*");
    g_test_trap_assert_stdout_unmatched("*other_debug*");
}

/* Checks that setting G_MESSAGES_DEBUG to 'Spice' impacts spice_debug() and
 * g_debug() but not other_debug() */
static void test_spice_g_messages_debug(void)
{
    if (g_test_subprocess()) {
        g_setenv("G_MESSAGES_DEBUG", "Spice", TRUE);

        spice_debug("spice_debug");
        spice_info("spice_info");
        g_debug("g_debug");
        g_info("g_info");
        g_message("g_message");
        other_debug("other_debug");
        other_info("other_info");
        other_message("other_message");

        return;
    }

    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_passed();
    g_test_trap_assert_stdout("*spice_debug\n*spice_info\n*g_debug\n*g_info\n");
    g_test_trap_assert_stderr("*g_message\n*other_message\n");
    g_test_trap_assert_stdout_unmatched("*other_debug*");
    g_test_trap_assert_stdout_unmatched("*other_info*");
}

/* Checks that setting G_MESSAGES_DEBUG to 'all' impacts spice_debug(),
 * g_debug() and other_debug() */
static void test_spice_g_messages_debug_all(void)
{
    if (g_test_subprocess()) {
        g_setenv("G_MESSAGES_DEBUG", "all", TRUE);

        spice_debug("spice_debug");
        spice_info("spice_info");
        g_debug("g_debug");
        g_info("g_info");
        g_message("g_message");
        other_debug("other_debug");
        other_info("other_info");
        other_message("other_message");

        return;
    }

    g_test_trap_subprocess(NULL, 0, 0);
    g_test_trap_assert_passed();
    g_test_trap_assert_stdout("*spice_debug\n*spice_info\n*g_debug\n*g_info\n*other_debug\n*other_info\n");
    g_test_trap_assert_stderr("*g_message\n*other_message\n");
}

static void handle_sigabrt(int sig G_GNUC_UNUSED)
{
    _Exit(1);
}

int main(int argc, char **argv)
{
    GLogLevelFlags fatal_mask;

    /* prevents core generations as this could cause some issues/timeout
     * depending on system configuration */
    signal(SIGABRT, handle_sigabrt);

    fatal_mask = (GLogLevelFlags)g_log_set_always_fatal((GLogLevelFlags) G_LOG_FATAL_MASK);

    g_test_init(&argc, &argv, NULL);

    /* Reset fatal mask set by g_test_init() as we don't want
     * warnings/criticals to be fatal by default since this is what some of the
     * test cases are going to test */
    g_log_set_always_fatal(fatal_mask & G_LOG_LEVEL_MASK);

    g_test_add_func("/spice-common/spice-g-messages-debug", test_spice_g_messages_debug);
    g_test_add_func("/spice-common/spice-g-messages-debug-all", test_spice_g_messages_debug_all);
    g_test_add_func("/spice-common/spice-log-levels", test_log_levels);
    g_test_add_func("/spice-common/spice-fatal-critical", test_spice_fatal_critical);
    g_test_add_func("/spice-common/spice-non-fatal-gcritical", test_spice_non_fatal_g_critical);
    g_test_add_func("/spice-common/spice-fatal-gcritical", test_spice_fatal_g_critical);
    g_test_add_func("/spice-common/spice-fatal-return-if-fail", test_spice_fatal_return_if_fail);
    g_test_add_func("/spice-common/spice-non-fatal-greturn-if-fail", test_spice_non_fatal_g_return_if_fail);
    g_test_add_func("/spice-common/spice-fatal-warning", test_spice_fatal_warning);

    return g_test_run();
}
