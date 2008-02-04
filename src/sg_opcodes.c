/* A utility program originally written for the Linux OS SCSI subsystem.
 *  Copyright (C) 2004-2007 D. Gilbert
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.

    This program outputs information provided by a SCSI REPORT SUPPORTED
    OPERATION CODES [0xa3/0xc] and REPORT SUPPORTED TASK MANAGEMENT
    FUNCTIONS [0xa3/0xd] commands.
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "sg_lib.h"
#include "sg_cmds_basic.h"
#include "sg_cmds_extra.h"

static char * version_str = "0.30 20070714";    /* spc4r11 */

// #define USE_LINUX_SG_IO_IF 1

/* Notes:
 *  - this file has both Linux specific pass through code using the
 *    SG_IO ioctl and a more generic sg_pt mechanism that is portable
 *    to other OSes. The code is conditionally compiled depending
 *    on the USE_LINUX_SG_IO_IF define and whether the Makefile
 *    indicates the OS is Linux.
 *    N.B. Various Makefiles are set assuming this is not defined.
 *
 *  - since support for the SCSI REPORT SUPPORTED OPERATION CODES and
 *    REPORT SUPPORTED TASK MANAGEMENT FUNCTIONS is uncommon, dummy
 *    response code is provided. Uncomment the '#define TEST_CODE'
 *    line for test mode.
 */

#if defined(USE_LINUX_SG_IO_IF) && defined(SG3_UTILS_LINUX)
  #define USE_SG_IO
#endif

#ifdef USE_SG_IO
  #include <sys/ioctl.h>
  #include <sys/types.h>
  #include <sys/stat.h>

  #include "sg_io_linux.h"
  #define EBUFF_SZ 256
  static char ebuff[EBUFF_SZ];
#else
  #include "sg_pt.h"
#endif

#define SENSE_BUFF_LEN 32       /* Arbitrary, could be larger */
#define DEF_TIMEOUT 60000       /* 60,000 millisecs == 60 seconds */

#define SG_MAINTENANCE_IN 0xa3
#define RSOC_SA     0xc
#define RSTMF_SA    0xd
#define RSOC_CMD_LEN 12
#define RSTMF_CMD_LEN 12
#define MX_ALLOC_LEN 8192

#define NAME_BUFF_SZ 64


static int peri_type = 0; /* ugly but not easy to pass to alpha compare */

static int do_rsoc(int sg_fd, int rctd, int rep_opts, int rq_opcode,
                   int rq_servact, void * resp, int mx_resp_len, int noisy,
                   int verbose);
static int do_rstmf(int sg_fd, void * resp, int mx_resp_len, int noisy,
                    int verbose);

/* <<<<<<<<<<<<<<< start of test code */
// #define TEST_CODE

#ifdef TEST_CODE

#warning "<<<< TEST_CODE response compiled in >>>>"

#define DUMMY_CMDS 17
#define DUMMY_CMD_LEN 8
#define DUMMY_TO_CMDS 4
#define DUMMY_TO_CMD_LEN 20

struct cmd_descript_t {
    unsigned char d[DUMMY_CMD_LEN];
};

struct dummy_resp_t {
    unsigned char cdl[4];
    struct cmd_descript_t descript[DUMMY_CMDS];
};

static struct dummy_resp_t dummy_resp = {
    {0, 0, 0, DUMMY_CMD_LEN * DUMMY_CMDS},
     {{{0, 0, 0, 0, 0, 0, 0, 6}},               /* tur */
      {{0xa3, 0, 0, 0xc, 0, 1, 0, 12}},         /* rsoc */
      {{0x12, 0, 0, 0, 0, 0, 0, 6}},            /* inq */
      {{0x1d, 0, 0, 0, 0, 0, 0, 6}},            /* sd */
      {{0x25, 0, 0, 0, 0, 0, 0, 10}},           /* rc */
      {{0x28, 0, 0, 0, 0, 0, 0, 10}},           /* r(10) */
      {{0x2a, 0, 0, 0, 0, 0, 0, 10}},           /* w(10) */
      {{0x1a, 0, 0, 0, 0, 0, 0, 6}},            /* ms(6) */
      {{0x15, 0, 0, 0, 0, 0, 0, 6}},            /* msel(6) */
      {{0xa3, 0, 0, 0x5, 0, 1, 0, 12}},         /* rii */
      {{0x5a, 0, 0, 0, 0, 0, 0, 10}},           /* ms(10) */
      {{0x55, 0, 0, 0, 0, 0, 0, 10}},           /* msel(10) */
      {{2, 0, 0, 0, 0, 0, 0, 6}},               /* ?? */
      {{3, 0, 0, 0, 0, 0, 0, 6}},               /* rs */
      {{4, 0, 0, 0, 0, 0, 0, 6}},               /* f */
      {{0xa0, 0, 0, 0, 0, 0, 0, 12}},           /* rl */
      {{0x7f, 0, 0, 0x3, 0, 1, 0, 32}},         /* vl:xdr(32) */
}};

struct cmd_descript_to_t {
    unsigned char d[DUMMY_TO_CMD_LEN];
};

struct dummy_resp_to_t {
    unsigned char cdl[4];
    struct cmd_descript_to_t descript[DUMMY_TO_CMDS];
};

