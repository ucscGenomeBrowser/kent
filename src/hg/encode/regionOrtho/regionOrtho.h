/* regionOrtho - create HTML files to compare syntenic predictions from liftOver and Mercator */
#include "common.h"
#include "jksql.h"
#include "dystring.h"
#include "linefile.h"

struct sizeList
{
    struct sizeList *next;
    int    chromStart, chromEnd;
    char  *chrom, *name;
    int    size; 
};

struct namedRegion
{
    struct namedRegion *next;
    int chromStart, chromEnd;
    char *chrom, *name;
    struct dyString *notes;
};

struct mercatorSummary
{
    struct mercatorSummary *next;
    int chromStart, chromEnd;
    int diffStart, diffEnd;
    char *chrom, *name;
    struct dyString *notes;
};

struct consensusRegion
{
    struct consensusRegion *next;
    char *chrom;
    int chromStart, chromEnd;
    struct dyString *notes;
};

