#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include "map_lib.h"

// Max number of parameters in LXB file that we handle
#define MAX_PAR       99
// Max chars needed to print MAX_PAR (must be updated when MAX_PAR is!)
#define MAX_PAR_CHARS 2

typedef struct {
    int begin_text, end_text;
    int begin_data, end_data;
    int begin_analysis, end_analysis;
} fcs_header;

static int verbose = true;

static struct option longopts[] = {
    { "silent" , no_argument       , &verbose , false } ,
    { "help"   , no_argument       , NULL     , 'h' }   ,
    { NULL     , 0                 , NULL     , 0 }
};

static int par_mask[MAX_PAR];


char *read_file(const char *filename, long *size)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
        return NULL;

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);
    if (!(filesize > 0)) {
        fclose(fp);
        return NULL;
    }

    char *buf = malloc(filesize);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    long actual = fread(buf, 1, filesize, fp);
    fclose(fp);

    if (actual != filesize) {
        free(buf);
        return NULL;
    }

    if (size)
        *size = filesize;

    return buf;
}

const char *parameter_key(int n, char type)
{
    // Key is of format "$PXY", where len(X) <= MAX_PAR_CHARS, and Y == type,
    // also include room for null terminator.
    static char buf[MAX_PAR_CHARS+4];
    if (n < 0 || n >= MAX_PAR)
        return "";

    sprintf(buf, "$P%d%c", n+1, type);
    return buf;
}

void init_parameter_mask(map_t txt)
{
    memset(par_mask, 0, MAX_PAR*sizeof(par_mask[0]));

    int npar = map_get_int(txt, "$PAR");
    for (int i = 0; i < npar; ++i) {
        const char *key = parameter_key(i, 'R');
        par_mask[i] = map_get_int(txt, key);
        if (par_mask[i] > 0)
            --par_mask[i];
    }
}

int parameter_mask(int n)
{
    return n >= 0 && n < MAX_PAR ? par_mask[n] : 0;
}

bool parse_header(const char *data, long size, fcs_header *hdr)
{
    if (!hdr) return false;

    if (size < 58) {
        fprintf(stderr, "  Bad LXB: header data is too small (%lu)\n", size);
        return false;
    }

    if (0 != strncmp(data, "FCS3.0    ", 10)) {
        fprintf(stderr, "  Bad LXB: magic bytes do not match\n");
        return false;
    }

    bool ok = true;
    ok &= sscanf(&data[10], "%8d", &hdr->begin_text);
    ok &= sscanf(&data[18], "%8d", &hdr->end_text);
    ok &= sscanf(&data[26], "%8d", &hdr->begin_data);
    ok &= sscanf(&data[34], "%8d", &hdr->end_data);
    ok &= sscanf(&data[42], "%8d", &hdr->begin_analysis);
    ok &= sscanf(&data[50], "%8d", &hdr->end_analysis);

    if (!ok)
        fprintf(stderr, "  Bad LXB: failed to parse segment offsets\n");

    return ok;
}

map_t parse_text(const char *text, long size)
{
    if (size < 2)
        return NULL;

    map_t m = map_create();
    char *sep = strndup(text, 1);
    char *data = strndup(text+1, size-1);

    char *p = data;
    for (;;) {
        // FIXME: FCS 3.0 allows the separator character to appear in keys and
        // values by repeating the separator twice -- this is currently NOT
        // handled.
        // For example, if sep='/' then "k//ey/value/" should be parsed as
        // "k/ey"="value", whereas we parse it as { "k"="", "ey"="value" }.
        char *key = strsep(&p, sep);
        if (!key) break;
        char *val = strsep(&p, sep);
        if (!val) break;

        map_set(m, key, val);
    }

    free(data);
    free(sep);

    return m;
}