static struct dummy_resp_to_t dummy_to_resp =
    {{0, 0, 0, DUMMY_TO_CMD_LEN * DUMMY_TO_CMDS},
     {{{0, 0, 0, 0, 0, 0x2, 0, 6,
        0, 0xa, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5}},         /* tur */
      {{0xa3, 0, 0, 0xc, 0, 0x3, 0, 12,
        0, 0xa, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6}},         /* rsoc */
      {{4, 0, 0, 0, 0, 0x2, 0, 6,
        0, 0xa, 0, 0, 0, 0, 0x8, 0, 0, 0, 0x10, 0}},    /* f */
      {{0x7f, 0, 0, 0x3, 0, 0x3, 0, 32,
        0, 0xa, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7}},         /* vl:xdr(32) */
}};

static unsigned char dummy_1_cmd[] = {
    0, 3, 0, 6, 0x12, 0x3, 0xff, 0x0, 0xff, 0x1
};

static unsigned char dummy_1_to_cmd[] = {
    0, 0x83, 0, 6, 0x12, 0x3, 0xff, 0x0, 0xff, 0x1,
        0, 0xa, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7,           /* inq */
};

static unsigned char dummy_rsmft_r0 = 0xff;
static unsigned char dummy_rsmft_r1 = 0x1;

#endif
/* <<<<<<<<<<<<<<< end of test code */

static struct option long_options[] = {
        {"alpha", 0, 0, 'a'},
        {"help", 0, 0, 'h'},
        {"hex", 0, 0, 'H'},
        {"new", 0, 0, 'N'},
        {"opcode", 1, 0, 'o'},
        {"old", 0, 0, 'O'},
        {"raw", 0, 0, 'r'},
        {"rctd", 0, 0, 'R'},
        {"sa", 1, 0, 's'},
        {"tmf", 0, 0, 't'},
        {"unsorted", 0, 0, 'u'},
        {"verbose", 0, 0, 'v'},
        {"version", 0, 0, 'V'},
        {0, 0, 0, 0},
};

struct opts_t {
    int do_alpha;
    int do_help;
    int do_hex;
    int do_opcode;
    int do_raw;
    int do_rctd;
    int do_servact;
    int do_verbose;
    int do_version;
    int do_unsorted;
    int do_taskman;
    const char * device_name;
    int opt_new;
};

static void usage()
{
    fprintf(stderr,
            "Usage:  sg_opcodes [--alpha] [--help] [--hex] [--opcode=OP] "
            "[--raw] [--rctd]\n"
            "                   [--sa=SA] [--tmf] [--unsorted] [--verbose] "
            "[--version]\n"
            "                   DEVICE\n"
            "  where:\n"
            "    --alpha|-a      output list of operation codes sorted "
            "alphabetically\n"
            "    --help|-h       print usage message then exit\n"
            "    --hex|-H        output response in hex\n"
            "    --opcode=OP|-o OP    first byte of command to query\n"
            "                         (decimal, prefix with '0x' for hex)\n"
            "    --raw|-r        output response in binary to stdout\n"
            "    --rctd|-R       set RCTD (return command timeout "
            "descriptor) bit\n"
            "    --sa=SA|-s SA    service action in addition to opcode\n"
            "                     (decimal, prefix with '0x' for hex)\n"
            "    --tmf|-t        output list of supported task management "
            "functions\n"
            "    --unsorted|-u    output list of operation codes as is "
            "(unsorted)\n"
            "    --verbose|-v    increase verbosity\n"
            "    --version|-V    print vesrion string then exit\n\n"
            "Performs a SCSI REPORT SUPPORTED OPERATION CODES or REPORT "
            "SUPPORTED\nTASK MANAGEMENT FUNCTIONS command\n");
}

static void usage_old()
{
    fprintf(stderr,
            "Usage:  sg_opcodes [-a] [-H] [-o=OP] [-r] [-R] [-s=SA]"
            " [-t] [-u]\n"
            "                   [-v] [-V] DEVICE\n"
            "  where:\n"
            "    -a    output list of operation codes sorted "
            "alphabetically\n"
            "    -H    print response in hex\n"
            "    -o=OP    first byte of command to query (in hex)\n"
            "    -r    output response in binary to stdout\n"
            "    -R    set RCTD (return command timeout "
            "descriptor) bit\n"
            "    -s=SA    in addition to opcode (in hex)\n"
            "    -t    output list of supported task management functions\n"
            "    -u    output list of operation codes as is (unsorted)\n"
            "    -v    verbose\n"
            "    -V    output version string\n"
            "    -?    output this usage message\n\n"
            "Performs a SCSI REPORT SUPPORTED OPERATION CODES (or REPORT "
            "TASK MANAGEMENT\nFUNCTIONS) command\n");
}

