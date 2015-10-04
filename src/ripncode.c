#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cdio/cdio.h>
#include <cdio/paranoia.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <murphy/common/macros.h>
#include <murphy/common/mm.h>
#include <murphy/common/log.h>
#include <murphy/common/debug.h>


#define ripncode_fatal(_r, ...) do {                     \
        mrp_log_error("fatal error: "__VA_ARGS__);       \
        exit(1);                                         \
    } while (0)

#define ripncode_error(_r, ...)  mrp_log_error(__VA_ARGS_)
#define ripncode_warn(_r, ...)   mrp_log_warning(__VA_ARGS__)
#define ripncode_info(_r, ...)   mrp_log_info(__VA_ARGS__)
#define ripncode_debug(_r, ...)  mrp_debug(__VA_ARGS__)


typedef struct {
    int            id;                   /* track number */
    track_format_t format;               /* track format */
    int            lsn_beg;              /* track first LSN */
    int            lsn_end;              /* track last LSN */
    int            lba;                  /* track LBA */
} rip_track_t;


typedef struct {
    CdIo_t           *cdio;              /* libcdio object for device */
    cdrom_drive_t    *cdda;              /* libcdio-cdda object for device */
    cdrom_paranoia_t *cdpa;              /* libcdio-paranoia object for device */
    int               endian;            /* endianness, 0:little, 1:big */
    int               first;             /* first track */
    int               last;              /* last track */
    rip_track_t      *tracks;
    int               ntrack;
    bool              dryrun;

    /* command line arguments */
    const char *argv0;                   /* (base)name of our executable */
    const char *device;                  /* device to rip from */
    const char *driver;                  /* cdio driver for device */
    const char *rip;                     /* tracks to rip */
    const char *format;                  /* which format to encode to */
    const char *metadata;                /* path to album metadata */
    int         log_mask;                /* what to log */
    const char *log_target;              /* where to log it to */
} ripncode_t;


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


static void print_usage(ripncode_t *r, int exit_code, const char *fmt, ...)
{
    va_list     ap;
    const char *base;

    if (fmt && *fmt) {
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf("\n");
    }

    base = argv0_base(r->argv0);

    printf("usage: %s [options]\n", base);
    printf("The possible options are:\n"
           "  -d, --device=<DEVICE>        device to rip from\n"
           "  -c, --driver=<DRIVER>        libcdio driver to use for <DEVICE>\n"
           "  -t, --tracks=<FIRST[-LAST]>  tracks to rip\n"
           "  -f, --format=<FORMAT>        format to encode tracks to \n"
           "  -m, --metadata=<FILE>        read metadata from <FILE>\n"
           "  -L, --log-level=<LEVELS>     log levels to enable\n"
           "    LEVELS is a comma-separated list of info, error and warning\n"
           "  -T, --log-target=<TARGET>    where to log messages\n"
           "    TARGET is one of stderr, stdout, syslog, or a logfile path\n"
           "  -v, --verbose                increase logging verbosity\n"
           "  -d, --debug=<SITE>           turn on debugging for the give site\n"
           "    SITE can be of the form 'function', '@file-name', or '*'\n"
           "  -h, --help                   show this help message\n");

    if (exit_code < 0)
        return;
    else
        exit(exit_code);
}


static void config_set_defaults(ripncode_t *r, const char *argv0)
{
    mrp_clear(r);
    r->argv0      = argv0;
    r->device     = "/dev/cdrom";
    r->driver     = "linux";
    r->log_mask   = MRP_LOG_UPTO(MRP_LOG_WARNING);
    r->log_target = "stdout";

    mrp_log_set_mask(r->log_mask);
    mrp_log_set_target(r->log_target);
}


static void parse_tracks(ripncode_t *r)
{
    const char *tracks = r->rip;
    char       *end;

    if (tracks == NULL || !strcmp(tracks, "all") || !strcmp(tracks, "*")) {
        r->first = r->last = -1;
        return;
    }

    r->first = strtoul(tracks, &end, 10);

    if (!*end)
        r->last = r->first;
    else {
        if (*end != '-' && *end != ':')
        invalid:
            print_usage(r, EINVAL, "invalid tracks given '%s'", tracks);

        r->last = strtoul(end + 1, &end, 10);

        if (*end && *end != '-' && *end != ':')
            goto invalid;
    }
}