bool check_par_format(map_t txt)
{
    int npar = map_get_int(txt, "$PAR");
    if (npar > MAX_PAR) {
        fprintf(stderr, "  Unsupported LXB: too many parameters (%d)\n", npar);
        return false;
    }

    const char *data_type = map_get(txt, "$DATATYPE");
    if (strcasecmp("I", data_type) != 0) {
        fprintf(stderr, "  Unsupported LXB: data is not integral "
                "($DATATYPE=%s)\n", data_type);
        return false;
    }

    const char *mode = map_get(txt, "$MODE");
    if (strcasecmp("L", mode) != 0) {
        fprintf(stderr, "  Unsupported LXB: data not in list format "
                "($MODE=%s)\n", mode);
        return false;
    }

    const char *byteord = map_get(txt, "$BYTEORD");
    if (strcmp("1,2,3,4", byteord) != 0) {
        fprintf(stderr, "  Unsupported LXB: data not in little endian format "
                "($BYTEORD=%s)\n", byteord);
        return false;
    }

    const char *unicode = map_get(txt, "$UNICODE");
    if (*unicode) {
        // FIXME: Support Unicode.  We try to parse the data even if the text
        // segment contains Unicode characters, so don't return false here.
        fprintf(stderr, "  Unsupported LXB: Unicode flag detected,"
                " output may be corrupted\n");
    }

    init_parameter_mask(txt);

    for (int i = 0; i < npar; ++i) {
        const char *key = parameter_key(i, 'B');
        int bits = map_get_int(txt, key);
        if (bits != 32) {
            fprintf(stderr, "  Unsupported LXB: parameter %d is not 32 bits "
                    "(%s=%d)\n", i, key, bits);
            return false;
        }
    }

    return true;
}

void print_header(map_t txt)
{
    int npar = map_get_int(txt, "$PAR");

    printf("# ");
    for (int i = 0; i < npar; ++i) {
        const char *label = map_get(txt, parameter_key(i, 'S'));
        const char *size = map_get(txt, parameter_key(i, 'R'));
        printf("%s (%s)%s", label, size, i==npar-1 ? "\n" : ", ");
    }
}

void print_data(const int32_t *data, long size, map_t txt)
{
    int ntot        = map_get_int(txt, "$TOT");
    int npar        = map_get_int(txt, "$PAR");
    int niter       = 0;
    const char *p   = (const char *)data;
    const char *end = p + size; // - npar * sizeof(int32_t) + 1;

    while (p < end && niter++ < ntot) {
        const int32_t *p32 = (const int32_t *)p;
        for (int i = 0; i < npar; ++i, ++p32)
            printf("%d%s", *p32 & parameter_mask(i), i==npar-1 ? "\n" : "\t");

        p = (const char *)p32;
    }
}

void usage()
{
    fprintf(stderr,
"usage: lxbread [--silent] [--help|-h] file1 [file2 ..]\n"
"\n"
"Reads one or more LXB (Luminex bead array) files and prints a column for\n"
"each parameter.  The first row is a comma separated list of parameter\n"
"names and the maximum value each parameter can assume in parenthesis.\n"
    );
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    int n;
    while ((n = getopt_long(argc, argv, "h", longopts, NULL)) != -1) {
        switch (n) {
        case 0:
            // long option - nothing to do
            break;
        case 'h':
            // help - fall through
        default:
            usage();
            break;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1)
        usage();

    char processing_fmt[] = "Processing file [%1d of %d]: %s\n";
    if (argc >= 1000)     processing_fmt[18] = '4';
    else if (argc >= 100) processing_fmt[18] = '3';
    else if (argc >= 10)  processing_fmt[18] = '2';
    else                  processing_fmt[18] = '1';

    bool did_header = false;
    for (int i = 0; i < argc; ++i) {
        if (verbose)
            fprintf(stderr, processing_fmt, i+1, argc, argv[i]);

        long size;
        char *buf = read_file(argv[i], &size);
        if (!buf) {
            fprintf(stderr, "  Could not read file: %s\n", argv[i]);
            continue;
        }

        fcs_header hdr;
        bool ok = parse_header(buf, size, &hdr);
        if (!ok) {
            free(buf);
            continue;
        }

        long txt_size = hdr.end_text - hdr.begin_text;
        if (!(txt_size > 0 && hdr.begin_text > 0 && hdr.end_text <= size)) {
            fprintf(stderr, "  Bad LXB: could not locate TEXT segment\n");
            free(buf);
            continue;
        }

        map_t txt = parse_text(buf + hdr.begin_text, txt_size);

        if (!check_par_format(txt)) {
            map_free(txt);
            free(buf);
            continue;
        }

        if (!did_header) {
            print_header(txt);
            did_header = true;
        }

        long data_size = hdr.end_data - hdr.begin_data;
        if (!(data_size > 0 && hdr.begin_data > 0 && hdr.end_data <= size)) {
            fprintf(stderr, "  Bad LXB: could not locate DATA segment\n");
            map_free(txt);
            free(buf);
            continue;
        }

        print_data((int32_t*)(buf + hdr.begin_data), data_size, txt);

        map_free(txt);
        free(buf);
    }

    return did_header ? EXIT_SUCCESS : EXIT_FAILURE;
}
