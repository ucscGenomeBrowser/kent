/* timePosTable - time access to a positional table. */
#include "common.h"
#include "options.h"
#include "hdb.h"
#include "jksql.h"
#include "hash.h"
#include "obscure.h"
#include "portable.h"

static char const rcsid[] = "$Id: timePosTable.c,v 1.3.24.1 2008/07/31 05:21:41 markd Exp $";

static void usage()
/* Explain usage and exit. */
{
errAbort(
    "timePosTable - time access to a positional table\n"
    "usage:\n"
    "    timePosTable [options] db table\n"
    "options:\n"
    "\n"
    "   -chrom=chr - run test for this chromosome, maybe repeated.\n"
    "    default is longest chromosome. Specify `all' to use all chromosomes.\n"
    "   -minSize=10000 - minimum base range\n"
    "   -maxSize=1000000 - maximum base range\n"
    "   -sizeMult=10 - amount to multiply size by for each pass\n"
    "   -iter=2 - number of iterations\n"
    "\n"
    "Read ranges across the specified chromsome(s) using hRangeQuery.\n"
    "Starting with ranges of min size, reading random, not overlapping\n"
    "ranges until all chromsome are covered.  Then increase size by sizeMult\n"
    "and repeat until maxSize is exceeded.  Tables are flushed between each\n"
    "pass.  This is repeated for each interation.\n");
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING|OPTION_MULTI},
   {"minSize", OPTION_INT},
   {"maxSize", OPTION_INT},
   {"sizeMult", OPTION_INT},
   {"iter", OPTION_INT},
   {NULL, 0}
};

struct chromSize
/* chrom and size */
{
    struct chromSize *next;
    char *chrom;
    int size;
};

static struct chromSize *chromSizeNew(char *chrom, int size)
/* create a chromSize object */
{
struct chromSize *cs;
AllocVar(cs);
cs->chrom = cloneString(chrom);
cs->size  = size;
return cs;
}

struct chromRange
/* a chrom range */
{
    struct chromRange *next;
    char *chrom;      // not allocated here!!
    int start;        // chrom subrange
    int end;
    double sortKey;   // random number used for sorting randomly.
};

static struct chromRange *chromRangeNew(char *chrom, int start, int end)
/* alloc a new chromRange */
{
struct chromRange *cr;
AllocVar(cr);
cr->chrom = chrom;
cr->start = start;
cr->end = end;
cr->sortKey = drand48();
return cr;
}

static int chromRangeCmp(const void *va, const void *vb)
/* compare chromRange by random sortKey */
{
const struct chromRange *a = *((struct chromRange **)va);
const struct chromRange *b = *((struct chromRange **)vb);
if (a->sortKey < b->sortKey)
    return -1;
else if (a->sortKey > b->sortKey)
    return 1;
else
    return 0;
}

static struct chromRange *buildChromRanges(int size, struct chromSize *chrom)
/* build ranges for the specified chrom */
{
struct chromRange *ranges = NULL;
int start;
for (start = 0; start < chrom->size; start += size)
    {
    int end = start+size;
    if (end > chrom->size)
        end = chrom->size;
    slSafeAddHead(&ranges, chromRangeNew(chrom->chrom, start, end));
    }
return ranges;
}

static struct chromRange *buildRanges(int size, struct chromSize *chroms)
/* build a randomly sorted list of ranges for the given read size */
{
struct chromRange *ranges = NULL;
struct chromSize *chrom;
for (chrom = chroms; chrom != NULL; chrom = chrom->next)
    ranges = slCat(ranges, buildChromRanges(size, chrom));
slSort(&ranges, chromRangeCmp);
return ranges;
}

static void queryRange(struct sqlConnection *conn, char *table, struct chromRange *range)
/* query one range */
{
struct sqlResult *sr = hRangeQuery(conn, table, range->chrom,
                                   range->start, range->end, NULL, NULL);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    continue;
sqlFreeResult(&sr);
}

