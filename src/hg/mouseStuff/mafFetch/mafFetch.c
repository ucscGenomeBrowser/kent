/* mafFetch - get overlapping records from an MAF using an index table. */
#include "common.h"
#include "options.h"
#include "bed.h"
#include "hdb.h"
#include "maf.h"
#include "hgMaf.h"

static char const rcsid[] = "$Id: mafFetch.c,v 1.1 2005/10/20 07:56:39 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafFetch - get overlapping records from an MAF using an index table\n"
  "usage:\n"
  "   mafFetch db table overBed mafOut\n"
  "\n"
  "Select MAF records overlapping records in the BED using the\n"
  "the database table to lookup the file and record offset.\n"
  "Only the first 3 columns are requried in the bed.\n"
  "\n"
  "Options:\n"
  );
}

void mafFetchBed(struct sqlConnection *conn, char *table, struct bed *bed,
                 FILE *mafFh)
/* copy maf records overlapping bed */
{
struct mafAli *ali, *aliLst;
aliLst = mafLoadInRegion(conn, table, bed->chrom, bed->chromStart, bed->chromEnd);

for (ali = aliLst; ali != NULL; ali = ali->next)
    mafWrite(mafFh, ali);

mafAliFree(&ali);
}

void mafFetch(char *db, char *table, char *overBed, char *mafOut)
/* mafFetch - get overlapping records from an MAF using an index table */
{
struct bed *beds, *bed;
struct sqlConnection *conn;
FILE *mafFh;

hSetDb(db);
beds = bedLoadNAll(overBed, 3);
slSort(&beds, bedCmp);

conn = hAllocConn();
mafFh = mustOpen(mafOut, "w");
mafWriteStart(mafFh, NULL);

for (bed = beds; bed != NULL; bed = bed->next)
    mafFetchBed(conn, table, bed, mafFh);

carefulClose(&mafFh);
hFreeConn(&conn);
bedFreeList(&beds);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
mafFetch(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
