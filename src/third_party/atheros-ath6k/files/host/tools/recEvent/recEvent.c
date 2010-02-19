/*
 * Copyright (c) 2006 Atheros Communications Inc.
 * All rights reserved.
 *
 *
 * 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
//
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>
#include <limits.h>
#include <asm/types.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/version.h>
#include <linux/wireless.h>
#include <a_config.h>
#include <a_osapi.h>
#include <a_types.h>
#include <athdefs.h>
#include <ieee80211.h>
#include <wmi.h>
#include <athdrv_linux.h>
#include <dbglog_api.h>

#undef DEBUG
#undef DBGLOG_DEBUG

#define ID_LEN                         2
#define DBGLOG_FILE                    "dbglog.h"
#define DBGLOGID_FILE                  "dbglog_id.h"

#define GET_CURRENT_TIME(s) do { \
    time_t t; \
    t = time(NULL); \
    s = strtok(ctime(&t), "\n"); \
} while (0);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
/* ---------- CONSTANTS --------------- */
#define ATH_WE_HEADER_TYPE_NULL     0         /* Not available */
#define ATH_WE_HEADER_TYPE_CHAR     2         /* char [IFNAMSIZ] */
#define ATH_WE_HEADER_TYPE_UINT     4         /* __u32 */
#define ATH_WE_HEADER_TYPE_FREQ     5         /* struct iw_freq */
#define ATH_WE_HEADER_TYPE_ADDR     6         /* struct sockaddr */
#define ATH_WE_HEADER_TYPE_POINT    8         /* struct iw_point */
#define ATH_WE_HEADER_TYPE_PARAM    9         /* struct iw_param */
#define ATH_WE_HEADER_TYPE_QUAL     10        /* struct iw_quality */

#define ATH_WE_DESCR_FLAG_DUMP      0x0001    /* Not part of the dump command */
#define ATH_WE_DESCR_FLAG_EVENT     0x0002    /* Generate an event on SET */
#define ATH_WE_DESCR_FLAG_RESTRICT  0x0004    /* GET : request is ROOT only */
#define ATH_WE_DESCR_FLAG_NOMAX     0x0008    /* GET : no limit on request size */

#define ATH_SIOCSIWMODUL            0x8b2f
#define ATH_SIOCGIWMODUL            0x8b2f
#define ATH_WE_VERSION          (A_INT16)22
/* ---------------------------- TYPES ---------------------------- */

/*
 * standard IOCTL looks like.
 */
struct ath_ioctl_description
{
    A_UINT8         header_type;        /* NULL, iw_point or other */
    A_UINT8         token_type;     /* Future */
    A_UINT16    token_size;     /* Granularity of payload */
    A_UINT16    min_tokens;     /* Min acceptable token number */
    A_UINT16    max_tokens;     /* Max acceptable token number */
    A_UINT32    flags;          /* Special handling of the request */
};

/* -------------------------- VARIABLES -------------------------- */

/*
 * Meta-data about all the standard Wireless Extension request we
 * know about.
 */