static int process_cl_new(struct opts_t * optsp, int argc, char * argv[])
{
    int c, n;

    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "ahHNo:OrRs:tuvV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'a':
            optsp->do_alpha = 1;
            break;
        case 'h':
        case '?':
            ++optsp->do_help;
            break;
        case 'H':
            ++optsp->do_hex;
            break;
        case 'N':
            break;      /* ignore */
        case 'o':
            n = sg_get_num(optarg);
            if ((n < 0) || (n > 255)) {
                fprintf(stderr, "bad argument to '--opcode'\n");
                usage();
                return SG_LIB_SYNTAX_ERROR;
            }
            optsp->do_opcode = n;
            break;
        case 'O':
            optsp->opt_new = 0;
            return 0;
        case 'r':
            ++optsp->do_raw;
            break;
        case 'R':
            ++optsp->do_rctd;
            break;
        case 's':
           n = sg_get_num(optarg);
           if (n < 0) {
                fprintf(stderr, "bad argument to '--sa'\n");
                usage();
                return SG_LIB_SYNTAX_ERROR;
            }
            optsp->do_servact = n;
            break;
        case 't':
            ++optsp->do_taskman;
            break;
        case 'u':
            ++optsp->do_unsorted;
            break;
        case 'v':
            ++optsp->do_verbose;
            break;
        case 'V':
            ++optsp->do_version;
            break;
        default:
            fprintf(stderr, "unrecognised option code %c [0x%x]\n", c, c);
            if (optsp->do_help)
                break;
            usage();
            return SG_LIB_SYNTAX_ERROR;
        }
    }
    if (optind < argc) {
        if (NULL == optsp->device_name) {
            optsp->device_name = argv[optind];
            ++optind;
        }
        if (optind < argc) {
            for (; optind < argc; ++optind)
                fprintf(stderr, "Unexpected extra argument: %s\n",
                        argv[optind]);
            usage();
            return SG_LIB_SYNTAX_ERROR;
        }
    }
    return 0;
}

static int process_cl_old(struct opts_t * optsp, int argc, char * argv[])
{
    int k, jmp_out, plen, n, num;
    const char * cp;

    for (k = 1; k < argc; ++k) {
        cp = argv[k];
        plen = strlen(cp);
        if (plen <= 0)
            continue;
        if ('-' == *cp) {
            for (--plen, ++cp, jmp_out = 0; plen > 0; --plen, ++cp) {
                switch (*cp) {
                case 'a':
                    ++optsp->do_alpha;
                    break;
                case 'H':
                    ++optsp->do_hex;
                    break;
                case 'N':
                    optsp->opt_new = 1;
                    return 0;
                case 'O':
                    break;
                case 'R':
                    ++optsp->do_rctd;
                    break;
                case 't':
                    ++optsp->do_taskman;
                    break;
                case 'u':
                    ++optsp->do_unsorted;
                    break;
                case 'v':
                    ++optsp->do_verbose;
                    break;
                case 'V':
                    ++optsp->do_version;
                    break;
                case 'h':
                case '?':
                    ++optsp->do_help;
                    break;
                default:
                    jmp_out = 1;
                    break;
                }
                if (jmp_out)
                    break;
            }
            if (plen <= 0)
                continue;
            if (0 == strncmp("o=", cp, 2)) {
                num = sscanf(cp + 2, "%x", (unsigned int *)&n);
                if ((1 != num) || (n > 255)) {
                    fprintf(stderr, "Bad number after 'o=' option\n");
                    usage_old();
                    return SG_LIB_SYNTAX_ERROR;
                }
                optsp->do_opcode = n;
            } else if (0 == strncmp("s=", cp, 2)) {
                num = sscanf(cp + 2, "%x", (unsigned int *)&n);
                if (1 != num) {
                    fprintf(stderr, "Bad number after 's=' option\n");
                    usage_old();
                    return SG_LIB_SYNTAX_ERROR;
                }
                optsp->do_servact = n;
            } else if (0 == strncmp("-old", cp, 4))
                ;
            else if (jmp_out) {
                fprintf(stderr, "Unrecognized option: %s\n", cp);
                usage_old();
                return SG_LIB_SYNTAX_ERROR;
            }
        } else if (NULL == optsp->device_name)
            optsp->device_name = cp;
        else {
            fprintf(stderr, "too many arguments, got: %s, not expecting: "
                    "%s\n", optsp->device_name, cp);
            usage_old();
            return SG_LIB_SYNTAX_ERROR;
        }
    }
    return 0;
}

static int process_cl(struct opts_t * optsp, int argc, char * argv[])
{
    int res;
    char * cp;

    cp = getenv("SG3_UTILS_OLD_OPTS");
    if (cp) {
        optsp->opt_new = 0;
        res = process_cl_old(optsp, argc, argv);
        if ((0 == res) && optsp->opt_new)
            res = process_cl_new(optsp, argc, argv);
    } else {
        optsp->opt_new = 1;
        res = process_cl_new(optsp, argc, argv);
        if ((0 == res) && (0 == optsp->opt_new))
            res = process_cl_old(optsp, argc, argv);
    }
    return res;
}

static void dStrRaw(const char* str, int len)
{
    int k;

    for (k = 0 ; k < len; ++k)
        printf("%c", str[k]);
}

/* returns -1 when left < right, 0 when left == right, else returns 1 */
static int opcode_num_compare(const void * left, const void * right)
{
    const unsigned char * ll = *(unsigned char **)left;
    const unsigned char * rr = *(unsigned char **)right;
    int l_serv_act = 0;
    int r_serv_act = 0;
    int l_opc, r_opc;

    if (NULL == ll)
        return -1;
    if (NULL == rr)
        return -1;
    l_opc = ll[0];
    if (ll[5] & 1)
        l_serv_act = ((ll[2] << 8) | ll[3]);
    r_opc = rr[0];
    if (rr[5] & 1)
        r_serv_act = ((rr[2] << 8) | rr[3]);
    if (l_opc < r_opc)
        return -1;
    if (l_opc > r_opc)
        return 1;
    if (l_serv_act < r_serv_act)
        return -1;
    if (l_serv_act > r_serv_act)
        return 1;
    return 0;
}

