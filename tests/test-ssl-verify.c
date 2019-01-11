/*
   Copyright (C) 2018 Red Hat, Inc.

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
#include "common/ssl_verify.c"

static gchar **result_set = NULL;
static gchar **next_result = NULL;
static int entry_count = 0;

// set expected result for next test, these will be checked
// results will be separated by ':' which is not a special character
static void setup_results(const char *results)
{
    g_assert_null(result_set);
    g_assert_null(next_result);
    result_set = g_strsplit_set(results, ":", -1);
    guint len = g_strv_length(result_set);
    g_assert_true(len % 2 == 0);
    next_result = result_set;
    entry_count = len / 2;
}

// cleanup results and prepare for next test
static void tear_results(void)
{
    g_assert_nonnull(next_result);
    g_assert_null(*next_result);
    g_strfreev(result_set);
    result_set = NULL;
    entry_count = 0;
    next_result = NULL;
}

// get next expected value
static const char *get_next_result(void)
{
    g_assert_nonnull(next_result);
    g_assert_nonnull(*next_result);
    return *next_result++;
}

// This override the OpenSSL function
int X509_NAME_add_entry_by_txt(X509_NAME *name, const char *field, int type,
                               const unsigned char *bytes, int len, int loc,
                               int set)
{
    g_assert_nonnull(name);
    g_assert_nonnull(field);
    g_assert_cmpint(type, ==, MBSTRING_UTF8);
    g_assert_nonnull(bytes);
    g_assert_cmpint(len, ==, -1);
    g_assert_cmpint(loc, ==, -1);
    g_assert_cmpint(set, ==, 0);
    g_assert_cmpstr(field, ==, get_next_result());
    g_assert_cmpstr((const char *)bytes, ==, get_next_result());
    return 1;
}

typedef struct {
    const char *input;
    const char *output;
    gboolean success;
} TestGenericParams;

static void test_generic(const void *arg)
{
    const TestGenericParams *params = arg;
    X509_NAME *name;
    int num_entries;

    setup_results(params->output);
    name = subject_to_x509_name(params->input, &num_entries);
    if (params->success) {
        g_assert_cmpint(num_entries, ==, entry_count);
        g_assert_nonnull(name);
        X509_NAME_free(name);
    } else {
        g_assert_null(name);
    }
    tear_results();
}

int main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

#define TEST_SUCCESS(name, input, output) \
    const TestGenericParams test_ ## name = { input, output, TRUE }; \
    g_test_add_data_func("/ssl_verify/" #name, &test_ ## name, test_generic)
#define TEST_ERROR(name, input, output) \
    const TestGenericParams test_ ## name = { input, output, FALSE }; \
    g_test_add_data_func("/ssl_verify/" #name, &test_ ## name, test_generic)

    // normal
    TEST_SUCCESS(easy1, "C=UK", "C:UK");
    TEST_SUCCESS(easy2, "a=b,c=d", "a:b:c:d");

    // check spaces before keys are ignored
    TEST_SUCCESS(space1, "    C=UK", "C:UK");
    TEST_SUCCESS(space2, "C=UK,    A=B", "C:UK:A:B");

    // empty key
    TEST_SUCCESS(empty1, "", "");
    TEST_SUCCESS(empty2, "a=b,", "a:b");
    TEST_SUCCESS(empty3, "   ", "");
    TEST_SUCCESS(empty4, "a=b,  ", "a:b");

    // empty value
    TEST_ERROR(empty5, "a=", "");

    // quoting
    TEST_SUCCESS(quote1, "\\,=a", ",:a");
    TEST_SUCCESS(quote2, "\\\\=a", "\\:a");
    TEST_SUCCESS(quote3, "a=\\,b,c=d", "a:,b:c:d");
    TEST_ERROR(quote4, ",", "");
    TEST_ERROR(quote5, "a\\w=x", "");

    TEST_ERROR(no_value1, "a", "");
    TEST_ERROR(no_value2, "a,b=c", "");

    return g_test_run();
}
