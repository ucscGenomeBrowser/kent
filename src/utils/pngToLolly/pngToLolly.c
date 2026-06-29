/* pngToLolly - turn a PNG image into a "stemless lolly" bigLolly track, so the
 * Genome Browser renders the picture as a mosaic of colored dots.  Each opaque
 * pixel becomes one lolly: its column maps to a genomic position, its row maps
 * to the lolly value (vertical position), and its color becomes itemRgb.  The
 * lolly's size can be varied by the amount of local detail (variation) in that
 * part of the image.
 *
 * The program writes everything needed for a self-contained assembly hub
 * (BED, autoSql, chrom.sizes, a blank synthetic genome, trackDb, hub.txt,
 * genomes.txt) plus a tiny makeHub.sh that runs faToTwoBit and bedToBigBed. */

/* png.h must come before common.h because of setjmp checking in pngconf.h */
#include "png.h"
#include <sys/stat.h>
#include "common.h"
#include "linefile.h"
#include "portable.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "obscure.h"

/* The synthetic single-chromosome assembly the dots live on. */
#define CHROM_NAME "chrImg"
#define GENOME_DB  "lollyImg"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pngToLolly - render a PNG as a stemless-lolly bigLolly track (a dot mosaic).\n"
  "usage:\n"
  "   pngToLolly in.png outDir\n"
  "Writes a ready-to-build assembly hub into outDir.  Then:\n"
  "   cd outDir && ./makeHub.sh\n"
  "   # move outDir under ~/public_html and load the printed hubUrl\n"
  "options:\n"
  "   -maxDim=N     longest side of the image after downscaling (default 64).\n"
  "                 Smaller values give smoother lolly-size variation; larger\n"
  "                 values give more spatial detail.\n"
  "   -dotSize=N    on-screen dot diameter in pixels (default 8).\n"
  "   -sizeVar=N    neighborhood radius used to measure local variation\n"
  "                 (default 1, i.e. a 3x3 window).  0 turns off size variation.\n"
  "   -sizeLo=F     smallest dot radius, as a fraction of the tiling radius\n"
  "                 (default 0.65); used for ordinarily-busy areas.\n"
  "   -sizeBusy=F   extra-small dot radius fraction for the very busiest areas\n"
  "                 (default 0.4).  The dots shrink from sizeLo toward this only\n"
  "                 at the high end of busyness, so fine detail stays sharp.\n"
  "                 Set equal to sizeLo to turn the extra shrink off.\n"
  "   -sizeHi=F     largest dot radius fraction (default 2.3); by default used\n"
  "                 for the flattest areas.  Raise sizeLo toward ~1.45 if you want\n"
  "                 the busy areas to stay overlapped (no white).\n"
  "   -invertSize   flip it: make busy areas big and flat areas small (default is\n"
  "                 busier -> smaller dot).\n"
  "   -jitter=F     random horizontal nudge per dot, as a fraction of a column\n"
  "                 (default 0.3).  Together with row stagger this breaks up the\n"
  "                 obvious vertical columns.  0 turns it off.\n"
  "   -alpha=N      skip pixels whose alpha is below N (0-255, default 128).\n"
  "   -name=string  short label for the track (default derived from outDir).\n"
  "   -db=database  instead of a synthetic assembly, emit a track hub for this\n"
  "                 existing assembly (e.g. hg38).  Requires -pos.\n"
  "   -pos=chr:s-e  genomic window the mosaic is spread across, when -db is set\n"
  "                 (e.g. chr19:11089417-11133830).  The image fills the window.\n"
  );
}

static struct optionSpec options[] = {
   {"maxDim", OPTION_INT},
   {"dotSize", OPTION_INT},
   {"sizeVar", OPTION_INT},
   {"sizeLo", OPTION_FLOAT},
   {"sizeBusy", OPTION_FLOAT},
   {"sizeHi", OPTION_FLOAT},
   {"invertSize", OPTION_BOOLEAN},
   {"jitter", OPTION_FLOAT},
   {"alpha", OPTION_INT},
   {"name", OPTION_STRING},
   {"db", OPTION_STRING},
   {"pos", OPTION_STRING},
   {NULL, 0},
};

