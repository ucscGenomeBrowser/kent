/* encodeSynteny - create HTML files to compare syntenic predictions from liftOver and Mercator */
#include "common.h"
#include "jksql.h"
#include "dystring.h"

//char *assem;

struct sizeList
{
    struct sizeList *next;
    char  *name;
    int    size; 
};

struct namedRegion
{
    struct namedRegion *next;
    int chromStart, chromEnd;
    char *chrom, *name;
};

struct mercatorSummary
{
    struct mercatorSummary *next;
    int chromStart, chromEnd;
    int diffStart, diffEnd;
    char *chrom, *name;
};

struct consensusRegion
{
    struct consensusRegion *next;
    char *chrom;
    int chromStart, chromEnd;
};