/* returns -1 when left < right, 0 when left == right, else returns 1 */
static int opcode_alpha_compare(const void * left, const void * right)
{
    const unsigned char * ll = *(unsigned char **)left;
    const unsigned char * rr = *(unsigned char **)right;
    int l_serv_act = 0;
    int r_serv_act = 0;
    char l_name_buff[NAME_BUFF_SZ];
    char r_name_buff[NAME_BUFF_SZ];
    int l_opc, r_opc;

    if (NULL == ll)
        return -1;
    if (NULL == rr)
        return -1;
    l_opc = ll[0];
    if (ll[5] & 1)
        l_serv_act = ((ll[2] << 8) | ll[3]);
    l_name_buff[0] = '\0';
    sg_get_opcode_sa_name(l_opc, l_serv_act, peri_type,
                          NAME_BUFF_SZ, l_name_buff);
    r_opc = rr[0];
    if (rr[5] & 1)
        r_serv_act = ((rr[2] << 8) | rr[3]);
    r_name_buff[0] = '\0';
    sg_get_opcode_sa_name(r_opc, r_serv_act, peri_type,
                          NAME_BUFF_SZ, r_name_buff);
    return strncmp(l_name_buff, r_name_buff, NAME_BUFF_SZ);
}

static void list_all_codes(unsigned char * rsoc_buff, int rsoc_len,
                           int unsorted, int alpha, int rctd)
{
    int k, j, cd_len, serv_act, len;
    unsigned long to;
    unsigned char * ucp;
    char name_buff[NAME_BUFF_SZ];
    char sa_buff[8];
    unsigned char ** sort_arr = NULL;

    cd_len = ((rsoc_buff[0] << 24) | (rsoc_buff[1] << 16) | 
              (rsoc_buff[2] << 8) | rsoc_buff[3]); 
    if (cd_len > (rsoc_len - 4)) {
        printf("sg_opcodes: command data length=%d, allocation=%d; "
               "truncate\n", cd_len, rsoc_len - 4);
        cd_len = ((rsoc_len - 4) / 8) * 8;
    }
    if (0 == cd_len) {
        printf("sg_opcodes: no commands to display\n");
        return;
    }
    if (rctd) {
        printf("\nOpcode  Service    CDB   Nominal  Recommended  Name\n");
        printf(  "(hex)   action(h)  size  timeout  timeout(sec)     \n");
        printf("-----------------------------------------------------------"
               "-----\n");
    } else {
        printf("\nOpcode  Service    CDB    Name\n");
        printf(  "(hex)   action(h)  size       \n");
        printf("-----------------------------------------------\n");
    }
    /* N.B. SPC-4 does _not_ requiring any ordering of response */
    if (! unsorted) {
        sort_arr = (unsigned char **)malloc(cd_len * sizeof(unsigned char *));
        if (NULL == sort_arr) {
            printf("sg_opcodes: no memory to sort operation codes, "
                   "try '-u'\n");
            return;
        }
        memset(sort_arr, 0, cd_len * sizeof(unsigned char *));
        ucp = rsoc_buff + 4;
        for (k = 0, j = 0; k < cd_len; ++j, k += len, ucp += len) {
            sort_arr[j] = ucp;
            len = (ucp[5] & 0x2) ? 20 : 8;
        }
        qsort(sort_arr, j, sizeof(unsigned char *), 
              (alpha ? opcode_alpha_compare : opcode_num_compare));
    }
    for (k = 0, j = 0; k < cd_len; ++j, k += len) {
        ucp = unsorted ? (rsoc_buff + 4 + k) : sort_arr[j];
        len = (ucp[5] & 0x2) ? 20 : 8;
        if (ucp[5] & 1) {
            serv_act = ((ucp[2] << 8) | ucp[3]);
            sg_get_opcode_sa_name(ucp[0], serv_act, peri_type,
                                  NAME_BUFF_SZ, name_buff);
            snprintf(sa_buff, sizeof(sa_buff), "%.4x", serv_act);
        } else {
            sg_get_opcode_name(ucp[0], peri_type, 
                               NAME_BUFF_SZ, name_buff);
            memset(sa_buff, ' ', sizeof(sa_buff));
        }
        if (rctd) {
            if (ucp[5] & 0x2) {
                printf(" %.2x     %.4s       %3d", ucp[0], sa_buff,
                       ((ucp[6] << 8) | ucp[7]));
                to = (ucp[12] << 24) + (ucp[13] << 16) + (ucp[14] << 8) +
                     ucp[15];
                if (0 == to)
                    printf("         -");
                else
                    printf("  %8lu", to);
                to = (ucp[16] << 24) + (ucp[17] << 16) + (ucp[18] << 8) +
                     ucp[19];
                if (0 == to)
                    printf("          -");
                else
                    printf("   %8lu", to);
                printf("    %s\n", name_buff);
            } else
                printf(" %.2x     %.4s       %3d                         "
                       "%s\n", ucp[0], sa_buff, ((ucp[6] << 8) | ucp[7]),
                       name_buff);
        } else
            printf(" %.2x     %.4s       %3d    %s\n",
                   ucp[0], sa_buff, ((ucp[6] << 8) | ucp[7]), name_buff);
    }
}