static void parse_cmdline(ripncode_t *r, int argc, char **argv, char **envp)
{
#   define OPTIONS "d:c:t:f:m:L:T:v::d:nh"

    struct option options[] = {
        { "device"           , required_argument, NULL, 'd' },
        { "driver"           , required_argument, NULL, 'c' },
        { "tracks"           , required_argument, NULL, 't' },
        { "format"           , required_argument, NULL, 'f' },
        { "metadata"         , required_argument, NULL, 'm' },
        { "log-level"        , required_argument, NULL, 'L' },
        { "log-target"       , required_argument, NULL, 'T' },
        { "verbose"          , optional_argument, NULL, 'v' },
        { "debug"            , required_argument, NULL, 'd' },
        { "dry-run"          , no_argument      , NULL, 'n' },
        { "help"             , no_argument      , NULL, 'h' },
        { NULL , 0, NULL, 0 }
    };

    int opt, help;

    MRP_UNUSED(envp);

    help = FALSE;

    while ((opt = getopt_long(argc, argv, OPTIONS, options, NULL)) != -1) {
        switch (opt) {
        case 'D':
            r->device = optarg;
            if (access(r->device, R_OK) < 0)
                ripncode_fatal(r, "can't access '%s' (%d: %s).", r->device,
                               errno, strerror(errno));
            break;

        case 'c':
            r->driver = optarg;
            break;

        case 't':
            r->rip = optarg;
            parse_tracks(r);
            break;

        case 'f':
            r->format = optarg;
            break;

        case 'm':
            r->metadata = optarg;
            if (access(r->metadata, R_OK) < 0)
                ripncode_fatal(r, "can't access '%s' (%d: %s).", r->metadata,
                               errno, strerror(errno));
            break;

        case 'L':
            r->log_mask = mrp_log_parse_levels(optarg);
            mrp_log_set_mask(r->log_mask);
            break;

        case 'v':
            r->log_mask <<= 1;
            r->log_mask  |= 1;
            mrp_log_set_mask(r->log_mask);
            break;

        case 'T':
            r->log_target = optarg;
            mrp_log_set_target(r->log_target);
            break;

        case 'd':
            r->log_mask |= MRP_LOG_MASK_DEBUG;
            mrp_log_set_mask(r->log_mask);
            mrp_debug_set_config(optarg);
            mrp_debug_enable(TRUE);
            break;

        case 'n':
            r->dryrun = 1;
            break;

        case 'h':
            help++;
            break;

        default:
            print_usage(r, EINVAL, "invalid option '%c'", opt);
        }
    }

    if (help) {
        print_usage(r, -1, "");
        exit(0);
    }
}


static driver_id_t driver_id(ripncode_t *r)
{
    struct {
        const char  *name;
        driver_id_t  id;
    } drivers[] = {
#define MAP(_name, _id) { #_name, DRIVER_##_id }
        MAP(device , DEVICE ),
        MAP(linux  , LINUX  ),
        MAP(aix    , AIX    ),
        MAP(bsdi   , BSDI   ),
        MAP(netbsd , NETBSD ),
        MAP(solaris, SOLARIS),
        MAP(cdrdao , CDRDAO ),
        MAP(bincue , BINCUE ),
        MAP(nrg    , NRG    ),
#undef MAP
        { NULL, -1 }
    }, *d;

    if (r->driver == NULL)
        r->driver = "device";

    for (d = drivers; d->name != NULL; d++)
        if (!strcmp(d->name, r->driver))
            return d->id;

    return DRIVER_UNKNOWN;
}


static const char *track_format_name(int format)
{
    switch (format) {
    case TRACK_FORMAT_AUDIO: return "audio";
    case TRACK_FORMAT_CDI:   return "CD-i";
    case TRACK_FORMAT_XA:    return "Mode2 of some sort";
    case TRACK_FORMAT_DATA:  return "Mode1 of some sort";
    case TRACK_FORMAT_PSX:   return "Playstation CD";
    default:                 return "unknown";
    }
}


static void device_open(ripncode_t *r)
{
    char *msg = NULL;

    r->cdio = cdio_open(r->device, driver_id(r));

    if (r->cdio == NULL)
        ripncode_fatal(r, "failed to open CdIo device '%s' with driver '%s'",
                       r->device, r->driver);

    r->cdda = cdio_cddap_identify_cdio(r->cdio, CDDA_MESSAGE_LOGIT, &msg);

    if (r->cdda == NULL)
        ripncode_fatal(r, "failed to identify CDDA device '%s' (%s)", r->device,
                       msg ? msg : "unknown error");

    cdio_cddap_free_messages(msg);

    if (cdio_cddap_open(r->cdda) < 0)
        ripncode_fatal(r, "failed to open CDDA device '%s' (%d: %s)", r->device,
                       errno, strerror(errno));

    r->cdpa = cdio_paranoia_init(r->cdda);

    if (r->cdpa == NULL)
        ripncode_fatal(r, "failed to open CD-Paranoia device '%s'", r->device);

    r->endian = data_bigendianp(r->cdda);

    ripncode_info(r, "Drive endianness: %d\n", r->endian);
}