static double timePass(int pass, struct sqlConnection *conn, char *table,
                       int size, struct chromSize *chroms)
/* one time pass for the given size */
{
struct chromRange *ranges = buildRanges(size, chroms);
long startTime = clock1000();
struct chromRange *range;
for (range = ranges; range != NULL; range = range->next)
    queryRange(conn, table, range);

double elapsed = ((double)(clock1000()-startTime))/1000.0;
printf("pass: %d  range size: %d  ranges: %d  time: %g seconds\n", pass, size,
       slCount(ranges), elapsed);
slFreeList(&ranges);
sqlUpdate(conn, "flush tables");
return elapsed;
}

static void timePosTableIter(char *db, char *table, int minSize, int maxSize,
                             int sizeMult,  int iter, struct chromSize *chroms)
/* run one iteration of test */
{
struct sqlConnection *conn = sqlConnect(db);
double totalTime = 0.0;
int sz;
int pass = 0;
for (sz = minSize; sz <= maxSize; sz *= sizeMult, pass++)
    totalTime += timePass(pass, conn, table, sz, chroms);
printf("iteration: %d  passes: %d  time: %g seconds\n", iter, pass,
       totalTime);
sqlDisconnect(&conn);
}

static void timePosTable(char *db, char *table, int minSize, int maxSize,
                         int sizeMult,  int iterations, struct chromSize *chroms)
/* time access to a positional table. */
{
printf("timing %s.%s iterations: %d  sizes: %d to %d  grow by: *%d  chroms: %d\n",
       db, table, iterations, minSize, maxSize, sizeMult, slCount(chroms));
int iter;
for (iter = 0; iter < iterations; iter++)
    timePosTableIter(db, table, minSize, maxSize, sizeMult, iter, chroms);
}

static struct chromSize *getLongestChrom(char *db)
/* find the longest chrom */
{
struct hash *chrTbl = hChromSizeHash(db);
char *maxChrom = NULL;;
int maxSize = 0;
struct hashCookie ck = hashFirst(chrTbl);
struct hashEl *hel;
while ((hel = hashNext(&ck)) != NULL)
    {
    if (ptToInt(hel->val) > maxSize)
        {
        maxChrom = hel->name;
        maxSize = ptToInt(hel->val);
        }
    }
hashFree(&chrTbl);
return chromSizeNew(maxChrom, maxSize);
}

static struct chromSize *getAllChroms(char *db)
/* get list of all chroms */
{
struct chromSize *chroms = NULL;
struct hash *chrTbl = hChromSizeHash(db);
struct hashCookie ck = hashFirst(chrTbl);
struct hashEl *hel;
while ((hel = hashNext(&ck)) != NULL)
    slSafeAddHead(&chroms, chromSizeNew(hel->name, ptToInt(hel->val)));
hashFree(&chrTbl);
return chroms;
}

static struct chromSize *getChromsFromSpecs(char *db, struct slName *specs)
/* build chromSizes from results of */
{
struct hash *chrTbl = hChromSizeHash(db);
struct chromSize *chroms = NULL;
struct slName *spec;
for (spec = specs;  spec != NULL; spec = spec->next)
    slSafeAddHead(&chroms, chromSizeNew(spec->name, hashIntVal(chrTbl, spec->name)));
hashFree(&chrTbl);
return chroms;
}

static struct chromSize *getChroms(char *db)
/* get list of chroms to use in tests */
{
struct slName *specs = optionMultiVal("chrom", NULL);
if (specs == NULL)
    return getLongestChrom(db);
else if (slNameInList(specs, "all"))
    return getAllChroms(db);
else
    return getChromsFromSpecs(db, specs);
}

int main(int argc, char *argv[])
/* entry point */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *db = argv[1];
char *table = argv[2];
timePosTable(db, table,
             optionInt("minSize", 10000),
             optionInt("maxSize", 1000000),
             optionInt("sizeMult", 10),
             optionInt("iter", 2),
             getChroms(db));
return 0;
}