static void decode_cmd_to_descriptor(unsigned char * dp, int max_b_len,
                                     char * b)
{
    int len;
    unsigned long to;

    if ((max_b_len < 2) || (NULL == dp))
        return;
    b[max_b_len - 1] = '\0';
    --max_b_len;
    len = (dp[0] << 8) + dp[1];
    if (10 != len) {
        snprintf(b, max_b_len, "command timeout descriptor length %d "
                 "(expect 10)", len);
        return;
    }
    to = (dp[4] << 24) + (dp[5] << 16) + (dp[6] << 8) + dp[7];
    if (0 == to)
        snprintf(b, max_b_len, "no nominal timeout, ");
    else
        snprintf(b, max_b_len, "nominal timeout: %lu secs, ", to);
    len = strlen(b);
    max_b_len -= len;
    b += len;
    to = (dp[8] << 24) + (dp[9] << 16) + (dp[10] << 8) + dp[11];
    if (0 == to)
        snprintf(b, max_b_len, "no recommended timeout");
    else
        snprintf(b, max_b_len, "recommended timeout: %lu secs", to);
    return;
}

static void list_one(unsigned char * rsoc_buff, int cd_len, int rep_opts,
                     int do_opcode, int do_servact)
{
    int k;
    char name_buff[NAME_BUFF_SZ];
    unsigned char * ucp;
    const char * cp;
    int v = 0;


    printf("\n  Opcode=0x%.2x", do_opcode);
    if (rep_opts > 1)
        printf("  Service_action=0x%.4x", do_servact);
    printf("\n");
    sg_get_opcode_sa_name(((do_opcode > 0) ? do_opcode : 0),
                          ((do_servact > 0) ? do_servact : 0),
                          peri_type, NAME_BUFF_SZ, name_buff);
    printf("  Command_name: %s\n", name_buff);
    switch((int)(rsoc_buff[1] & 7)) {
    case 0:
        cp = "not currently available";
        break;
    case 1:
        cp = "NOT supported";
        break;
    case 3:
        cp = "supported (conforming to SCSI standard)";
        v = 1;
        break;
    case 5:
        cp = "supported (in a vendor specific manner)";
        v = 1;
        break;
    default:
        snprintf(name_buff, NAME_BUFF_SZ, "support reserved [0x%x]",
                 rsoc_buff[1] & 7); 
        cp = name_buff;
        break;
    }
    printf("  Command %s\n", cp);
    if (v) {
        printf("  Usage data: ");
        ucp = rsoc_buff + 4;
        for (k = 0; k < cd_len; ++k)
            printf("%.2x ", ucp[k]);
        printf("\n");
    }
    if (0x80 & rsoc_buff[1]) {      /* CTDP */
        ucp = rsoc_buff + 4 + cd_len;
        decode_cmd_to_descriptor(ucp, NAME_BUFF_SZ, name_buff);
        printf("  %s\n", name_buff);
    }
}


