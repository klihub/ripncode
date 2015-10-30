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

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#define __GNU_SOURCE
#include <getopt.h>

#include <ripncode/ripncode.h>
#include <ripncode/setup.h>


static const char *argv0_base(const char *argv0)
{
    const char *base;

    base = strrchr(argv0, '/');

    if (base == NULL)
        return argv0;

    base++;
    if (!strncmp(base, "lt-", 3))
        return base + 3;
    else
        return base;
}


static void print_usage(rnc_t *rnc, int exit_code, const char *fmt, ...)
{
    va_list     ap;
    const char *base;

    if (fmt && *fmt) {
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf("\n");
    }

    base = argv0_base(rnc->argv0);

    printf("usage: %s [options] <input> [<output>]\n", base);
    printf("The possible options are:\n");
    printf("  -d, --driver=<DRIVER>        use <DRIVER> to open <input>\n"
           "  -s, --speed=<SPEEDT>         device speed\n"
           "  -o, --output=<FORMAT>        encode to <FORMAT> in <output>\n"
           "  -t, --tracks=<FIRST[-LAST]>  ripncode given tracks\n"
           "  -m, --metadata=<FILE>        read album metadata from <FILE>\n"
           "  -p, --pattern=<PATTERN>      tracks naming <PATTERN>\n"
           "  -L, --log-level=<LEVELS>     what messages to log\n"
           "  -v, --verbose                increase logging verbosity\n"
           "  -T, --log-target=<TARGET>    where to log messages to \n"
           "  -D, --debug=<SITE>           enable <SITE> for debugging\n"
           "  -h, --help                   show help on usage\n");

    if (exit_code < 0)
        return;
    else
        exit(exit_code);
}


static void setup_defaults(rnc_t *rnc, const char *argv0)
{
    rnc->argv0      = argv0;
    rnc->device     = "/dev/cdrom";
    rnc->speed      = 0;
    rnc->log_mask   = MRP_LOG_UPTO(MRP_LOG_WARNING);
    rnc->log_target = "stdout";

    rnc->rip    = "all";
    rnc->output = "track";
    rnc->format = "flac";

    mrp_log_set_mask(rnc->log_mask);
    mrp_log_set_target(rnc->log_target);
}


void rnc_cmdline_parse(rnc_t *rnc, int argc, char **argv, char **envp)
{
#   define OPTIONS "d:s:o:f:t:m:p:L:vT:D:n:h"
    struct option options[] = {
        { "driver"           , required_argument, NULL, 'd' },
        { "speed"            , required_argument, NULL, 's' },
        { "output"           , required_argument, NULL, 'o' },
        { "format"           , required_argument, NULL, 'f' },
        { "tracks"           , required_argument, NULL, 't' },
        { "metadata"         , required_argument, NULL, 'm' },
        { "pattern"          , required_argument, NULL, 'p' },
        { "log-level"        , required_argument, NULL, 'L' },
        { "verbose"          , no_argument      , NULL, 'v' },
        { "log-target"       , required_argument, NULL, 'T' },
        { "debug"            , required_argument, NULL, 'D' },
        { "help"             , no_argument      , NULL, 'h' },
        { NULL, 0, NULL, 0 },
    };
    int   opt, help, dbg;
    char *e;

    MRP_UNUSED(envp);

    help = FALSE;
    setup_defaults(rnc, argv[0]);

    while ((opt = getopt_long(argc, argv, OPTIONS, options, NULL)) != -1) {
        switch (opt) {
        case 'd':
            rnc->driver = optarg;
            break;

        case 's':
            rnc->speed = strtoul(optarg, &e, 10);
            if (e && *e)
                print_usage(rnc, EINVAL, "invalid speed '%s'", optarg);
            break;

        case 'o':
            rnc->output = optarg;
            break;

        case 'f':
            rnc->format = optarg;
            break;

        case 't':
            rnc->rip = optarg;
            break;

        case 'm':
            rnc->metadata = optarg;
            break;

        case 'p':
            rnc->pattern = optarg;
            break;

        case 'L':
            dbg = mrp_log_enable(0) & MRP_LOG_MASK_DEBUG;
            rnc->log_mask = mrp_log_parse_levels(optarg);
            mrp_log_set_mask(rnc->log_mask | dbg);
            break;

        case 'v':
            rnc->log_mask <<= 1;
            rnc->log_mask  |= 1;
            mrp_log_set_mask(rnc->log_mask);
            break;

        case 'T':
            rnc->log_target = optarg;
            mrp_log_set_target(rnc->log_target);
            break;

        case 'D':
            rnc->log_mask |= MRP_LOG_MASK_DEBUG;
            mrp_log_set_mask(rnc->log_mask);
            mrp_debug_enable(TRUE);
            mrp_debug_set(optarg);
            break;

        case 'n':
            rnc->dry_run = 1;
            break;

        case 'h':
            help++;
            break;

        default:
            print_usage(rnc, EINVAL, "invalid option '%c'", opt);
        }
    }

    if (help) {
        print_usage(rnc, -1, "");
        exit(0);
    }

    if (optind == argc - 2) {
        rnc->device = argv[optind++];
        rnc->output = argv[optind];
    }
    else if (optind == argc - 1)
        rnc->device = argv[optind];
    else
        print_usage(rnc, EINVAL, "need an input an an optional output");
}
