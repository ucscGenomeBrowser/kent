/* fakeLinkage - Fake some linkage data. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"

static char const rcsid[] = "$Id: fakeLinkage.c,v 1.1 2006/09/13 03:01:48 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fakeLinkage - Fake some linkage data\n"
  "usage:\n"
  "   fakeLinkage database output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void fakeChromLinkage(char *database, char *chrom, FILE *f)
/* Generate fake linkage data for one chromosome */
{
int size = hdbChromSize(database, chrom);
int stepSize = 5000000;
int pos;
uglyf("faking %s %d\n", chrom, size);
struct sqlConnection *conn = hAllocConn();
for (pos=0; pos<size; pos += stepSize)
    {
    int rowOffset = 0;
    char query[512];
    safef(query, sizeof(query), "select name from stsMap where chrom='%s' and chromStart >= %d && chromStart < %d", chrom, pos, pos+stepSize);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row = sqlNextRow(sr);
    if (row != NULL)
        {
	double x;
	if (rand()%250 == 0)
	    {
	    x = 3.5 + (double)rand()/RAND_MAX;
	    }
	else
	    {
	    x = (double)rand() / RAND_MAX + 0.001;
	    x = log(x) + 1;
	    if (x < -1)
	       x = -1;
	    }
        fprintf(f, "%s\t%f\n", row[0], x);
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}


void fakeLinkage(char *database, char *output)
/* fakeLinkage - Fake some linkage data. */
{
FILE *f = mustOpen(output, "w");
hSetDb(database);
int i;
for (i=1; i<=22; ++i)
    {
    char chromName[256];
    safef(chromName, sizeof(chromName), "chr%d", i);
    fakeChromLinkage(database, chromName, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
fakeLinkage(argv[1], argv[2]);
return 0;
}
