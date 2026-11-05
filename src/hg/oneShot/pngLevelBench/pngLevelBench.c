/* pngLevelBench - encode a png at every zlib level and row filter, and report
 * bytes and time.
 *
 * Refs #38109.  The compression level decision needs two numbers per candidate
 * setting: the bytes the track image grows by, and the encode time it saves.
 * This measures both on real track images.
 *
 * The encode here is the same one lib/pngwrite.c does: RGBA, 8 bits, no
 * interlace, and by default the row filter pinned to UP (refs #38107).  At
 * level 6, which is what libpng's default resolves to, the byte count must
 * equal the size of the input file; -check reports that comparison, and it is
 * what makes this tool's numbers stand for what hgTracks really does.
 *
 * The row filter can be swept as well as the level, with -filters.  A filter is
 * a lossless per-row transform, so the decoded image is the same whichever one
 * is used, and the two settings interact: the filter that compresses best is
 * not necessarily the same one at every level.
 *
 * Nothing is written to disk.  The encoder's output goes to a counting
 * function, so the time is the encode alone with no write() in it.
 */

#include "png.h"   // MUST come before common.h, due to setjmp checking in pngconf.h
#include "common.h"
#include "options.h"
#include "portable.h"
#include <time.h>

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pngLevelBench - encode a png at every zlib level and report bytes and time\n"
  "usage:\n"
  "   pngLevelBench in.png [in2.png ...]\n"
  "options:\n"
  "   -reps=N       encode each setting N times and keep the fastest (default 3)\n"
  "   -minLevel=N   first level to try (default 0)\n"
  "   -maxLevel=N   last level to try (default 9)\n"
  "   -base=N       the level the summary compares against (default 6, which is\n"
  "                 what libpng's default resolves to)\n"
  "   -filters=LIST comma separated row filters to try, from none, sub, up, avg,\n"
  "                 paeth and all (default up, which is what the browser uses;\n"
  "                 all is libpng's own per row choice, which was the browser's\n"
  "                 behaviour before #38107)\n"
  "   -baseFilter=F the filter the summary compares against (default up)\n"
  "   -tab          one tab separated row per file per setting, no summary\n"
  "   -check        also print whether the base setting matches the file size\n"
  );
}

static struct optionSpec options[] = {
   {"reps", OPTION_INT},
   {"minLevel", OPTION_INT},
   {"maxLevel", OPTION_INT},
   {"base", OPTION_INT},
   {"filters", OPTION_STRING},
   {"baseFilter", OPTION_STRING},
   {"tab", OPTION_BOOLEAN},
   {"check", OPTION_BOOLEAN},
   {NULL, 0},
};

int reps = 3;
int minLevel = 0;
int maxLevel = 9;
int baseLevel = 6;
boolean tabOut = FALSE;
boolean checkOut = FALSE;

struct filterSpec
/* one libpng row filter, by the name this program takes for it */
    {
    char *name;
    int mask;
    };

static struct filterSpec allFilters[] = {
    {"none", PNG_FILTER_NONE},
    {"sub", PNG_FILTER_SUB},
    {"up", PNG_FILTER_UP},
    {"avg", PNG_FILTER_AVG},
    {"paeth", PNG_FILTER_PAETH},
    {"all", PNG_ALL_FILTERS},
};

#define maxFilters ArraySize(allFilters)

struct filterSpec *filters[maxFilters];	/* the ones asked for, in order */
int filterCount = 0;
int baseFilterIx = 0;

struct image
/* one decoded png, as the RGBA rows libpng will be handed back */
    {
    int width, height;
    png_byte *pixels;		/* width * height * 4 bytes */
    png_byte **rowPointers;
    long fileSize;		/* the input file, for the base setting check */
    };

struct cell
/* what one filter and level cost, summed over the images */
    {
    long bytes;
    double ms;
    };

static struct cell sum[maxFilters][10];

static size_t byteCount;	/* where the counting write function adds up */

static void pngAbort(png_structp png, png_const_charp errorMessage)
/* type png_error wrapper around errAbort */
{
errAbort("%s", (char *)errorMessage);
}

static void pngWarn(png_structp png, png_const_charp warningMessage)
/* type png_error wrapper around warn */
{
warn("%s", (char *)warningMessage);
}

static void countWrite(png_structp png, png_bytep data, png_size_t length)
/* stand in for fwrite: keep the size, throw the bytes away */
{
byteCount += length;
}