/* horizontal sub-column resolution: each dot column spans this many bp, so a
 * dot can be placed at a fraction of a column to break up the grid. */
#define SUB 64

/* command line options, with defaults */
int clMaxDim = 64;
int clDotSize = 8;
int clSizeVar = 1;
double clSizeLo = 0.65;
double clSizeBusy = 0.4;
double clSizeHi = 2.3;
boolean clInvertSize = FALSE;
double clJitter = 0.3;
int clAlpha = 128;
char *clName = NULL;
char *clDb = NULL;
char *clPos = NULL;

static double hashJitter(int x, int y)
/* Deterministic pseudo-random value in [-1,1) from the pixel coordinates, so
 * the dot scatter is stable across runs without needing a RNG. */
{
unsigned h = ((unsigned)x * 73856093u) ^ ((unsigned)y * 19349663u);
h = h * 2654435761u + 0x9e3779b9u;
return ((double)((h >> 8) & 0xffff) / 32768.0) - 1.0;
}

static unsigned char *readPngRgba(char *fileName, int *retWidth, int *retHeight)
/* Read a PNG into a freshly allocated row-major RGBA buffer (4 bytes/pixel). */
{
png_image image;
zeroBytes(&image, sizeof(image));
image.version = PNG_IMAGE_VERSION;

if (!png_image_begin_read_from_file(&image, fileName))
    errAbort("Can't read PNG %s: %s", fileName, image.message);
image.format = PNG_FORMAT_RGBA;

unsigned char *buffer = needLargeMem(PNG_IMAGE_SIZE(image));
if (!png_image_finish_read(&image, NULL, buffer, 0, NULL))
    {
    png_image_free(&image);
    errAbort("Error decoding PNG %s: %s", fileName, image.message);
    }
*retWidth = image.width;
*retHeight = image.height;
return buffer;
}

static unsigned char *downscale(unsigned char *src, int sw, int sh, int maxDim,
                                int *retW, int *retH)
/* Box-average src down so its longest side is at most maxDim.  Averaging is
 * alpha-weighted so transparent pixels don't darken their neighbors.  Returns a
 * new RGBA buffer (caller frees); the source dimensions are returned when no
 * scaling is needed. */
{
int big = (sw > sh) ? sw : sh;
if (maxDim <= 0 || big <= maxDim)
    {
    *retW = sw;
    *retH = sh;
    return cloneMem(src, (size_t)sw * sh * 4);
    }
double scale = (double)maxDim / big;
int dw = round(sw * scale);
int dh = round(sh * scale);
if (dw < 1) dw = 1;
if (dh < 1) dh = 1;

unsigned char *dst = needLargeMem((size_t)dw * dh * 4);
int dx, dy;
for (dy = 0; dy < dh; ++dy)
    {
    int sy0 = (int)((double)dy * sh / dh);
    int sy1 = (int)((double)(dy + 1) * sh / dh);
    if (sy1 <= sy0) sy1 = sy0 + 1;
    for (dx = 0; dx < dw; ++dx)
        {
        int sx0 = (int)((double)dx * sw / dw);
        int sx1 = (int)((double)(dx + 1) * sw / dw);
        if (sx1 <= sx0) sx1 = sx0 + 1;
        double sumR = 0, sumG = 0, sumB = 0, sumA = 0;
        int n = 0;
        int x, y;
        for (y = sy0; y < sy1; ++y)
            for (x = sx0; x < sx1; ++x)
                {
                unsigned char *p = src + ((size_t)y * sw + x) * 4;
                double a = p[3];
                sumR += p[0] * a;
                sumG += p[1] * a;
                sumB += p[2] * a;
                sumA += a;
                ++n;
                }
        unsigned char *o = dst + ((size_t)dy * dw + dx) * 4;
        if (sumA > 0)
            {
            o[0] = (unsigned char)(sumR / sumA + 0.5);
            o[1] = (unsigned char)(sumG / sumA + 0.5);
            o[2] = (unsigned char)(sumB / sumA + 0.5);
            o[3] = (unsigned char)(sumA / n + 0.5);
            }
        else
            o[0] = o[1] = o[2] = o[3] = 0;
        }
    }
*retW = dw;
*retH = dh;
return dst;
}