int main(int argc, char * argv[])
{
    int sg_fd, cd_len, res, len;
    unsigned char rsoc_buff[MX_ALLOC_LEN];
    int rep_opts = 0;
    const char * cp;
    char buff[48];
    struct sg_simple_inquiry_resp inq_resp;
    const char * op_name;
    struct opts_t opts;

    memset(&opts, 0, sizeof(opts));
    opts.do_opcode = -1;
    opts.do_servact = -1;
    res = process_cl(&opts, argc, argv);
    if (res)
        return SG_LIB_SYNTAX_ERROR;
    if (opts.do_help) {
        if (opts.opt_new)
            usage();
        else
            usage_old();
        return 0;
    }
    if (opts.do_version) {
        fprintf(stderr, "Version string: %s\n", version_str);
        return 0;
    }
    
    if (NULL == opts.device_name) {
        fprintf(stderr, "No DEVICE argument given\n");
        if (opts.opt_new)
            usage();
        else
            usage_old();
        return SG_LIB_SYNTAX_ERROR;
    }
    if ((-1 != opts.do_servact) && (-1 == opts.do_opcode)) {
        fprintf(stderr, "When '-s' is chosen, so must '-o' be chosen\n");
        if (opts.opt_new)
            usage();
        else
            usage_old();
        return SG_LIB_SYNTAX_ERROR;
    }
    if (opts.do_unsorted && opts.do_alpha)
        fprintf(stderr, "warning: unsorted ('-u') and alpha ('-a') options "
                "chosen, ignoring alpha\n");
    if (opts.do_taskman && ((-1 != opts.do_opcode) || opts.do_alpha ||
        opts.do_unsorted)) {
        fprintf(stderr, "warning: task management functions ('-t') chosen "
                "so alpha ('-a'),\n          unsorted ('-u') and opcode "
                "('-o') options ignored\n");
    }
    op_name = opts.do_taskman ? "Report supported task management functions" :
              "Report supported operation codes";

#ifdef USE_SG_IO
    if ((sg_fd = open(opts.device_name, O_RDONLY | O_NONBLOCK)) < 0) {
        snprintf(ebuff, EBUFF_SZ, "sg_opcodes: error opening file (ro): %s",
                 opts.device_name);
        perror(ebuff);
        return SG_LIB_FILE_ERROR;
    }
#else
    if ((sg_fd = scsi_pt_open_device(opts.device_name, 1 /* RO */,
                                     opts.do_verbose)) < 0) {
        fprintf(stderr, "sg_opcodes: error opening file (ro): %s: %s\n",
                opts.device_name, safe_strerror(-sg_fd));
        return SG_LIB_FILE_ERROR;
    }
#endif
    if (0 == sg_simple_inquiry(sg_fd, &inq_resp, 1, opts.do_verbose)) {
        peri_type = inq_resp.peripheral_type;
        if (0 == opts.do_raw) {
            printf("  %.8s  %.16s  %.4s\n", inq_resp.vendor, inq_resp.product,
                   inq_resp.revision);
            cp = sg_get_pdt_str(peri_type, sizeof(buff), buff);
            if (strlen(cp) > 0)
                printf("  Peripheral device type: %s\n", cp);
            else
                printf("  Peripheral device type: 0x%x\n", peri_type);
        }
    } else {
        fprintf(stderr, "sg_opcodes: %s doesn't respond to a SCSI "
                "INQUIRY\n", opts.device_name);
        return SG_LIB_CAT_OTHER;
    }
#ifdef USE_SG_IO
    close(sg_fd);
#else
    res = sg_cmds_close_device(sg_fd);
    if (res < 0) {
        fprintf(stderr, "close error: %s\n", safe_strerror(-res));
        return SG_LIB_FILE_ERROR;
    }
#endif

#ifndef TEST_CODE
    if (5 == peri_type) {
        printf("'%s' command not supported\nfor CD/DVD devices\n",
               op_name);
        return SG_LIB_CAT_OTHER;
    }
#endif

#ifdef USE_SG_IO
    if ((sg_fd = open(opts.device_name, O_RDWR | O_NONBLOCK)) < 0) {
        snprintf(ebuff, EBUFF_SZ, "sg_opcodes: error opening file: %s (rw)",
                 opts.device_name);
        perror(ebuff);
        return SG_LIB_FILE_ERROR;
    }
#else
    if ((sg_fd = scsi_pt_open_device(opts.device_name, 0 /* RW */,
                                     opts.do_verbose)) < 0) {
        fprintf(stderr, "sg_opcodes: error opening file (rw): %s: %s\n",
                opts.device_name, safe_strerror(-sg_fd));
        return SG_LIB_FILE_ERROR;
    }
#endif
    if (opts.do_opcode >= 0)
        rep_opts = ((opts.do_servact >= 0) ? 2 : 1);
    memset(rsoc_buff, 0, sizeof(rsoc_buff));
#ifndef TEST_CODE
    if (opts.do_taskman)
        res = do_rstmf(sg_fd, rsoc_buff, sizeof(rsoc_buff), 0,
                       opts.do_verbose);
    else
        res = do_rsoc(sg_fd, opts.do_rctd, rep_opts, opts.do_opcode,
                      opts.do_servact, rsoc_buff, sizeof(rsoc_buff), 0,
                      opts.do_verbose);
    switch (res) {
    case 0:
    case SG_LIB_CAT_RECOVERED:
        break;
    case SG_LIB_CAT_ABORTED_COMMAND:
        fprintf(stderr, "%s: aborted command\n", op_name);
        goto err_out;
    case SG_LIB_CAT_NOT_READY:
        fprintf(stderr, "%s: device not ready\n", op_name);
        goto err_out;
    case SG_LIB_CAT_UNIT_ATTENTION:
        fprintf(stderr, "%s: unit attention\n", op_name);
        goto err_out;
    case SG_LIB_CAT_INVALID_OP:
        fprintf(stderr, "%s: operation not supported\n", op_name);
        goto err_out;
    case SG_LIB_CAT_ILLEGAL_REQ:
        fprintf(stderr, "bad field in cdb including %s not supported\n",
                op_name);
        goto err_out;
    default:
        fprintf(stderr, "%s failed\n", op_name);
        goto err_out;
    }
#else
    if (opts.do_taskman) {
        rsoc_buff[0] = dummy_rsmft_r0;
        rsoc_buff[1] = dummy_rsmft_r1;
    } else if (opts.do_rctd) {
        if (0 == rep_opts) {
#if 1
            memcpy(rsoc_buff, (unsigned char *)&dummy_to_resp,
                   sizeof(dummy_to_resp));
#else
            memcpy(rsoc_buff, (unsigned char *)&dummy_resp,
                   sizeof(dummy_resp));
#endif
        } else
            memcpy(rsoc_buff, dummy_1_to_cmd, sizeof(dummy_1_to_cmd));
    } else {
        if (0 == rep_opts)
            memcpy(rsoc_buff, (unsigned char *)&dummy_resp,
                   sizeof(dummy_resp));
        else
            memcpy(rsoc_buff, dummy_1_cmd, sizeof(dummy_1_cmd));
    }
#endif
    if (opts.do_taskman) {
        if (opts.do_raw) {
            dStrRaw((const char *)rsoc_buff, 4);
            goto err_out;
        }
        printf("\nTask Management Functions supported by device:\n");
        if (opts.do_hex) {
            dStrHex((const char *)rsoc_buff, 4, 1);
            goto err_out;
        }
        if (rsoc_buff[0] & 0x80)
            printf("    Abort task\n");
        if (rsoc_buff[0] & 0x40)
            printf("    Abort task set\n");
        if (rsoc_buff[0] & 0x20)
            printf("    Clear ACA\n");
        if (rsoc_buff[0] & 0x10)
            printf("    Clear task set\n");
        if (rsoc_buff[0] & 0x8)
            printf("    Logical unit reset\n");
        if (rsoc_buff[0] & 0x4)
            printf("    Query task\n");
        if (rsoc_buff[0] & 0x2)
            printf("    Target reset\n");
        if (rsoc_buff[0] & 0x1)
            printf("    Wakeup\n");
        if (rsoc_buff[1] & 0x4)
            printf("    Query unit attention\n");
        if (rsoc_buff[1] & 0x2)
            printf("    Query task set\n");
        if (rsoc_buff[1] & 0x1)
            printf("    I_T nexus reset\n");
    } else if (0 == rep_opts) {  /* list all supported operation codes */
        len = ((rsoc_buff[0] << 24) | (rsoc_buff[1] << 16) |
               (rsoc_buff[2] << 8) | rsoc_buff[3]) + 4;
        if (len > (int)sizeof(rsoc_buff))
            len = sizeof(rsoc_buff);
        if (opts.do_raw) {
            dStrRaw((const char *)rsoc_buff, len);
            goto err_out;
        }
        if (opts.do_hex) {
            dStrHex((const char *)rsoc_buff, len, 1);
            goto err_out;
        }
        list_all_codes(rsoc_buff, sizeof(rsoc_buff), opts.do_unsorted,
                       opts.do_alpha, opts.do_rctd);
    } else {    /* asked about specific command */
        cd_len = ((rsoc_buff[2] << 8) | rsoc_buff[3]);
        len = cd_len + 4;
        if (len > (int)sizeof(rsoc_buff))
            len = sizeof(rsoc_buff);
        if (opts.do_raw) {
            dStrRaw((const char *)rsoc_buff, len);
            goto err_out;
        }
        if (opts.do_hex) {
            dStrHex((const char *)rsoc_buff, len, 1);
            goto err_out;
        }
        list_one(rsoc_buff, cd_len, rep_opts, opts.do_opcode,
                 opts.do_servact);
    }
    res = 0;

err_out:
#ifdef USE_SG_IO
    close(sg_fd);
#else
    sg_cmds_close_device(sg_fd);
#endif
    return res;
}