static void countFlush(png_structp png)
/* nothing to flush when nothing is written */
{
}

static double msNow()
/* a monotonic clock in ms, with the fraction kept.  clock1000() rounds to the
 * millisecond, which is too coarse for a small image at a low level. */
{
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);
return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

void parseFilters(char *list)
/* work out which filters to try, keeping the order they are named in */
{
struct slName *names = slNameListFromComma(list), *name;
for (name = names;  name != NULL;  name = name->next)
    {
    int i;
    boolean found = FALSE;
    for (i = 0;  i < maxFilters;  i++)
	{
	if (sameWord(name->name, allFilters[i].name))
	    {
	    if (filterCount >= maxFilters)
		errAbort("too many filters named");
	    filters[filterCount++] = &allFilters[i];
	    found = TRUE;
	    break;
	    }
	}
    if (!found)
	errAbort("no such row filter '%s'; use none, sub, up, avg, paeth or all",
		 name->name);
    }
if (filterCount == 0)
    errAbort("-filters needs at least one filter");
slNameFreeList(&names);
}

struct image *imageLoad(char *fileName)
/* read a png into RGBA rows, the same shape memGfx holds them in */
{
png_image image;
zeroBytes(&image, sizeof image);
image.version = PNG_IMAGE_VERSION;
if (!png_image_begin_read_from_file(&image, fileName))
    errAbort("%s: %s", fileName, image.message);
image.format = PNG_FORMAT_RGBA;

struct image *img;
AllocVar(img);
img->width = image.width;
img->height = image.height;
img->pixels = needLargeMem(PNG_IMAGE_SIZE(image));
if (!png_image_finish_read(&image, NULL, img->pixels, 0, NULL))
    {
    png_image_free(&image);
    errAbort("%s: %s", fileName, image.message);
    }
img->rowPointers = needMem(img->height * sizeof(png_byte *));
int i;
for (i = 0;  i < img->height;  i++)
    img->rowPointers[i] = img->pixels + (long)i * img->width * 4;
img->fileSize = fileSize(fileName);
return img;
}

long encode(struct image *img, int filterMask, int level)
/* encode with one filter at one level and return the bytes it came to.
 * Everything but the filter and the level matches mgSaveToPng in
 * lib/pngwrite.c. */
{
png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
					  pngAbort, pngWarn);
if (!png)
    errAbort("png_write_struct failed");
png_infop info = png_create_info_struct(png);
if (!info)
    {
    png_destroy_write_struct(&png, NULL);
    errAbort("png create_info_struct failed");
    }
if (setjmp(png_jmpbuf(png)))
    {
    png_destroy_write_struct(&png, &info);
    errAbort("pngLevelBench: setjmp nonzero");
    }

byteCount = 0;
png_set_write_fn(png, NULL, countWrite, countFlush);
png_set_IHDR(png, info, img->width, img->height, 8,
             PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
             PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
png_set_compression_level(png, level);
png_set_filter(png, PNG_FILTER_TYPE_BASE, filterMask);
png_set_rows(png, info, img->rowPointers);
png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);
png_destroy_write_struct(&png, &info);
return byteCount;
}

void benchOne(char *fileName)
/* time every filter and level on one image, print the rows, add to the totals */
{
struct image *img = imageLoad(fileName);
int fi, level;
for (fi = 0;  fi < filterCount;  fi++)
    {
    for (level = minLevel;  level <= maxLevel;  level++)
	{
	long bytes = 0;
	double best = 0;
	int rep;
	for (rep = 0;  rep < reps;  rep++)
	    {
	    double start = msNow();
	    bytes = encode(img, filters[fi]->mask, level);
	    double ms = msNow() - start;
	    if (rep == 0 || ms < best)
		best = ms;
	    }
	if (tabOut)
	    printf("%s\t%d\t%d\t%ld\t%s\t%d\t%ld\t%.3f\n", fileName, img->width,
		   img->height, img->fileSize, filters[fi]->name, level, bytes,
		   best);
	else
	    printf("  %-5s level %d  %8ld bytes  %8.2f ms\n", filters[fi]->name,
		   level, bytes, best);
	sum[fi][level].bytes += bytes;
	sum[fi][level].ms += best;
	}
    }
freeMem(img->rowPointers);
freeMem(img->pixels);
freeMem(img);
}

