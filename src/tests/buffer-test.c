/*
 * Copyright (c) 2015, Krisztian Litkey, <kli@iki.fi>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <check.h>

#include <murphy/common/log.h>
#include <murphy/common/debug.h>
#include <ripncode/ripncode.h>

#define REQUIRE(name) name(_i)

static rnc_buf_t *b = NULL;

START_TEST(mem_create)
{
    b = rnc_buf_create("test buffer", 0, 32);

    ck_assert_ptr_ne(b, NULL);
}
END_TEST

START_TEST(mem_close)
{
    REQUIRE(mem_create);

    ck_assert_int_eq(rnc_buf_close(b), 0);

    b = NULL;
}
END_TEST

START_TEST(mem_unlink)
{
    REQUIRE(mem_create);

    ck_assert_int_eq(rnc_buf_unlink(b), 0);

    b = NULL;
}
END_TEST

START_TEST(file_open)
{
    b = rnc_buf_open("/tmp/test.buf", O_RDWR | O_CREAT, 0644);

    ck_assert_ptr_ne(b, NULL);
}
END_TEST

START_TEST(file_close)
{
    REQUIRE(file_open);

    ck_assert_int_eq(rnc_buf_close(b), 0);

    b = NULL;
}
END_TEST

START_TEST(file_unlink)
{
    REQUIRE(file_open);

    ck_assert_int_eq(rnc_buf_unlink(b), 0);

    b = NULL;
}
END_TEST


char pattern[] =                                                \
    "00000000001111111111222222222233333333334444444444"        \
    "55555555556666666666777777777788888888889999999999"        \
    "abcdefghijklmnopqrstuvxyzABCDEFGHIJKLMNOPQRSTUVXYZ";
char none[] =                                                   \
    ".................................................."        \
    ".................................................."        \
    "..................................................";

START_TEST(mem_seqwrite)
{
    int i, len;

    REQUIRE(mem_create);

    len = sizeof(pattern) - 1;
    for (i = 0; i < 50; i++)
        ck_assert_int_eq(rnc_buf_write(b, pattern, len), len);
}
END_TEST

START_TEST(mem_seqread)
{
    char c;
    int  i, len, n;

    REQUIRE(mem_seqwrite);

    len = sizeof(pattern) - 1;
    for (i = 0; i < 50 * len; i++) {
        n = rnc_buf_read(b, &c, 1);

        ck_assert_int_eq(n, 1);
        ck_assert_int_eq(c, pattern[i % len]);
    }
}
END_TEST

START_TEST(mem_rndwrite)
{
    int i, len, n;
    off_t ooffs, coffs;

    REQUIRE(mem_create);

    len = sizeof(pattern) - 1;

    for (i = 0; i < 50; i++) {
        if (!(i & 0x1))
            n = rnc_buf_write(b, pattern, len);
        else
            n = rnc_buf_write(b, none, len);

        ck_assert_int_eq(n, len);

        if (i & 0x1) {
            ooffs = rnc_buf_wseek(b, 0, SEEK_CUR);
            /*printf("ooffs = %ld\n", ooffs);*/
            coffs = rnc_buf_wseek(b, -len, SEEK_CUR);
            ck_assert_int_eq(coffs, ooffs - len);
            ck_assert_int_eq(rnc_buf_write(b, pattern, len), len);

        }
    }
}
END_TEST

START_TEST(mem_rndread)
{
    int len, i, n, c;

    REQUIRE(mem_rndwrite);

    len = sizeof(pattern) - 1;

    {
        int fd;

        fd = open("/tmp/buffer-test.dump", O_WRONLY | O_CREAT, 0644);

        if (fd != -1) {
            char buf[len * 50];
            rnc_buf_read(b, buf, sizeof(buf));
            write(fd, buf, sizeof(buf));
            rnc_buf_rseek(b, 0, SEEK_SET);
            close(fd);
        }
    }

    c = 0;
    for (i = 0; i < 50 * len; i++) {
        n = rnc_buf_read(b, &c, 1);

        ck_assert_int_eq(n, 1);
        ck_assert_int_eq(c, pattern[i % len]);
    }
}
END_TEST


void basic_tests(Suite *s)
{
    TCase *c;

    c = tcase_create("Basic Open/Close/Unlink Tests");

    tcase_add_test(c, mem_create);
    tcase_add_test(c, mem_close);
    tcase_add_test(c, mem_unlink);
    tcase_add_test(c, file_open);
    tcase_add_test(c, file_close);
    tcase_add_test(c, file_unlink);

    suite_add_tcase(s, c);
}


void sequential_tests(Suite *s)
{
    TCase *c;

    c = tcase_create("Sequential Read/Write Tests");

    tcase_add_test(c, mem_seqwrite);
    tcase_add_test(c, mem_seqread);

    suite_add_tcase(s, c);
}


void randomaccess_tests(Suite *s)
{
    TCase *c;

    c = tcase_create("Random access Read/Write Tests");

    tcase_add_test(c, mem_rndwrite);
    tcase_add_test(c, mem_rndread);

    suite_add_tcase(s, c);
}


int main(int argc, char *argv[])
{
    Suite   *s;
    SRunner *r;
    int      f, i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d") && i < argc - 1) {
            mrp_log_set_mask(MRP_LOG_UPTO(MRP_LOG_WARNING) | MRP_LOG_MASK_DEBUG);
            mrp_debug_set(argv[i + 1]);
            mrp_debug_enable(TRUE);
        }
    }

    s = suite_create("Buffer");
    r = srunner_create(s);

    basic_tests(s);
    sequential_tests(s);
    randomaccess_tests(s);

    srunner_run_all(r, CK_NORMAL);
    f = srunner_ntests_failed(r);
    srunner_free(r);

    exit(f == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
