/* fakeHomozygousity - Fake homozygousity data. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "fakeHomozygousity - Fake homozygousity data\n"
  "usage:\n"
  "   fakeHomozygousity XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void fakeHomozygousity(char *output)
/* fakeHomozygousity - Fake homozygousity data. */
{
FILE *f = mustOpen(output, "w");
struct sqlConnection *conn = sqlConnect("hg18");
struct sqlResult *sr = sqlGetResult(conn, "NOSQLINJ select chrom,chromStart,chromEnd from ctgPos");
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *chrom = row[0];
    int start = sqlUnsigned(row[1]);
    int end = sqlUnsigned(row[2]);
    int pos;
    for (pos = start; pos <end; pos += 10000)
	{
	char *homozygous = (((rand()&3)==0) ? "1" : "0");
	fprintf(f, "%s\t%d\t%s\n", chrom, pos, homozygous);
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
fakeHomozygousity(argv[1]);
return 0;
}