#ifdef USE_SG_IO
/* Report Supported Operation Codes */
/* Returns 0 when successful */
static int do_rsoc(int sg_fd, int rctd, int rep_opts, int rq_opcode,
                   int rq_servact, void * resp, int mx_resp_len, int noisy,
                   int verbose)
{
    int res, k;
    unsigned char rsocCmdBlk[RSOC_CMD_LEN] = {SG_MAINTENANCE_IN, RSOC_SA, 0, 
                                              0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    if (rctd)
        rsocCmdBlk[2] |= 0x80;
    if (rep_opts)
        rsocCmdBlk[2] |= (rep_opts & 0x7);
    if (rq_opcode > 0)
        rsocCmdBlk[3] = (rq_opcode & 0xff);
    if (rq_servact > 0) {
        rsocCmdBlk[4] = (unsigned char)((rq_servact >> 8) & 0xff);
        rsocCmdBlk[5] = (unsigned char)(rq_servact & 0xff);

    }
    rsocCmdBlk[6] = (unsigned char)((mx_resp_len >> 24) & 0xff);
    rsocCmdBlk[7] = (unsigned char)((mx_resp_len >> 16) & 0xff);
    rsocCmdBlk[8] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    rsocCmdBlk[9] = (unsigned char)(mx_resp_len & 0xff);

    if (verbose) {
        fprintf(stderr, "    Report Supported Operation Codes cmd: ");
        for (k = 0; k < RSOC_CMD_LEN; ++k)
            fprintf(stderr, "%02x ", rsocCmdBlk[k]);
        fprintf(stderr, "\n");
    }
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(rsocCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;
    io_hdr.dxferp = resp;
    io_hdr.cmdp = rsocCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        perror("SG_IO (rsoc) error");
        return -1;
    }
    if (verbose > 2)
        fprintf(stderr, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        sg_chk_n_print3("Report supported operation codes", &io_hdr,
                        verbose > 1);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        return 0;
    default:
        if (noisy | verbose) {
            char ebuff[EBUFF_SZ];

            if (0 == rep_opts)
                snprintf(ebuff, EBUFF_SZ, "RSOC error, rep_opts=0 (all) ");
            else if (1 == rep_opts)
                snprintf(ebuff, EBUFF_SZ, "RSOC error, rq_opcode=0x%x ",
                         rq_opcode);
            else
                snprintf(ebuff, EBUFF_SZ, "RSOC error, rq_opcode=0x%x, "
                         "rq_sa=0x%x ", rq_opcode, rq_servact);
            sg_chk_n_print3(ebuff, &io_hdr, verbose > 1);
        }
        return res;
    }
}

/* Report Supported Task Management Function */
/* Returns 0 when successful */
static int do_rstmf(int sg_fd, void * resp, int mx_resp_len, int noisy,
                    int verbose)
{
    int res, k;
    unsigned char rstmfCmdBlk[RSTMF_CMD_LEN] = {SG_MAINTENANCE_IN, RSTMF_SA,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    rstmfCmdBlk[6] = (unsigned char)((mx_resp_len >> 24) & 0xff);
    rstmfCmdBlk[7] = (unsigned char)((mx_resp_len >> 16) & 0xff);
    rstmfCmdBlk[8] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    rstmfCmdBlk[9] = (unsigned char)(mx_resp_len & 0xff);

    if (verbose) {
        fprintf(stderr, "    Report Supported Task Management Functions "
                "cmd: ");
        for (k = 0; k < RSTMF_CMD_LEN; ++k)
            fprintf(stderr, "%02x ", rstmfCmdBlk[k]);
        fprintf(stderr, "\n");
    }
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(rstmfCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;
    io_hdr.dxferp = resp;
    io_hdr.cmdp = rstmfCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        perror("SG_IO (rstmf) error");
        return -1;
    }
    if (verbose > 2)
        fprintf(stderr, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        sg_chk_n_print3("Report supported task management fns", &io_hdr,
                        verbose > 1);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        return 0;
    default:
        if (noisy | verbose) {
            char ebuff[EBUFF_SZ];
            snprintf(ebuff, EBUFF_SZ, "RSTMF error ");
            sg_chk_n_print3(ebuff, &io_hdr, verbose > 1);
        }
        return res;
    }
}

#else /* use generic pass through code instead */

static int do_rsoc(int sg_fd, int rctd, int rep_opts, int rq_opcode,
                   int rq_servact, void * resp, int mx_resp_len, int noisy,
                   int verbose)
{
    int k, ret, res, sense_cat;
    unsigned char rsocCmdBlk[RSOC_CMD_LEN] = {SG_MAINTENANCE_IN, RSOC_SA, 0,
                                              0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    if (rctd)
        rsocCmdBlk[2] |= 0x80;
    if (rep_opts)
        rsocCmdBlk[2] |= (rep_opts & 0x7);
    if (rq_opcode > 0)
        rsocCmdBlk[3] = (rq_opcode & 0xff);
    if (rq_servact > 0) {
        rsocCmdBlk[4] = (unsigned char)((rq_servact >> 8) & 0xff);
        rsocCmdBlk[5] = (unsigned char)(rq_servact & 0xff);

    }
    rsocCmdBlk[6] = (unsigned char)((mx_resp_len >> 24) & 0xff);
    rsocCmdBlk[7] = (unsigned char)((mx_resp_len >> 16) & 0xff);
    rsocCmdBlk[8] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    rsocCmdBlk[9] = (unsigned char)(mx_resp_len & 0xff);

    if (verbose) {
        fprintf(stderr, "    Report Supported Operation Codes cmd: ");
        for (k = 0; k < RSOC_CMD_LEN; ++k)
            fprintf(stderr, "%02x ", rsocCmdBlk[k]);
        fprintf(stderr, "\n");
    }
    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "Report Supported Operation Codes: out "
                "of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, rsocCmdBlk, sizeof(rsocCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "Report Supported Operation Codes", res,
                               mx_resp_len, sense_b, noisy, verbose,
                               &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else
        ret = 0;

    destruct_scsi_pt_obj(ptvp);
    return ret;
}

static int do_rstmf(int sg_fd, void * resp, int mx_resp_len, int noisy,
                    int verbose)
{
    int k, ret, res, sense_cat;
    unsigned char rstmfCmdBlk[RSTMF_CMD_LEN] = {SG_MAINTENANCE_IN, RSTMF_SA,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_pt_base * ptvp;

    rstmfCmdBlk[6] = (unsigned char)((mx_resp_len >> 24) & 0xff);
    rstmfCmdBlk[7] = (unsigned char)((mx_resp_len >> 16) & 0xff);
    rstmfCmdBlk[8] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    rstmfCmdBlk[9] = (unsigned char)(mx_resp_len & 0xff);

    if (verbose) {
        fprintf(stderr, "    Report Supported Task Management Functions "
                "cmd: ");
        for (k = 0; k < RSTMF_CMD_LEN; ++k)
            fprintf(stderr, "%02x ", rstmfCmdBlk[k]);
        fprintf(stderr, "\n");
    }
    ptvp = construct_scsi_pt_obj();
    if (NULL == ptvp) {
        fprintf(sg_warnings_strm, "Report Supported Task Management "
                "Functions: out of memory\n");
        return -1;
    }
    set_scsi_pt_cdb(ptvp, rstmfCmdBlk, sizeof(rstmfCmdBlk));
    set_scsi_pt_sense(ptvp, sense_b, sizeof(sense_b));
    set_scsi_pt_data_in(ptvp, (unsigned char *)resp, mx_resp_len);
    res = do_scsi_pt(ptvp, sg_fd, DEF_TIMEOUT, verbose);
    ret = sg_cmds_process_resp(ptvp, "Report Supported Task management "
                               "functions", res, mx_resp_len, sense_b, noisy,
                                verbose, &sense_cat);
    if (-1 == ret)
        ;
    else if (-2 == ret) {
        switch (sense_cat) {
        case SG_LIB_CAT_NOT_READY:
        case SG_LIB_CAT_UNIT_ATTENTION:
        case SG_LIB_CAT_INVALID_OP:
        case SG_LIB_CAT_ILLEGAL_REQ:
        case SG_LIB_CAT_ABORTED_COMMAND:
            ret = sense_cat;
            break;
        case SG_LIB_CAT_RECOVERED:
        case SG_LIB_CAT_NO_SENSE:
            ret = 0;
            break;
        default:
            ret = -1;
            break;
        }
    } else
        ret = 0;

    destruct_scsi_pt_obj(ptvp);
    return ret;
}

#endif