static double *luminanceMap(unsigned char *img, int w, int h)
/* Return a w*h array of luminance values (0-255), -1 for transparent pixels. */
{
double *lum = needLargeMem((size_t)w * h * sizeof(double));
int i;
for (i = 0; i < w * h; ++i)
    {
    unsigned char *p = img + (size_t)i * 4;
    if (p[3] < clAlpha)
        lum[i] = -1;
    else
        lum[i] = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
    }
return lum;
}

static double localVariation(double *lum, int w, int h, int x, int y, int radius)
/* Standard deviation of luminance over the window around (x,y), considering
 * only opaque pixels.  Returns 0 when there isn't enough to compare. */
{
double sum = 0, sumSq = 0;
int n = 0;
int dx, dy;
for (dy = -radius; dy <= radius; ++dy)
    for (dx = -radius; dx <= radius; ++dx)
        {
        int nx = x + dx, ny = y + dy;
        if (nx < 0 || nx >= w || ny < 0 || ny >= h)
            continue;
        double v = lum[(size_t)ny * w + nx];
        if (v < 0)
            continue;
        sum += v;
        sumSq += v * v;
        ++n;
        }
if (n < 2)
    return 0;
double mean = sum / n;
double var = sumSq / n - mean * mean;
if (var < 0)
    var = 0;
return sqrt(var);
}

