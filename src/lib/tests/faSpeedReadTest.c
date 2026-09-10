/* faSpeedReadTest - exercise the buffer growth in faMixedSpeedReadNext.
 *
 * The reader keeps one buffer and grows it as it walks the lines of a record.
 * A record whose lines vary a lot in length used to make it write past the end
 * of that buffer, because the test that decided to grow and the request that
 * did the growing did not ask for the same size.  refs #38320
 *
 * Each case writes a FASTA file with a given set of line lengths, reads it
 * back, and checks the name, the size and every base against what went in.
 * The buffer is freed before each case so the growth starts from the same
 * place every time.  That is what makes a case reproduce a given sequence of
 * buffer sizes, and so what makes the shapes below mean anything.
 */
#include "common.h"
#include "linefile.h"
#include "fa.h"

static int errCount = 0;

static char baseAt(int i)
/* Deterministic base for position i in a record, so a long sequence can be
 * written and then checked without keeping a copy of it. */
{
static char bases[] = "acgtACGTn";
unsigned x = (unsigned)i * 1103515245 + 12345;
return bases[(x >> 16) % (sizeof(bases)-1)];
}

/* Line lengths of each record, 0 terminated.  A case is a NULL terminated
 * list of these. */

static int ticketShape[] = {20001, 6238, 112347, 0};
/* The shape from #38320.  Line 1 takes the buffer to 65536, line 2 leaves
 * bufIx at 26239, and line 3 needs 138587 bytes where doubling gives 131072. */

static int quietShape[] = {20001, 6238, 105000, 0};
/* The same shape with a shorter third line.  This one ran past the end by 167
 * bytes and still exited 0, so it is the case a crash test would miss. */

static int atInitialSize[] = {65535, 1, 0};
static int fillsInitialSize[] = {65536, 0};
static int justOverInitialSize[] = {65537, 0};
/* The buffer starts at 65536, so these sit on either side of that edge. */

static int atSecondSize[] = {131071, 2, 0};
/* One byte short of the first doubling, then a line that crosses it. */

static int smallThenLarge[] = {1, 65535, 0};
static int manyDoublings[] = {70000, 70000, 200000, 0};
static int evenWrap[] = {50, 50, 50, 50, 0};
static int oneShortLine[] = {12, 0};

static int firstOfTwo[] = {20001, 6238, 112347, 0};
static int secondOfTwo[] = {300, 90000, 0};
/* Two records in one file.  The second must start from bufIx 0 and must not
 * pick up anything the first one left behind. */

struct faCase
/* One test file: what it is for, and the line lengths of each of its records. */
    {
    char *why;                  /* What this shape tests. */
    int *recLens[3];            /* Line lengths per record, NULL terminated. */
    };

static struct faCase cases[] = {
    {"ticket shape, three uneven lines",         {ticketShape, NULL}},
    {"same shape, overrun too small to abort",   {quietShape, NULL}},
    {"one byte short of the initial size",       {atInitialSize, NULL}},
    {"exactly the initial size",                 {fillsInitialSize, NULL}},
    {"one byte over the initial size",           {justOverInitialSize, NULL}},
    {"crossing the first doubling",              {atSecondSize, NULL}},
    {"short line then a long one",               {smallThenLarge, NULL}},
    {"several doublings in one record",          {manyDoublings, NULL}},
    {"evenly wrapped, the ordinary case",        {evenWrap, NULL}},
    {"a single short line",                      {oneShortLine, NULL}},
    {"two records in one file",                  {firstOfTwo, secondOfTwo, NULL}},
    };

static void writeCase(struct faCase *fc, char *fileName)
/* Write the FASTA file this case describes. */
{
FILE *f = mustOpen(fileName, "w");
int recIx;
for (recIx = 0; fc->recLens[recIx] != NULL; ++recIx)
    {
    int *lens = fc->recLens[recIx];
    int base = 0, lineIx;
    fprintf(f, ">rec%d\n", recIx);
    for (lineIx = 0; lens[lineIx] != 0; ++lineIx)
        {
        int i;
        for (i = 0; i < lens[lineIx]; ++i)
            fputc(baseAt(base++), f);
        fputc('\n', f);
        }
    }
carefulClose(&f);
}

static void readCase(struct faCase *fc, char *fileName)
/* Read the file back and check each record against what was written. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int recIx;
for (recIx = 0; fc->recLens[recIx] != NULL; ++recIx)
    {
    int *lens = fc->recLens[recIx];
    int wrote = 0, lineIx;
    for (lineIx = 0; lens[lineIx] != 0; ++lineIx)
        wrote += lens[lineIx];

    DNA *dna = NULL;
    int size = 0, i;
    char *name = NULL, wantName[64];
    safef(wantName, sizeof(wantName), "rec%d", recIx);

    if (!faMixedSpeedReadNext(lf, &dna, &size, &name))
        {
        printf("  rec %d  MISSING\n", recIx);
        ++errCount;
        continue;
        }

    /* Count the bases that differ rather than stopping at the first one, so
     * the output says how far the damage reaches. */
    int badBases = 0;
    for (i = 0; i < size && i < wrote; ++i)
        {
        if (dna[i] != baseAt(i))
            ++badBases;
        }

    boolean ok = (size == wrote && badBases == 0 && sameString(name, wantName)
                  && dna[size] == 0);
    if (!ok)
        ++errCount;
    printf("  rec %d  lines %d  wrote %d  read %d  name %s  %s\n",
        recIx, lineIx, wrote, size, name,
        ok ? "ok" : "BAD");
    if (badBases != 0)
        printf("  rec %d  %d bases differ\n", recIx, badBases);
    }

DNA *dna = NULL;
int size = 0;
char *name = NULL;
if (faMixedSpeedReadNext(lf, &dna, &size, &name))
    {
    printf("  extra record %s after the end\n", name);
    ++errCount;
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
{
char *fileName = "output/faSpeedReadTest.fa";
int i;

for (i = 0; i < ArraySize(cases); ++i)
    {
    struct faCase *fc = &cases[i];
    /* Start every case from an unallocated buffer, so the sizes it grows
     * through are the ones the case was built for.  Freeing here also means a
     * write past the end of the buffer has to survive a free before the next
     * case can print anything. */
    faFreeFastBuf();
    printf("%s\n", fc->why);
    writeCase(fc, fileName);
    readCase(fc, fileName);
    }
faFreeFastBuf();
remove(fileName);

printf("%d cases, %d problems\n", (int)ArraySize(cases), errCount);
return (errCount == 0 ? 0 : 1);
}