static const struct ath_ioctl_description standard_ioctl_descr[] = {
    [SIOCSIWCOMMIT  - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_NULL,
    },
    [SIOCGIWNAME    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_CHAR,
        .flags      = ATH_WE_DESCR_FLAG_DUMP,
    },
    [SIOCSIWNWID    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
        .flags      = ATH_WE_DESCR_FLAG_EVENT,
    },
    [SIOCGIWNWID    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
        .flags      = ATH_WE_DESCR_FLAG_DUMP,
    },
    [SIOCSIWFREQ    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_FREQ,
        .flags      = ATH_WE_DESCR_FLAG_EVENT,
    },
    [SIOCGIWFREQ    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_FREQ,
        .flags      = ATH_WE_DESCR_FLAG_DUMP,
    },
    [SIOCSIWMODE    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_UINT,
        .flags      = ATH_WE_DESCR_FLAG_EVENT,
    },
    [SIOCGIWMODE    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_UINT,
        .flags      = ATH_WE_DESCR_FLAG_DUMP,
    },
    [SIOCSIWSENS    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCGIWSENS    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCSIWRANGE   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_NULL,
    },
    [SIOCGIWRANGE   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = sizeof(struct iw_range),
        .flags      = ATH_WE_DESCR_FLAG_DUMP,
    },
    [SIOCSIWPRIV    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_NULL,
    },
    [SIOCGIWPRIV    - SIOCIWFIRST] = { /* (handled directly by us) */
        .header_type    = ATH_WE_HEADER_TYPE_NULL,
    },
    [SIOCSIWSTATS   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_NULL,
    },
    [SIOCGIWSTATS   - SIOCIWFIRST] = { /* (handled directly by us) */
        .header_type    = ATH_WE_HEADER_TYPE_NULL,
        .flags      = ATH_WE_DESCR_FLAG_DUMP,
    },
    [SIOCSIWSPY - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = sizeof(struct sockaddr),
        .max_tokens = IW_MAX_SPY,
    },
    [SIOCGIWSPY - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = sizeof(struct sockaddr) +
                  sizeof(struct iw_quality),
        .max_tokens = IW_MAX_SPY,
    },
    [SIOCSIWTHRSPY  - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = sizeof(struct iw_thrspy),
        .min_tokens = 1,
        .max_tokens = 1,
    },
    [SIOCGIWTHRSPY  - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = sizeof(struct iw_thrspy),
        .min_tokens = 1,
        .max_tokens = 1,
    },
    [SIOCSIWAP  - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_ADDR,
    },
    [SIOCGIWAP  - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_ADDR,
        .flags      = ATH_WE_DESCR_FLAG_DUMP,
    },
    [SIOCSIWMLME    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .min_tokens = sizeof(struct iw_mlme),
        .max_tokens = sizeof(struct iw_mlme),
    },
    [SIOCGIWAPLIST  - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = sizeof(struct sockaddr) +
                  sizeof(struct iw_quality),
        .max_tokens = IW_MAX_AP,
        .flags      = ATH_WE_DESCR_FLAG_NOMAX,
    },
    [SIOCSIWSCAN    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .min_tokens = 0,
        .max_tokens = sizeof(struct iw_scan_req),
    },
    [SIOCGIWSCAN    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = IW_SCAN_MAX_DATA,
        .flags      = ATH_WE_DESCR_FLAG_NOMAX,
    },
    [SIOCSIWESSID   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = IW_ESSID_MAX_SIZE + 1,
        .flags      = ATH_WE_DESCR_FLAG_EVENT,
    },
    [SIOCGIWESSID   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = IW_ESSID_MAX_SIZE + 1,
        .flags      = ATH_WE_DESCR_FLAG_DUMP,
    },
    [SIOCSIWNICKN   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = IW_ESSID_MAX_SIZE + 1,
    },
    [SIOCGIWNICKN   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = IW_ESSID_MAX_SIZE + 1,
    },
    [SIOCSIWRATE    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCGIWRATE    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCSIWRTS - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCGIWRTS - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCSIWFRAG    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCGIWFRAG    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCSIWTXPOW   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCGIWTXPOW   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCSIWRETRY   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCGIWRETRY   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCSIWENCODE  - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = IW_ENCODING_TOKEN_MAX,
        .flags      = ATH_WE_DESCR_FLAG_EVENT | ATH_WE_DESCR_FLAG_RESTRICT,
    },
    [SIOCGIWENCODE  - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = IW_ENCODING_TOKEN_MAX,
        .flags      = ATH_WE_DESCR_FLAG_DUMP | ATH_WE_DESCR_FLAG_RESTRICT,
    },
    [SIOCSIWPOWER   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCGIWPOWER   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [ATH_SIOCSIWMODUL   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [ATH_SIOCGIWMODUL   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCSIWGENIE   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = IW_GENERIC_IE_MAX,
    },
    [SIOCGIWGENIE   - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = IW_GENERIC_IE_MAX,
    },
    [SIOCSIWAUTH    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCGIWAUTH    - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_PARAM,
    },
    [SIOCSIWENCODEEXT - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .min_tokens = sizeof(struct iw_encode_ext),
        .max_tokens = sizeof(struct iw_encode_ext) +
                  IW_ENCODING_TOKEN_MAX,
    },
    [SIOCGIWENCODEEXT - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .min_tokens = sizeof(struct iw_encode_ext),
        .max_tokens = sizeof(struct iw_encode_ext) +
                  IW_ENCODING_TOKEN_MAX,
    },
    [SIOCSIWPMKSA - SIOCIWFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .min_tokens = sizeof(struct iw_pmksa),
        .max_tokens = sizeof(struct iw_pmksa),
    },
};
static const unsigned int standard_ioctl_num = (sizeof(standard_ioctl_descr) /
                        sizeof(struct ath_ioctl_description));

/*
 * Meta-data about all the additional standard Wireless Extension events
 */
static const struct ath_ioctl_description standard_event_descr[] = {
    [IWEVTXDROP - IWEVFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_ADDR,
    },
    [IWEVQUAL   - IWEVFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_QUAL,
    },
    [IWEVCUSTOM - IWEVFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = IW_CUSTOM_MAX,
    },
    [IWEVREGISTERED - IWEVFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_ADDR,
    },
    [IWEVEXPIRED    - IWEVFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_ADDR,
    },
    [IWEVGENIE  - IWEVFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = IW_GENERIC_IE_MAX,
    },
    [IWEVMICHAELMICFAILURE  - IWEVFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = sizeof(struct iw_michaelmicfailure),
    },
    [IWEVASSOCREQIE - IWEVFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = IW_GENERIC_IE_MAX,
    },
    [IWEVASSOCRESPIE    - IWEVFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = IW_GENERIC_IE_MAX,
    },
    [IWEVPMKIDCAND  - IWEVFIRST] = {
        .header_type    = ATH_WE_HEADER_TYPE_POINT,
        .token_size = 1,
        .max_tokens = sizeof(struct iw_pmkid_cand),
    },
};
static const unsigned int standard_event_num = (sizeof(standard_event_descr) /
                        sizeof(struct ath_ioctl_description));

/* Size (in bytes) of various events */
static const int event_type_size[] = {
    IW_EV_LCP_PK_LEN,   /* ATH_WE_HEADER_TYPE_NULL */
    0,
    IW_EV_CHAR_PK_LEN,  /* ATH_WE_HEADER_TYPE_CHAR */
    0,
    IW_EV_UINT_PK_LEN,  /* ATH_WE_HEADER_TYPE_UINT */
    IW_EV_FREQ_PK_LEN,  /* ATH_WE_HEADER_TYPE_FREQ */
    IW_EV_ADDR_PK_LEN,  /* ATH_WE_HEADER_TYPE_ADDR */
    0,
    IW_EV_POINT_PK_LEN, /* Without variable payload */
    IW_EV_PARAM_PK_LEN, /* ATH_WE_HEADER_TYPE_PARAM */
    IW_EV_QUAL_PK_LEN,  /* ATH_WE_HEADER_TYPE_QUAL */
};

/* Structure used for parsing event list, such as Wireless Events
 * and scan results */
typedef struct event_list
{
  A_INT8 *	end;		/* End of the list */
  A_INT8 *	current;	/* Current event in list of events */
  A_INT8 *	value;		/* Current value in event */
} event_list;

#endif /*#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21) */

#define SRCDIR_FLAG            0x01
#define LOGFILE_FLAG           0x02
#define DBGREC_LIMIT_FLAG      0x04
#define RESTORE_FLAG           0x08

const char *progname;
char restorefile[PATH_MAX];
char dbglogfile[PATH_MAX];
char dbglogidfile[PATH_MAX];
char dbglogoutfile[PATH_MAX];
FILE *fpout;
int dbgRecLimit = 1000000; /* Million records is a good default */
int optionflag;
char dbglog_id_tag[DBGLOG_MODULEID_NUM_MAX][DBGLOG_DBGID_NUM_MAX][DBGLOG_DBGID_DEFINITION_LEN_MAX];
const char options[] = 
"Options:\n\
--logfile=<Output log file> [Mandatory]\n\
--srcdir=<Directory containing the dbglog header files> [Mandatory]\n\
--reclimit=<Maximum number of records before the log rolls over> [Optional]\n\
--restore=<Script to recover from errors on the target> [Optional]\n\
The options can also be given in the abbreviated form --option=x or -o x. The options can be given in any order";

#ifdef DEBUG
int debugRecEvent = 0;
#define RECEVENT_DEBUG_PRINTF(args...)        if (debugRecEvent) printf(args);
#else
#define RECEVENT_DEBUG_PRINTF(args...)
#endif

static A_STATUS app_wmiready_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_connect_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_disconnect_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_bssInfo_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_pstream_timeout_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_reportError_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_rssi_threshold_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_scan_complete_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_challenge_resp_event_rx(A_UINT8 *datap, int len);
static A_STATUS app_target_debug_event_rx(A_INT8 *datap, int len);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
int app_extract_events(struct event_list*   list,
                struct iw_event *   iwe);
#endif
static void
event_rtm_newlink(struct nlmsghdr *h, int len);

static void
event_wireless(A_INT8 *data, int len);

int
string_search(FILE *fp, char *string)
{
    char str[DBGLOG_DBGID_DEFINITION_LEN_MAX];

    rewind(fp);
    memset(str, 0, DBGLOG_DBGID_DEFINITION_LEN_MAX);
    while (!feof(fp)) {
        fscanf(fp, "%s", str);
        if (strstr(str, string)) return 1;
    }

    return 0;
}

void
get_module_name(char *string, char *dest)
{
    char *str1, *str2;
    char str[DBGLOG_DBGID_DEFINITION_LEN_MAX];

    memset(str, 0, DBGLOG_DBGID_DEFINITION_LEN_MAX);
    strcpy(str, string);
    str1 = strtok(str, "_");
    while ((str2 = strtok(NULL, "_"))) {
        str1 = str2;
    }

    strcpy(dest, str1);
}

#ifdef DBGLOG_DEBUG
void
dbglog_print_id_tags(void)
{
    int i, j;

    for (i = 0; i < DBGLOG_MODULEID_NUM_MAX; i++) {
        for (j = 0; j < DBGLOG_DBGID_NUM_MAX; j++) {
            printf("[%d][%d]: %s\n", i, j, dbglog_id_tag[i][j]);
        }
    }
}
#endif /* DBGLOG_DEBUG */

int
dbglog_generate_id_tags(void)
{
    int id1, id2;
    FILE *fp1, *fp2;
    char str1[DBGLOG_DBGID_DEFINITION_LEN_MAX];
    char str2[DBGLOG_DBGID_DEFINITION_LEN_MAX];
    char str3[DBGLOG_DBGID_DEFINITION_LEN_MAX];

    if (!(fp1 = fopen(dbglogfile, "r"))) {
        perror(dbglogfile);
        return -1;
    }

    if (!(fp2 = fopen(dbglogidfile, "r"))) {
        perror(dbglogidfile);
        fclose(fp1);
        return -1;
    }

    memset(dbglog_id_tag, 0, sizeof(dbglog_id_tag));
    if (string_search(fp1, "DBGLOG_MODULEID_START")) {
        fscanf(fp1, "%s %s %d", str1, str2, &id1);
        do {
            memset(str3, 0, DBGLOG_DBGID_DEFINITION_LEN_MAX);
            get_module_name(str2, str3);
            strcat(str3, "_DBGID_DEFINITION_START");
            if (string_search(fp2, str3)) {
                memset(str3, 0, DBGLOG_DBGID_DEFINITION_LEN_MAX);
                get_module_name(str2, str3);
                strcat(str3, "_DBGID_DEFINITION_END");
                fscanf(fp2, "%s %s %d", str1, str2, &id2);
                while (!(strstr(str2, str3))) {
                    strcpy((char *)&dbglog_id_tag[id1][id2], str2);
                    fscanf(fp2, "%s %s %d", str1, str2, &id2);
                }
            }
            fscanf(fp1, "%s %s %d", str1, str2, &id1);
        } while (!(strstr(str2, "DBGLOG_MODULEID_END")));
    }

    fclose(fp2);
    fclose(fp1);

    return 0;
}

static void
usage(void)
{
    fprintf(stderr, "Usage:\n%s options\n", progname);
    fprintf(stderr, "%s\n", options);
    exit(-1);
}

int main(int argc, char** argv)
{
    int s, c, ret;
    struct sockaddr_nl local;
    struct sockaddr_nl from;
    socklen_t fromlen;
    struct nlmsghdr *h;
    char buf[8192];
    int left;

    progname = argv[0];

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"logfile", 1, NULL, 'f'},
            {"srcdir", 1, NULL, 'd'},
            {"reclimit", 1, NULL, 'l'},
            {"restore", 1, NULL, 'r'},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "f:d:l:r:", long_options, &option_index);
        if (c == -1) break;

        switch (c) {
            case 'f':
                memset(dbglogoutfile, 0, PATH_MAX);
                strncpy(dbglogoutfile, optarg, sizeof(dbglogoutfile)-1);
                optionflag |= LOGFILE_FLAG;
                break;

            case 'd':
                memset(dbglogfile, 0, PATH_MAX);
                strncpy(dbglogfile, optarg, sizeof(dbglogfile) - 1);
                strcat(dbglogfile, DBGLOG_FILE);
                memset(dbglogidfile, 0, PATH_MAX);
                strncpy(dbglogidfile, optarg, sizeof(dbglogidfile) - 1);
                strcat(dbglogidfile, DBGLOGID_FILE);
                optionflag |= SRCDIR_FLAG;
                break;

            case 'l':
                dbgRecLimit = strtoul(optarg, NULL, 0);
                break;

            case 'r':
                strncpy(restorefile, optarg, sizeof(restorefile)-1);
                optionflag |= RESTORE_FLAG;
                break;

            default:
                usage();
        }
    }

    if (!((optionflag & SRCDIR_FLAG) && (optionflag & LOGFILE_FLAG))) {
        usage();
    }

    /* Get the file name for dbglog output file */
    if (!(fpout = fopen(dbglogoutfile, "w+"))) {
        perror(dbglogoutfile);
        return -1;
    }

    /* first 8 bytes are to indicate the last record */
    fseek(fpout, 8, SEEK_SET);
    fprintf(fpout, "\n");

    s = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (s < 0) {
        perror("socket(PF_NETLINK,SOCK_RAW,NETLINK_ROUTE)");
        return -1;
    }

    memset(&local, 0, sizeof(local));
    local.nl_family = AF_NETLINK;
    local.nl_groups = RTMGRP_LINK;
    if (bind(s, (struct sockaddr *) &local, sizeof(local)) < 0) {
        perror("bind(netlink)");
        close(s);
        return -1;
    }

    if ((ret = dbglog_generate_id_tags()) < 0) {
        return -1;
    }