void pngToLolly(char *inPng, char *outDir)
/* Read the PNG, downscale, and write the hub files into outDir. */
{
int sw, sh;
unsigned char *raw = readPngRgba(inPng, &sw, &sh);
verbose(1, "read %s: %dx%d\n", inPng, sw, sh);

int w = 0, h = 0;
unsigned char *img = downscale(raw, sw, sh, clMaxDim, &w, &h);
freez(&raw);
verbose(1, "rendering as %dx%d dots\n", w, h);

double *lum = (clSizeVar > 0) ? luminanceMap(img, w, h) : NULL;

/* Geometry.  Adjacent dot columns are dotSize pixels apart, and rows are
 * dotSize pixels apart vertically.  Each column spans SUB bp so a dot can be
 * placed at a fraction of a column; the genome is therefore w*SUB bp wide and
 * 1 bp == dotSize/SUB pixels.  See lollyTrack.c for how the value maps onto
 * usableHeight = trackHeight - LOLLY_DIAMETER - fontHeight. */
double tileRadius = clDotSize / 2.0;             /* radius that exactly tiles */
/* The largest dot fraction sets the pixel radius used for the top/bottom
 * margin and the maxHeightPixels we emit. */
double maxFrac = (clSizeHi > 1.0) ? clSizeHi : 1.0;
int maxRadius = (int)ceil(tileRadius * (lum != NULL ? maxFrac : 1.0));
int lollyDiameter = 2 * (maxRadius + 2);         /* matches LOLLY_DIAMETER */
int fontHeight = 9;                              /* tl.fontHeight + 1, default font */
int trackHeight = (h - 1) * clDotSize + lollyDiameter + fontHeight;
if (trackHeight < lollyDiameter + fontHeight + clDotSize)
    trackHeight = lollyDiameter + fontHeight + clDotSize;
int pix = w * clDotSize;

/* Where the dots live.  By default a synthetic single-chromosome assembly with
 * each column spanning SUB bp.  With -db/-pos the dots instead spread across a
 * real genomic window on an existing assembly, so the image fills that window. */
char *chromName = CHROM_NAME;
long winStart = 0;
int chromSize = w * SUB;                          /* synthetic chrom length */
double colSpacing = SUB;                          /* bp between dot columns */
if (clDb != NULL)
    {
    if (clPos == NULL)
        errAbort("-db requires -pos (e.g. -pos=chr19:11089417-11133830)");
    /* parse chr:start-end, tolerating commas in the numbers */
    char *spec = cloneString(clPos);
    stripChar(spec, ',');
    char *colon = strrchr(spec, ':');
    char *dash = colon ? strchr(colon, '-') : NULL;
    if (colon == NULL || dash == NULL)
        errAbort("can't parse -pos=%s (expected chr:start-end)", clPos);
    *colon = *dash = '\0';
    long e = atol(dash + 1);
    winStart = atol(colon + 1) - 1;               /* 1-based display -> 0-based */
    if (winStart < 0)
        winStart = 0;
    if (e <= winStart)
        errAbort("-pos end must be greater than start: %s", clPos);
    chromName = cloneString(spec);
    colSpacing = (double)(e - winStart) / w;
    chromSize = e;                                /* only used for clamping */
    }

makeDirsOnPath(outDir);
char path[PATH_LEN];

/* The BED: bed9 + a lolly-size field.  Color comes from itemRgb (field 9);
 * the score field carries the row value that becomes the vertical position. */
safef(path, sizeof(path), "%s/img.bed", outDir);
FILE *bed = mustOpen(path, "w");
int x, y;
long dots = 0;
int minSize = BIGNUM, maxSize = 0;
for (y = 0; y < h; ++y)
    {
    for (x = 0; x < w; ++x)
        {
        unsigned char *p = img + ((size_t)y * w + x) * 4;
        if (p[3] < clAlpha)
            continue;
        int val = (h - 1) - y;                   /* top row -> highest value */

        /* lolly radius in pixels, from local variation.  By default busier
         * areas (more variation) get smaller dots, flat areas get bigger ones;
         * -invertSize flips that. */
        double radius = tileRadius;
        double busy = 0.5;
        if (lum != NULL)
            {
            busy = localVariation(lum, w, h, x, y, clSizeVar) / 80.0;
            if (busy > 1.0) busy = 1.0;
            busy = pow(busy, 0.6);               /* push more areas toward small */
            if (clInvertSize) busy = 1.0 - busy;
            double frac = clSizeHi + (clSizeLo - clSizeHi) * busy;
            /* The very busiest areas shrink further, from sizeLo toward sizeBusy.
             * The cubic concentrates this at the high end so ordinarily-busy
             * areas keep their sizeLo dots. */
            if (clSizeBusy < clSizeLo)
                frac -= (clSizeLo - clSizeBusy) * pow(busy, 3.0);
            radius = tileRadius * frac;
            }
        /* The browser renders the lolly radius as sizeField * trackHeight / 100
         * (see lollyTrack.c), so convert our desired pixel radius into that
         * percent-of-height unit.  trackHeight is large, so this quantizes to a
         * few integer levels - that is an inherent limit of the percent scheme. */
        int sizeVal = round(radius * 100.0 / trackHeight);
        if (sizeVal < 1)
            sizeVal = 1;
        if (sizeVal < minSize) minSize = sizeVal;
        if (sizeVal > maxSize) maxSize = sizeVal;

        /* Horizontal placement, in fractions of a column.  Stagger alternate
         * rows by half a column and add a size-weighted random nudge so the
         * dots don't line up into obvious vertical stripes. */
        double offset = (y & 1) ? 0.5 : 0.0;
        offset += clJitter * (1.0 + busy) * hashJitter(x, y);
        double centerBp = winStart + (x + 0.5 + offset) * colSpacing;
        long start = (long)floor(centerBp);      /* lolly center uses start+0.5 */
        if (start < winStart)
            start = winStart;
        if (start > chromSize - 1)
            start = chromSize - 1;

        fprintf(bed, "%s\t%ld\t%ld\tp%d_%d\t%d\t.\t%ld\t%ld\t%d,%d,%d\t%d\n",
            chromName, start, start + 1, x, y, val, start, start + 1,
            p[0], p[1], p[2], sizeVal);
        ++dots;
        }
    }
carefulClose(&bed);
verbose(1, "wrote %ld dots; %d size level(s), rendered radius %g-%g px\n",
    dots, maxSize - minSize + 1, minSize * trackHeight / 100.0,
    maxSize * trackHeight / 100.0);

/* autoSql for the bed9+1 */
safef(path, sizeof(path), "%s/lolly.as", outDir);
FILE *as = mustOpen(path, "w");
fprintf(as,
    "table lolly\n"
    "\"stemless lolly mosaic\"\n"
    "    (\n"
    "    string chrom;      \"Reference sequence chromosome\"\n"
    "    uint   chromStart; \"Start position in chromosome\"\n"
    "    uint   chromEnd;   \"End position in chromosome\"\n"
    "    string name;       \"Pixel id\"\n"
    "    uint   score;      \"Row value (vertical position)\"\n"
    "    char[1] strand;    \"Unused\"\n"
    "    uint   thickStart; \"Unused\"\n"
    "    uint   thickEnd;   \"Unused\"\n"
    "    uint   itemRgb;    \"Pixel color\"\n"
    "    uint   lollySize;  \"Dot size (percent of track height *100)\"\n"
    "    )\n");
carefulClose(&as);

/* chrom.sizes and a blank synthetic genome.  For a track hub on an existing
 * assembly we skip these; makeHub.sh fetches the real chrom.sizes instead. */
if (clDb == NULL)
    {
    safef(path, sizeof(path), "%s/chrom.sizes", outDir);
    FILE *cs = mustOpen(path, "w");
    fprintf(cs, "%s\t%d\n", CHROM_NAME, chromSize);
    carefulClose(&cs);

    safef(path, sizeof(path), "%s/img.fa", outDir);
    FILE *fa = mustOpen(path, "w");
    fprintf(fa, ">%s\n", CHROM_NAME);
    int col;
    for (col = 0; col < chromSize; ++col)
        {
        fputc('N', fa);
        if ((col % 60) == 59)
            fputc('\n', fa);
        }
    if ((chromSize % 60) != 0)
        fputc('\n', fa);
    carefulClose(&fa);
    }

char *name = clName;
if (name == NULL)
    {
    char dir[PATH_LEN], base[FILENAME_LEN], ext[FILEEXT_LEN];
    splitPath(outDir, dir, base, ext);
    name = cloneString(base[0] ? base : "lollyImg");
    }

/* trackDb.txt.  Squish visibility gives clean filled dots (no black outline).
 * Squish halves the configured height, so maxHeightPixels is set to 2*height.
 * lollyMaxSize is the biggest rendered radius (maxSize * trackHeight / 100), so
 * the top/bottom margin (LOLLY_DIAMETER) leaves room for the largest dot. */
int lollyMaxSizePx = (int)ceil(maxSize * trackHeight / 100.0);
safef(path, sizeof(path), "%s/trackDb.txt", outDir);
FILE *tdb = mustOpen(path, "w");
fprintf(tdb,
    "track %s\n"
    "type bigLolly\n"
    "shortLabel %s\n"
    "longLabel %s lolly mosaic\n"
    "bigDataUrl img.bb\n"
    "visibility squish\n"
    "lollyNoStems on\n"
    "lollySizeField lollySize\n"
    "lollyMaxSize %d\n"
    "yAxisNumLabels off\n"
    "autoScale off\n"
    "viewLimits 0:%d\n"
    "maxHeightPixels %d:%d:1\n"
    "itemRgb on\n",
    GENOME_DB, name, name, lollyMaxSizePx, h - 1, 2 * trackHeight, 2 * trackHeight);
carefulClose(&tdb);

/* hub.txt and genomes.txt for a single-genome assembly hub */
safef(path, sizeof(path), "%s/hub.txt", outDir);
FILE *hub = mustOpen(path, "w");
fprintf(hub,
    "hub %sLolly\n"
    "shortLabel %s lolly\n"
    "longLabel %s rendered as a stemless lolly mosaic\n"
    "genomesFile genomes.txt\n"
    "email %s\n",
    name, name, name, "genome-www@soe.ucsc.edu");
carefulClose(&hub);

safef(path, sizeof(path), "%s/genomes.txt", outDir);
FILE *gen = mustOpen(path, "w");
if (clDb != NULL)
    fprintf(gen,
        "genome %s\n"
        "trackDb trackDb.txt\n",
        clDb);
else
    fprintf(gen,
        "genome %s\n"
        "trackDb trackDb.txt\n"
        "twoBitPath img.2bit\n"
        "organism Lolly art\n"
        "scientificName %s\n"
        "description %s lolly mosaic\n"
        "defaultPos %s:1-%d\n",
        GENOME_DB, name, name, CHROM_NAME, chromSize);
carefulClose(&gen);

/* makeHub.sh - turns the text/data files into the 2bit and bigBed */
safef(path, sizeof(path), "%s/makeHub.sh", outDir);
FILE *shf = mustOpen(path, "w");
if (clDb != NULL)
    fprintf(shf,
        "#!/bin/sh\n"
        "# Build the bigLolly for this track hub.  Run from this directory.\n"
        "set -e\n"
        "fetchChromSizes %s > chrom.sizes\n"
        "bedToBigBed -sort -as=lolly.as -type=bed9+1 -tab img.bed chrom.sizes img.bb\n"
        "echo 'done - move this directory under ~/public_html and load hub.txt'\n",
        clDb);
else
    fprintf(shf,
        "#!/bin/sh\n"
        "# Build the 2bit and bigLolly for this hub.  Run from this directory.\n"
        "set -e\n"
        "faToTwoBit img.fa img.2bit\n"
        "bedToBigBed -sort -as=lolly.as -type=bed9+1 -tab img.bed chrom.sizes img.bb\n"
        "echo 'done - move this directory under ~/public_html and load hub.txt'\n");
carefulClose(&shf);
chmod(path, 0755);

verbose(1, "wrote hub to %s\n", outDir);
printf("\n");
printf("Next steps:\n");
printf("  cd %s && ./makeHub.sh\n", outDir);
printf("  # then move %s under ~/public_html and load:\n", outDir);
if (clDb != NULL)
    printf("  https://hgwdev-braney.gi.ucsc.edu/cgi-bin/hgTracks?"
           "db=%s&hubUrl=<your-public-url>/%s/hub.txt"
           "&position=%s:%ld-%d&pix=%d&%s=squish\n",
           clDb, name, chromName, winStart + 1, chromSize, pix, GENOME_DB);
else
    printf("  https://hgwdev-braney.gi.ucsc.edu/cgi-bin/hgTracks?"
           "hubUrl=<your-public-url>/%s/hub.txt"
           "&genome=%s&position=%s:1-%d&pix=%d&%s=squish\n",
           name, GENOME_DB, CHROM_NAME, chromSize, pix, GENOME_DB);

freez(&img);
freez(&lum);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clMaxDim = optionInt("maxDim", clMaxDim);
clDotSize = optionInt("dotSize", clDotSize);
clSizeVar = optionInt("sizeVar", clSizeVar);
clSizeLo = optionFloat("sizeLo", clSizeLo);
clSizeBusy = optionFloat("sizeBusy", clSizeBusy);
clSizeHi = optionFloat("sizeHi", clSizeHi);
clInvertSize = optionExists("invertSize");
clJitter = optionFloat("jitter", clJitter);
clAlpha = optionInt("alpha", clAlpha);
clName = optionVal("name", clName);
clDb = optionVal("db", clDb);
clPos = optionVal("pos", clPos);
if (clDotSize < 2)
    errAbort("-dotSize must be at least 2");
pngToLolly(argv[1], argv[2]);
return 0;
}
