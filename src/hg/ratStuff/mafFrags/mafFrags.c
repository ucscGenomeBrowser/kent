/* mafFrags - Collect MAFs from regions specified in a 6 column bed file. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "maf.h"
#include "hdb.h"
#include "hgMaf.h"


static char const rcsid[] = "$Id: mafFrags.c,v 1.1 2003/10/25 06:00:17 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafFrags - Collect MAFs from regions specified in a 6 column bed file\n"
  "usage:\n"
  "   mafFrags database track in.bed out.maf\n"
  "options:\n"
  "   -orgs=org.txt - File with list of databases/organisms in order\n"
  );
}

static struct optionSpec options[] = {
   {"orgs", OPTION_STRING},
   {NULL, 0},
};

void mafFrags(char *database, char *track, char *bedFile, char *mafFile)
/* mafFrags - Collect MAFs from regions specified in a 6 column bed file. */
{
struct slName *orgList = NULL;
struct lineFile *lf = lineFileOpen(bedFile, TRUE);
FILE *f = mustOpen(mafFile, "w");
char *row[6];

hSetDb(database);
if (optionExists("orgs"))
    {
    char *orgFile = optionVal("orgs", NULL);
    char *buf;
    readInGulp(orgFile, &buf, NULL);
    orgList = stringToSlNames(buf);
    }

mafWriteStart(f, "zero");
while (lineFileRow(lf, row))
    {
    struct bed *bed = bedLoadN(row, ArraySize(row));
    struct mafAli *maf = hgMafFrag(database, track, 
    	bed->chrom, bed->chromStart, bed->chromEnd, bed->strand[0],
	bed->name, orgList);
    mafWrite(f, maf);
    mafAliFree(&maf);
    bedFree(&bed);
    }
mafWriteEnd(f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
mafFrags(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