void breakEvenString(long dBytes, double dMs, char *out, int outSize)
/* the reader throughput in Mbit/s at which dBytes more costs exactly dMs less.
 * A setting is a trade only when it moves both ways at once; anything else is
 * a straight win or a straight loss and needs no speed. */
{
if (dBytes > 0 && dMs > 0)
    // bigger and faster: the reader gains above this speed
    safef(out, outSize, "%10.1f", dBytes * 8.0 / dMs / 1000.0);
else if (dBytes < 0 && dMs < 0)
    // smaller and slower: the reader gains only below this speed
    safef(out, outSize, "%9s%.1f", "<", dBytes * 8.0 / dMs / 1000.0);
else if (dBytes == 0 && dMs == 0)
    safef(out, outSize, "%10s", "base");
else
    safef(out, outSize, "%10s", dBytes <= 0 ? "always" : "never");
}

void pngLevelBench(int fileCount, char *fileNames[])
/* pngLevelBench - encode a png at every zlib level and report bytes and time */
{
long inputTotal = 0;
int i;

if (tabOut)
    printf("#file\twidth\theight\tfileBytes\tfilter\tlevel\tencodedBytes\tms\n");
for (i = 0;  i < fileCount;  i++)
    {
    if (!tabOut)
	printf("%s\n", fileNames[i]);
    inputTotal += fileSize(fileNames[i]);
    benchOne(fileNames[i]);
    }
if (tabOut)
    return;

if (baseLevel < minLevel || baseLevel > maxLevel)
    errAbort("-base=%d is outside the levels measured", baseLevel);
struct cell *base = &sum[baseFilterIx][baseLevel];
printf("\n%d image%s, %ld bytes on disk\n", fileCount,
       fileCount == 1 ? "" : "s", inputTotal);
if (checkOut)
    printf("%s at level %d encodes to %ld bytes against %ld on disk, "
	   "a difference of %ld\n", filters[baseFilterIx]->name, baseLevel,
	   base->bytes, inputTotal, base->bytes - inputTotal);
printf("\n%-7s %-7s %12s %10s %12s %10s %14s\n", "filter", "level", "bytes",
       "ms", "bytes vs b", "ms vs b", "break-even");
int fi, level;
for (fi = 0;  fi < filterCount;  fi++)
    {
    for (level = minLevel;  level <= maxLevel;  level++)
	{
	long dBytes = sum[fi][level].bytes - base->bytes;
	double dMs = base->ms - sum[fi][level].ms;
	char breakEven[32];
	breakEvenString(dBytes, dMs, breakEven, sizeof breakEven);
	printf("%-7s %-7d %12ld %10.2f %12ld %10.2f %14s\n", filters[fi]->name,
	       level, sum[fi][level].bytes, sum[fi][level].ms, dBytes, dMs,
	       breakEven);
	}
    }
printf("\nbytes vs b and ms vs b are against %s at level %d.  A positive ms vs b\n"
       "is time saved.  break-even is the reader throughput in Mbit/s at which\n"
       "the extra bytes cost exactly the saved time.  A plain number means the\n"
       "setting is bigger and faster, so the reader gains above that speed; a\n"
       "number with a < means it is smaller and slower, so the reader gains only\n"
       "below it.  always means smaller and faster, never means bigger and\n"
       "slower.\n", filters[baseFilterIx]->name, baseLevel);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
reps = optionInt("reps", reps);
minLevel = optionInt("minLevel", minLevel);
maxLevel = optionInt("maxLevel", maxLevel);
baseLevel = optionInt("base", baseLevel);
tabOut = optionExists("tab");
checkOut = optionExists("check");
parseFilters(optionVal("filters", "up"));
char *baseFilter = optionVal("baseFilter", "up");
int fi;
baseFilterIx = -1;
for (fi = 0;  fi < filterCount;  fi++)
    if (sameWord(baseFilter, filters[fi]->name))
	baseFilterIx = fi;
if (baseFilterIx < 0)
    errAbort("-baseFilter=%s is not one of the filters being measured",
	     baseFilter);
if (minLevel < 0 || maxLevel > 9 || minLevel > maxLevel)
    errAbort("levels run from 0 to 9 and -minLevel cannot exceed -maxLevel");
if (reps < 1)
    errAbort("-reps must be at least 1");
pngLevelBench(argc - 1, argv + 1);
return 0;
}