#ifdef DBGLOG_DEBUG
    dbglog_print_id_tags();
#endif /* DBGLOG_DEBUG */

    while (1) {
        fromlen = sizeof(from);
        left = recvfrom(s, buf, sizeof(buf), 0,
                        (struct sockaddr *) &from, &fromlen);
        if (left < 0) {
            if (errno != EINTR && errno != EAGAIN)
                perror("recvfrom(netlink)");
            break;
        }

        h = (struct nlmsghdr *) buf;

        while (left >= sizeof(*h)) {
            int len, plen;

            len = h->nlmsg_len;
            plen = len - sizeof(*h);
            if (len > left || plen < 0) {
                perror("Malformed netlink message: ");
                break;
            }

            switch (h->nlmsg_type) {
            case RTM_NEWLINK:
                event_rtm_newlink(h, plen);
                break;
            case RTM_DELLINK:
                RECEVENT_DEBUG_PRINTF("DELLINK\n");
                break;
            default:
                RECEVENT_DEBUG_PRINTF("OTHERS\n");
            }

            len = NLMSG_ALIGN(len);
            left -= len;
            h = (struct nlmsghdr *) ((char *) h + len);
        }
    }

    fclose(fpout);
    close(s);
    return 0;
}

static void
event_rtm_newlink(struct nlmsghdr *h, int len)
{
    struct ifinfomsg *ifi;
    int attrlen, nlmsg_len, rta_len;
    struct rtattr * attr;

    if (len < sizeof(*ifi)) {
        perror("too short\n");
        return;
    }

    ifi = NLMSG_DATA(h);

    nlmsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

    attrlen = h->nlmsg_len - nlmsg_len;
    if (attrlen < 0) {
        perror("bad attren\n");
        return;
    }

    attr = (struct rtattr *) (((char *) ifi) + nlmsg_len);

    rta_len = RTA_ALIGN(sizeof(struct rtattr));
    while (RTA_OK(attr, attrlen)) {
        if (attr->rta_type == IFLA_WIRELESS) {
            event_wireless( ((A_INT8*)attr) + rta_len, attr->rta_len - rta_len);
        } else if (attr->rta_type == IFLA_IFNAME) {

        }
        attr = RTA_NEXT(attr, attrlen);
    }
}

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
/*------------------------------------------------------------------*/
/*
 * Extract the next event from the event list.
 */
