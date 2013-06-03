/* fakeSeqDiffs - Fake sequence differences from a custom track.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "options.h"
#include "snp126.h"
#include "hdb.h"


char *database = "hg18";
char *chrom = "chr11";
int chromStart = 116578928;
int chromEnd = 116580717;

char *names[] =
    {
    "Jim", "Joe", "Jeff", "Jerry", "Jack", "Jorge", "Jiame", "Jose",
    "Sara", "Susan", "Sandy", "Sally", "Mary", "Maria", "Martha",
    "Betty", "Bob", "Maya", "Sandra", "Sofie", "Rupert", "Rick",
    "Francis", "Eric", "David", "Charles", "Jane", "Janet", "Hanna",
    "Diane", "Donna", "Dot", "Mira", "Tisa", "Heidi", "Joseph", "Harley",
    "Chantelle", "Amber", "Heather", "Wally", "Linus", "Juan", "Chad",
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fakeSeqDiffs - Fake sequence differences from a custom track.\n"
  "usage:\n"
  "   fakeSeqDiffs output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

boolean randBit(double prob)
/* Return TRUE at given probability. */
{
int threshold = prob * RAND_MAX;
return (rand() <= threshold);
}

void fakeSeqDiffs(char *outFile)
/* fakeSeqDiffs - Fake sequence differences from a custom track.. */
{
FILE *f = mustOpen(outFile, "w");
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
char query[512];

sqlSafef(query, sizeof(query),
    "select * from snp126 where chrom='%s' and chromStart >= %d and chromEnd <= %d and avHet > 0.01", chrom, chromStart, chromEnd);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    static struct snp126 snp;
    int i;
    snp126StaticLoad(row+1, &snp);
    for (i=0; i<ArraySize(names); ++i)
        {
	if (randBit(snp.avHet))
	    {
	    fprintf(f, "%s\t%d\t%d\t", snp.chrom, snp.chromStart, snp.chromEnd);
	    fprintf(f, "%s\t", names[i]);
	    fprintf(f, "0\t+\t%d\t%d\t", snp.chromStart, snp.chromEnd);
	    fprintf(f, "200,0,0\n");
	    } 
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
fakeSeqDiffs(argv[1]);
return 0;
}