static void read_status(long int i, paranoia_cb_mode_t mode)
{
    printf("\roffset: %ld, mode: 0x%x (%s)", i, mode,
           paranoia_cb_mode2str[mode]);
    fflush(stdout);
}


static void get_track_info(ripncode_t *r)
{
    rip_track_t *t;
    int          i, base;
    char         file[1024], *msg;
    int16_t     *samples;
    int          fd, n, l, total;

    base      = cdio_get_first_track_num(r->cdio);
    r->ntrack = cdio_get_num_tracks(r->cdio);
    r->tracks = mrp_allocz_array(rip_track_t, r->ntrack);

    if (r->tracks == NULL && r->ntrack > 0)
        ripncode_fatal(r, "failed to allocate track info");

    cdio_paranoia_modeset(r->cdpa, PARANOIA_MODE_FULL);

    for (i = 0, t = r->tracks; i < r->ntrack; i++, t++) {
        if (i >= r->last)
            break;
        t->id      = base + i;
        t->format  = cdio_get_track_format(r->cdio, base + i);
        t->lsn_beg = cdio_get_track_lsn(r->cdio, base + i);
        t->lsn_end = cdio_get_track_last_lsn(r->cdio, base + i);
        t->lba     = cdio_get_track_lba(r->cdio, base + i);

        printf("track #%d (id %d): lsn: %u - %u (lba %u), format: %s\n",
               i, base + i, t->lsn_beg, t->lsn_end, t->lba,
               track_format_name(t->format));

        if (r->dryrun)
            continue;

        snprintf(file, sizeof(file), "track-%d.raw", i);
        if ((fd = open(file, O_WRONLY|O_CREAT, 0644)) < 0)
            ripncode_fatal(r, "failed to open '%s' (%d: %s)", file,
                           errno, strerror(errno));

        cdio_paranoia_seek(r->cdpa, t->lsn_beg * CDIO_CD_FRAMESIZE_RAW,
                           SEEK_SET);

        total = t->lsn_end - t->lsn_beg + 1;
        for (l = 0; l <= total; l++) {
            printf("\rreading track sector %d/%d (#%d)...", l, total,
                   t->lsn_beg + l);
            fflush(stdout);

            samples = cdio_paranoia_read(r->cdpa, read_status);

            if (samples == NULL) {
                msg = cdio_cddap_errors(r->cdda);
                ripncode_fatal(r, "failed to seek to LSN %d (%s)", t->lsn_beg,
                               msg ? msg : "unknown error");
            }

            n = write(fd, samples, CDIO_CD_FRAMESIZE_RAW);

            if (n < 0)
                ripncode_fatal(r, "failed to write sector data (%d: %s)",
                               errno, strerror(errno));
        }

        close(fd);
    }
}


int main(int argc, char *argv[], char **envp)
{
    ripncode_t r;

    config_set_defaults(&r, argv[0]);
    parse_cmdline(&r, argc, argv, envp);

    device_open(&r);
    get_track_info(&r);

    return 0;
}




#if 0
int
main(int argc, const char *argv[])
{
    CdIo_t *p_cdio = cdio_open ("/dev/cdrom", DRIVER_LINUX);
    track_t i_first_track;
    track_t i_tracks;
    int j, i;


    if (NULL == p_cdio) {
        printf("Couldn't find a driver.. leaving.\n");
        return 1;
    }

    printf("Disc last LSN: %d\n", cdio_get_disc_last_lsn(p_cdio));

    i_tracks      = cdio_get_num_tracks(p_cdio);
    i_first_track = i = cdio_get_first_track_num(p_cdio);

    printf("CD-ROM Track List (%i - %i)\n", i_first_track,
           i_first_track+i_tracks-1);

    printf("  #:  LSN\n");

    for (j = 0; j < i_tracks; i++, j++) {
        lsn_t lsn = cdio_get_track_lsn(p_cdio, i);
        if (CDIO_INVALID_LSN != lsn)
            printf("%3d: %06lu\n", (int) i, (long unsigned int) lsn);
    }
    printf("%3X: %06lu  leadout\n", CDIO_CDROM_LEADOUT_TRACK,
           (long unsigned int) cdio_get_track_lsn(p_cdio,
                                                  CDIO_CDROM_LEADOUT_TRACK));
    cdio_destroy(p_cdio);
    return 0;
}
#endif