int
app_extract_events(struct event_list*   list,   /* list of events */
            struct iw_event *   iwe /* Extracted event */
            )
{
  const struct ath_ioctl_description *  descr = NULL;
  int       event_type = 0;
  unsigned int  event_len = 1;      /* Invalid */
  A_INT8 *  pointer;
  A_INT16       we_version = ATH_WE_VERSION;
  unsigned  cmd_index;

  /* Check for end of list */
  if((list->current + IW_EV_LCP_PK_LEN) > list->end)
    return(0);

  /* Extract the event header (to get the event id).
   * Note : the event may be unaligned, therefore copy... */
  memcpy((char *) iwe, list->current, IW_EV_LCP_PK_LEN);

  /* Check invalid events */
  if(iwe->len <= IW_EV_LCP_PK_LEN)
    return(-1);

  /* Get the type and length of that event */
  if(iwe->cmd <= SIOCIWLAST)
    {
      cmd_index = iwe->cmd - SIOCIWFIRST;
      if(cmd_index < standard_ioctl_num)
    descr = &(standard_ioctl_descr[cmd_index]);
    }
  else
    {
      cmd_index = iwe->cmd - IWEVFIRST;
      if(cmd_index < standard_event_num)
    descr = &(standard_event_descr[cmd_index]);
    }
  if(descr != NULL)
    event_type = descr->header_type;
  /* Unknown events -> event_type=0 => IW_EV_LCP_PK_LEN */
  event_len = event_type_size[event_type];
  /* Fixup for earlier version of WE */
  if((we_version <= 18) && (event_type == ATH_WE_HEADER_TYPE_POINT))
    event_len += IW_EV_POINT_OFF;

  /* Check if we know about this event */
  if(event_len <= IW_EV_LCP_PK_LEN)
    {
      /* Skip to next event */
      list->current += iwe->len;
      return(2);
    }
  event_len -= IW_EV_LCP_PK_LEN;

  /* Set pointer on data */
  if(list->value != NULL)
    pointer = list->value;          /* Next value in event */
  else
    pointer = list->current + IW_EV_LCP_PK_LEN; /* First value in event */

  /* Copy the rest of the event (at least, fixed part) */
  if((pointer + event_len) > list->end)
    {
      /* Go to next event */
      list->current += iwe->len;
      return(-2);
    }
  /* Fixup for WE-19 and later : pointer no longer in the list */
  /* Beware of alignement. Dest has local alignement, not packed */
  if((we_version > 18) && (event_type == ATH_WE_HEADER_TYPE_POINT))
    memcpy((char *) iwe + IW_EV_LCP_LEN + IW_EV_POINT_OFF,
       pointer, event_len);
  else
    memcpy((char *) iwe + IW_EV_LCP_LEN, pointer, event_len);

  /* Skip event in the stream */
  pointer += event_len;

  /* Special processing for iw_point events */
  if(event_type == ATH_WE_HEADER_TYPE_POINT)
    {
      /* Check the length of the payload */
      unsigned int  extra_len = iwe->len - (event_len + IW_EV_LCP_PK_LEN);
      if(extra_len > 0)
    {
      /* Set pointer on variable part (warning : non aligned) */
      iwe->u.data.pointer = pointer;

      /* Check that we have a descriptor for the command */
      if(descr == NULL)
        /* Can't check payload -> unsafe... */
        iwe->u.data.pointer = NULL; /* Discard paylod */
      else
        {
          /* Those checks are actually pretty hard to trigger,
           * because of the checks done in the kernel... */

          unsigned int  token_len = iwe->u.data.length * descr->token_size;

          /* Ugly fixup for alignement issues.
           * If the kernel is 64 bits and userspace 32 bits,
           * we have an extra 4+4 bytes.
           * Fixing that in the kernel would break 64 bits userspace. */
          if((token_len != extra_len) && (extra_len >= 4))
        {
          A_UINT16      alt_dlen = *((A_UINT16 *) pointer);
          unsigned int  alt_token_len = alt_dlen * descr->token_size;
          if((alt_token_len + 8) == extra_len)
            {
              /* Ok, let's redo everything */
              pointer -= event_len;
              pointer += 4;
              /* Dest has local alignement, not packed */
              memcpy((char*) iwe + IW_EV_LCP_LEN + IW_EV_POINT_OFF,
                 pointer, event_len);
              pointer += event_len + 4;
              iwe->u.data.pointer = pointer;
              token_len = alt_token_len;
            }
        }

          /* Discard bogus events which advertise more tokens than
           * what they carry... */
          if(token_len > extra_len)
        iwe->u.data.pointer = NULL; /* Discard paylod */
          /* Check that the advertised token size is not going to
           * produce buffer overflow to our caller... */
          if((iwe->u.data.length > descr->max_tokens)
         && !(descr->flags & ATH_WE_DESCR_FLAG_NOMAX))
        iwe->u.data.pointer = NULL; /* Discard paylod */
          /* Same for underflows... */
          if(iwe->u.data.length < descr->min_tokens)
        iwe->u.data.pointer = NULL; /* Discard paylod */
        }
    }
      else
    /* No data */
    iwe->u.data.pointer = NULL;

      /* Go to next event */
      list->current += iwe->len;
    }
  else
    {
      /* Ugly fixup for alignement issues.
       * If the kernel is 64 bits and userspace 32 bits,
       * we have an extra 4 bytes.
       * Fixing that in the kernel would break 64 bits userspace. */
      if((list->value == NULL)
     && ((((iwe->len - IW_EV_LCP_PK_LEN) % event_len) == 4)
         || ((iwe->len == 12) && ((event_type == ATH_WE_HEADER_TYPE_UINT) ||
                      (event_type == ATH_WE_HEADER_TYPE_QUAL))) ))
    {
      pointer -= event_len;
      pointer += 4;
      /* Beware of alignement. Dest has local alignement, not packed */
      memcpy((char *) iwe + IW_EV_LCP_LEN, pointer, event_len);
      pointer += event_len;
    }

      /* Is there more value in the event ? */
      if((pointer + event_len) <= (list->current + iwe->len))
    /* Go to next value */
    list->value = pointer;
      else
    {
      /* Go to next event */
      list->value = NULL;
      list->current += iwe->len;
    }
    }
  return(1);
}
#endif /*#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21) */

static void
event_wireless(A_INT8 *data, int len)
{
  A_INT8 *pos, *end, *custom, *buf;
  A_UINT16 eventid;
  A_STATUS status;

  pos = data;
  end = data + len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
  struct iw_event   iwe;
  struct event_list list;
  int           ret;

  /* Cleanup */
  memset((char *) &list, '\0', sizeof(struct event_list));

  /* Set things up */
  list.current = data;
  list.end = data + len;

  do
  {
      /* Extract an event and print it */
      ret = app_extract_events(&list, &iwe);
      if(ret != 0)
      {
          RECEVENT_DEBUG_PRINTF("\n cmd = %x, length = %d, ",iwe.cmd,iwe.u.data.length);
            
          switch (iwe.cmd) {
              case SIOCGIWAP:
        RECEVENT_DEBUG_PRINTF("event = new AP: "
                    "%02x:%02x:%02x:%02x:%02x:%02x" ,
                MAC2STR((__u8 *) iwe.u.ap_addr.sa_data));
        if (memcmp(iwe.u.ap_addr.sa_data,
                       "\x00\x00\x00\x00\x00\x00", 6) == 0
                ||
        memcmp(iwe.u.ap_addr.sa_data,
                       "\x44\x44\x44\x44\x44\x44", 6) == 0)
                {
                    RECEVENT_DEBUG_PRINTF(" Disassociated\n");
                }
                else
                {
                    RECEVENT_DEBUG_PRINTF(" Associated\n");
                }
             break;
             case IWEVCUSTOM:
                custom = pos + IW_EV_POINT_LEN;
                if (custom + iwe.u.data.length > end)
                    return;
                buf = malloc(iwe.u.data.length + 1);
                if (buf == NULL) return;
                memcpy(buf, custom, iwe.u.data.length);
                eventid = *((A_UINT16*)buf);
                RECEVENT_DEBUG_PRINTF("\n eventid = %x",eventid);

                switch (eventid) {
                case (WMI_READY_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Ready, len = %d\n", iwe.u.data.length);
                    status = app_wmiready_event_rx((A_UINT8 *)(buf + ID_LEN), iwe.u.data.length - ID_LEN);
                    break;
                case (WMI_CONNECT_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Connect, len = %d\n", iwe.u.data.length);
                    status = app_connect_event_rx((A_UINT8 *)(buf + ID_LEN), iwe.u.data.length - ID_LEN);
                    break;
                case (WMI_DISCONNECT_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Disconnect, len = %d\n", iwe.u.data.length);
                    status = app_disconnect_event_rx((A_UINT8 *)(buf + ID_LEN), iwe.u.data.length - ID_LEN);
                    break;
                case (WMI_PSTREAM_TIMEOUT_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Pstream Timeout, len = %d\n", iwe.u.data.length);
                    status = app_pstream_timeout_event_rx((A_UINT8 *)(buf + ID_LEN), iwe.u.data.length - ID_LEN);
                    break;
                case (WMI_ERROR_REPORT_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Error Report, len = %d\n", iwe.u.data.length);
                    status = app_reportError_event_rx((A_UINT8 *)(buf + ID_LEN), iwe.u.data.length - ID_LEN);
                    break;
                case (WMI_RSSI_THRESHOLD_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Rssi Threshold, len = %d\n", iwe.u.data.length);
                    status = app_rssi_threshold_event_rx((A_UINT8 *)(buf + ID_LEN), iwe.u.data.length - ID_LEN);
                    break;
                case (WMI_SCAN_COMPLETE_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Scan Complete, len = %d\n", iwe.u.data.length);
                    status = app_scan_complete_event_rx((A_UINT8 *)(buf + ID_LEN), iwe.u.data.length - ID_LEN);
                    break;
                case (WMI_TX_RETRY_ERR_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Tx Retry Err, len = %d\n", iwe.u.data.length);
                    break;
                case (WMIX_HB_CHALLENGE_RESP_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Challenge Resp, len = %d\n", iwe.u.data.length);
                    status = app_challenge_resp_event_rx((A_UINT8 *)(buf + ID_LEN), iwe.u.data.length - ID_LEN);
                    break;
                case (WMIX_DBGLOG_EVENTID):
                    status = app_target_debug_event_rx((A_INT8 *)(buf + ID_LEN), iwe.u.data.length - ID_LEN);
                    break;
                default:
                    RECEVENT_DEBUG_PRINTF("Host received other event with id 0x%x\n",
                                     eventid);
                    break;
                }
                free(buf);
            break;
        case SIOCGIWSCAN:
            RECEVENT_DEBUG_PRINTF("event = SCAN: \n");
            break;
        case SIOCSIWESSID:
            RECEVENT_DEBUG_PRINTF("event = ESSID: ");
            custom = pos + IW_EV_POINT_LEN;
            if (custom + iwe.u.data.length > end)
                return;
            buf = malloc(iwe.u.data.length + 1);
            if (buf == NULL) return;
            memcpy(buf, custom, iwe.u.data.length);
            buf[iwe.u.data.length] = '\0';
            RECEVENT_DEBUG_PRINTF("%s\n", buf);
            free(buf);
            break;
        case IWEVGENIE:
            custom = pos + IW_EV_POINT_LEN;
            if (custom + iwe.u.data.length > end)
                return;
            buf = malloc(iwe.u.data.length + 1);
            if (buf == NULL) return;
            memcpy(buf, custom, iwe.u.data.length);
            eventid = *((A_UINT16*)buf);

            switch (eventid) {
            case (WMI_BSSINFO_EVENTID):
                RECEVENT_DEBUG_PRINTF("event = Wmi Bss Info, len = %d\n", iwe.u.data.length);
                status = app_bssInfo_event_rx((A_UINT8 *)buf + ID_LEN, iwe.u.data.length - ID_LEN);
                break;
            default:
                RECEVENT_DEBUG_PRINTF("Host received other generic event with id 0x%x\n", eventid);
                break;
            }
            free(buf);
        break;

        default:
            RECEVENT_DEBUG_PRINTF("event = Others\n");
            break;
      }
    }
    }
    while(ret > 0);

#else

    struct iw_event iwe_buf, *iwe = &iwe_buf;

    while (pos + IW_EV_LCP_LEN <= end) {
        /* Event data may be unaligned, so make a local, aligned copy
         * before processing. */
        memcpy((char*)&iwe_buf, pos, sizeof(struct iw_event));
#if 0
        RECEVENT_DEBUG_PRINTF("Wireless event: cmd=0x%x len=%d\n",
               iwe->cmd, iwe->len);
#endif
        if (iwe->len <= IW_EV_LCP_LEN)
            return;
        switch (iwe->cmd) {
        case SIOCGIWAP:
            RECEVENT_DEBUG_PRINTF("event = new AP: "
                   "%02x:%02x:%02x:%02x:%02x:%02x" ,
                   MAC2STR((__u8 *) iwe->u.ap_addr.sa_data));
            if (memcmp(iwe->u.ap_addr.sa_data,
                   "\x00\x00\x00\x00\x00\x00", 6) == 0
                ||
                memcmp(iwe->u.ap_addr.sa_data,
                   "\x44\x44\x44\x44\x44\x44", 6) == 0)
            {
                RECEVENT_DEBUG_PRINTF(" Disassociated\n");
            }
            else
            {
                RECEVENT_DEBUG_PRINTF(" Associated\n");
            }
            break;
        case IWEVCUSTOM:
            custom = pos + IW_EV_POINT_LEN;
            if (custom + iwe->u.data.length > end)
                return;
            buf = malloc(iwe->u.data.length + 1);
            if (buf == NULL) return;
            memcpy(buf, custom, iwe->u.data.length);
            #if 0
            buf[iwe->u.data.length] = '\0';
            RECEVENT_DEBUG_PRINTF("%s\n", buf);
            #endif
            /* we send all the event content to the APP, the first two bytes is
             * event ID, then is the content of the event.
             */
            {
                eventid = *((A_UINT16*)buf);
                switch (eventid) {
                case (WMI_READY_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Ready, len = %d\n", iwe->u.data.length);
                    status = app_wmiready_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_CONNECT_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Connect, len = %d\n", iwe->u.data.length);
                    status = app_connect_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_DISCONNECT_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Disconnect, len = %d\n", iwe->u.data.length);
                    status = app_disconnect_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_BSSINFO_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Bss Info, len = %d\n", iwe->u.data.length);
                    status = app_bssInfo_event_rx((A_UINT8 *)buf + ID_LEN, iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_PSTREAM_TIMEOUT_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Pstream Timeout, len = %d\n", iwe->u.data.length);
                    status = app_pstream_timeout_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_ERROR_REPORT_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Error Report, len = %d\n", iwe->u.data.length);
                    status = app_reportError_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_RSSI_THRESHOLD_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Rssi Threshold, len = %d\n", iwe->u.data.length);
                    status = app_rssi_threshold_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_SCAN_COMPLETE_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Scan Complete, len = %d\n", iwe->u.data.length);
                    status = app_scan_complete_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMI_TX_RETRY_ERR_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Tx Retry Err, len = %d\n", iwe->u.data.length);
                    break;
                case (WMIX_HB_CHALLENGE_RESP_EVENTID):
                    RECEVENT_DEBUG_PRINTF("event = Wmi Challenge Resp, len = %d\n", iwe->u.data.length);
                    status = app_challenge_resp_event_rx((A_UINT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                case (WMIX_DBGLOG_EVENTID):
                    status = app_target_debug_event_rx((A_INT8 *)(buf + ID_LEN), iwe->u.data.length - ID_LEN);
                    break;
                default:
                    RECEVENT_DEBUG_PRINTF("Host received other event with id 0x%x\n",
                                     eventid);
                    break;
                }
            }
            free(buf);
            break;
        case SIOCGIWSCAN:
            RECEVENT_DEBUG_PRINTF("event = SCAN: \n");
            break;
        case SIOCSIWESSID:
            RECEVENT_DEBUG_PRINTF("event = ESSID: ");
            custom = pos + IW_EV_POINT_LEN;
            if (custom + iwe->u.data.length > end)
                return;
            buf = malloc(iwe->u.data.length + 1);
            if (buf == NULL) return;
            memcpy(buf, custom, iwe->u.data.length);
            buf[iwe->u.data.length] = '\0';
            RECEVENT_DEBUG_PRINTF("%s\n", buf);
            free(buf);
            break;
        default:
            RECEVENT_DEBUG_PRINTF("event = Others\n");
        }

        pos += iwe->len;
   }
#endif /*#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21) */
}

static A_STATUS app_wmiready_event_rx(A_UINT8 *datap, int len)
{
    WMI_READY_EVENT *ev;

    if (len < sizeof(WMI_READY_EVENT)) {
        return A_EINVAL;
    }
    ev = (WMI_READY_EVENT *)datap;
    RECEVENT_DEBUG_PRINTF("\nApplication receive wmi ready event:\n");
    RECEVENT_DEBUG_PRINTF("mac address =  %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x ",
              ev->macaddr[0], ev->macaddr[1], ev->macaddr[2], ev->macaddr[3],
              ev->macaddr[4], ev->macaddr[5]);
    RECEVENT_DEBUG_PRINTF("Physical capability = %d\n",ev->phyCapability);
    return A_OK;
}

static A_STATUS app_connect_event_rx(A_UINT8 *datap, int len)
{
    WMI_CONNECT_EVENT *ev;
    A_UINT16 i, assoc_resp_ie_pos, assoc_req_ie_pos;;

    if (len < sizeof(WMI_CONNECT_EVENT)) {
        return A_EINVAL;
    }
    ev = (WMI_CONNECT_EVENT *)datap;

    RECEVENT_DEBUG_PRINTF("\nApplication receive connected event on freq %d \n", ev->channel);
    RECEVENT_DEBUG_PRINTF("with bssid %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x "
            " listenInterval=%d, assocReqLen=%d assocRespLen =%d\n",
             ev->bssid[0], ev->bssid[1], ev->bssid[2],
             ev->bssid[3], ev->bssid[4], ev->bssid[5],
             ev->listenInterval, ev->assocReqLen, ev->assocRespLen);

    /*
     * The complete Association Response Frame is delivered to the host
     */
    RECEVENT_DEBUG_PRINTF("Association Request frame: ");
    for (i = 0; i < ev->assocReqLen; i++)
    {
        if (!(i % 0x10)) {
            RECEVENT_DEBUG_PRINTF("\n");
        }
        RECEVENT_DEBUG_PRINTF("%2.2x ", ev->assocInfo[i]);
    }
    RECEVENT_DEBUG_PRINTF("\n");
    assoc_req_ie_pos = sizeof(A_UINT16)  +  /* capinfo*/
                       sizeof(A_UINT16);    /* listen interval */
    RECEVENT_DEBUG_PRINTF("AssocReqIEs: ");
    for (i = assoc_req_ie_pos; i < ev->assocReqLen; i++)
    {
        if (!((i- assoc_req_ie_pos) % 0x10)) {
            RECEVENT_DEBUG_PRINTF("\n");
        }
        RECEVENT_DEBUG_PRINTF("%2.2x ", ev->assocInfo[i]);
    }
    RECEVENT_DEBUG_PRINTF("\n");

    RECEVENT_DEBUG_PRINTF("Association Response frame: ");
    for (i = ev->assocReqLen; i < (ev->assocReqLen + ev->assocRespLen); i++)
    {
        if (!((i-ev->assocReqLen) % 0x10)) {
            RECEVENT_DEBUG_PRINTF("\n");
        }
        RECEVENT_DEBUG_PRINTF("%2.2x ", ev->assocInfo[i]);
    }
    RECEVENT_DEBUG_PRINTF("\n");

    assoc_resp_ie_pos = sizeof(struct ieee80211_frame) +
                        sizeof(A_UINT16)  +  /* capinfo*/
                        sizeof(A_UINT16)  +  /* status Code */
                        sizeof(A_UINT16)  ;  /* associd */
    RECEVENT_DEBUG_PRINTF("AssocRespIEs: ");
    for (i = ev->assocReqLen + assoc_resp_ie_pos;
             i < (ev->assocReqLen + ev->assocRespLen); i++)
    {
        if (!((i- ev->assocReqLen- assoc_resp_ie_pos) % 0x10)) {
            RECEVENT_DEBUG_PRINTF("\n");
        }
        RECEVENT_DEBUG_PRINTF("%2.2x ", ev->assocInfo[i]);
    }
    RECEVENT_DEBUG_PRINTF("\n");

    return A_OK;
}

static A_STATUS app_disconnect_event_rx(A_UINT8 *datap, int len)
{
    WMI_DISCONNECT_EVENT *ev;
    A_UINT16 i;

    if (len < sizeof(WMI_DISCONNECT_EVENT)) {
        return A_EINVAL;
    }

    ev = (WMI_DISCONNECT_EVENT *)datap;
    RECEVENT_DEBUG_PRINTF("\nApplication receive disconnected event: reason is %d protocol reason/status code is %d\n",
            ev->disconnectReason, ev->protocolReasonStatus);
    RECEVENT_DEBUG_PRINTF("Disconnect from %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x ",
              ev->bssid[0], ev->bssid[1], ev->bssid[2], ev->bssid[3],
              ev->bssid[4], ev->bssid[5]);

    RECEVENT_DEBUG_PRINTF("\nAssocResp Frame = %s",
                    ev->assocRespLen ? " " : "NULL");
    for (i = 0; i < ev->assocRespLen; i++) {
        if (!(i % 0x10)) {
            RECEVENT_DEBUG_PRINTF("\n");
        }
        RECEVENT_DEBUG_PRINTF("%2.2x ", datap[i]);
    }
    RECEVENT_DEBUG_PRINTF("\n");
    return A_OK;
}

static A_STATUS app_bssInfo_event_rx(A_UINT8 *datap, int len)
{
    WMI_BSS_INFO_HDR *bih;

    if (len <= sizeof(WMI_BSS_INFO_HDR)) {
        return A_EINVAL;
    }
    bih = (WMI_BSS_INFO_HDR *)datap;
    RECEVENT_DEBUG_PRINTF("\nApplication receive BSS info event:\n");
    RECEVENT_DEBUG_PRINTF("channel = %d, frame type = %d, snr = %d rssi = %d.\n",
            bih->channel, bih->frameType, bih->snr, bih->rssi);
    RECEVENT_DEBUG_PRINTF("BSSID is: %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x \n",
              bih->bssid[0], bih->bssid[1], bih->bssid[2], bih->bssid[3],
              bih->bssid[4], bih->bssid[5]);
    return A_OK;
}

static A_STATUS app_pstream_timeout_event_rx(A_UINT8 *datap, int len)
{
    WMI_PSTREAM_TIMEOUT_EVENT *ev;

    if (len < sizeof(WMI_PSTREAM_TIMEOUT_EVENT)) {
        return A_EINVAL;
    }
    ev = (WMI_PSTREAM_TIMEOUT_EVENT *)datap;
    RECEVENT_DEBUG_PRINTF("\nApplication receive pstream timeout event:\n");
    RECEVENT_DEBUG_PRINTF("streamID= %d\n", ev->trafficClass);
    return A_OK;
}

static A_STATUS app_reportError_event_rx(A_UINT8 *datap, int len)
{
    WMI_TARGET_ERROR_REPORT_EVENT *reply;

    if (len < sizeof(WMI_TARGET_ERROR_REPORT_EVENT)) {
        return A_EINVAL;
    }
    reply = (WMI_TARGET_ERROR_REPORT_EVENT *)datap;
    RECEVENT_DEBUG_PRINTF("\nApplication receive report error event\n");
    RECEVENT_DEBUG_PRINTF("error value is %d\n",reply->errorVal);

    /* Initiate recovery if its a fatal error */
    if (reply->errorVal & WMI_TARGET_FATAL_ERR) {
        /* Reset the ar6000 module in the driver */
        if (optionflag & RESTORE_FLAG) {
            printf("Executing script: %s\n", restorefile);
            system(restorefile);
        }
    }

    return A_OK;
}

static A_STATUS
app_rssi_threshold_event_rx(A_UINT8 *datap, int len)
{
    USER_RSSI_THOLD *evt;

    if (len < sizeof(USER_RSSI_THOLD)) {
        return A_EINVAL;
    }
    evt = (USER_RSSI_THOLD*)datap;
    RECEVENT_DEBUG_PRINTF("\nApplication receive rssi threshold event\n");
    RECEVENT_DEBUG_PRINTF("tag is %d, rssi is %d\n", evt->tag, evt->rssi);

    return A_OK;
}

static A_STATUS
app_scan_complete_event_rx(A_UINT8 *datap, int len)
{
    RECEVENT_DEBUG_PRINTF("\nApplication receive scan complete event\n");

    return A_OK;
}

static A_STATUS
app_challenge_resp_event_rx(A_UINT8 *datap, int len)
{
    A_UINT32 cookie;

    memcpy(&cookie, datap, len);
    RECEVENT_DEBUG_PRINTF("\nApplication receive challenge response event: 0x%x\n", cookie);

    return A_OK;
}

static A_STATUS
app_target_debug_event_rx(A_INT8 *datap, int len)
{
#define BUF_SIZE    120
    A_UINT32 count;
    A_UINT32 timestamp;
    A_UINT32 debugid;
    A_UINT32 numargs;
    A_UINT32 moduleid;
    A_INT32 *buffer;
    A_UINT32 length;
    char *tm, buf[BUF_SIZE];
    long curpos;
    static int numOfRec = 0;

#ifdef DBGLOG_DEBUG
    RECEVENT_DEBUG_PRINTF("Application received target debug event: %d\n", len);
#endif /* DBGLOG_DEBUG */
    count = 0;
    buffer = (A_INT32 *)datap;
    length = (len >> 2);
    while (count < length) {
        debugid = DBGLOG_GET_DBGID(buffer[count]);
        moduleid = DBGLOG_GET_MODULEID(buffer[count]);
        numargs = DBGLOG_GET_NUMARGS(buffer[count]);
        timestamp = DBGLOG_GET_TIMESTAMP(buffer[count]);
        GET_CURRENT_TIME(tm);
        switch (numargs) {
            case 0:
            fprintf(fpout, "%s: %s (%d)\n", tm,
                    dbglog_id_tag[moduleid][debugid],
                    timestamp);
#ifdef DBGLOG_DEBUG
            RECEVENT_DEBUG_PRINTF("%s (%d)\n",
                                  dbglog_id_tag[moduleid][debugid],
                                  timestamp);
#endif /* DBGLOG_DEBUG */
            break;

            case 1:
            fprintf(fpout, "%s: %s (%d): 0x%x\n", tm,
                    dbglog_id_tag[moduleid][debugid],
                    timestamp, buffer[count+1]);
#ifdef DBGLOG_DEBUG
            RECEVENT_DEBUG_PRINTF("%s (%d): 0x%x\n",
                                  dbglog_id_tag[moduleid][debugid],
                                  timestamp, buffer[count+1]);
#endif /* DBGLOG_DEBUG */
            break;

            case 2:
            fprintf(fpout, "%s: %s (%d): 0x%x, 0x%x\n", tm,
                    dbglog_id_tag[moduleid][debugid],
                    timestamp, buffer[count+1],
                    buffer[count+2]);
#ifdef DBGLOG_DEBUG
            RECEVENT_DEBUG_PRINTF("%s (%d): 0x%x, 0x%x\n",
                                  dbglog_id_tag[moduleid][debugid],
                                  timestamp, buffer[count+1],
                                  buffer[count+2]);
#endif /* DBGLOG_DEBUG */
            break;

            default:
            RECEVENT_DEBUG_PRINTF("Invalid args: %d\n", numargs);
        }
        count += (numargs + 1);

        numOfRec++;
        if(dbgRecLimit && (numOfRec % dbgRecLimit == 0)) {
            /* Once record limit is hit, rewind to start
             * after 8 bytes from start
             */
            numOfRec = 0;
            curpos = ftell(fpout);
            truncate(dbglogoutfile, curpos);
            rewind(fpout);
            fseek(fpout, 8, SEEK_SET);
            fprintf(fpout, "\n");
        }
    }

    /* Update the last rec at the top of file */
    curpos = ftell(fpout);
    if( fgets(buf, BUF_SIZE, fpout) ) {
        buf[BUF_SIZE - 1] = 0;  /* In case string is longer from logs */
        length = strlen(buf);
        memset(buf, ' ', length-1);
        buf[length] = 0;
        fseek(fpout, curpos, SEEK_SET);
        fprintf(fpout, "%s", buf);
    }

    rewind(fpout);
    /* Update last record */
    fprintf(fpout, "%08d\n", numOfRec);
    fseek(fpout, curpos, SEEK_SET);
    fflush(fpout);

#undef BUF_SIZE
    return A_OK;
}
