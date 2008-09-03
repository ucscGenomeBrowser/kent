/* mafFetch - get overlapping records from an MAF using an index table. */
#include "common.h"
#include "options.h"
#include "bed.h"
#include "hdb.h"
#include "maf.h"
#include "scoredRef.h"

static char const rcsid[] = "$Id: mafFetch.c,v 1.4 2008/09/03 19:20:37 markd Exp $";

static void usage()
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

static void loadBedMafRefs(struct sqlConnection *conn, char *table,
                           struct bed *bed, struct scoredRef **refs)
/* load maf refs overlapping range and add to the list */
{
int rowOff;
char **row;
struct sqlResult *sr = hRangeQuery(conn, table, bed->chrom, bed->chromStart, bed->chromEnd,
                                   NULL, &rowOff);
while ((row = sqlNextRow(sr)) != NULL)
    slSafeAddHead(refs, scoredRefLoad(row+rowOff));

sqlFreeResult(&sr);
}

static int cmpScoredRef(const void *va, const void *vb)
/* score ref comparisons */
{
const struct scoredRef *a = *((struct scoredRef **)va);
const struct scoredRef *b = *((struct scoredRef **)vb);
int dif = a->chromStart - b->chromStart;
if (dif == 0)
    dif = a->chromEnd - b->chromEnd;
return dif;
}

static struct scoredRef *loadMafRefs(struct sqlConnection *conn, char *table,
                                     struct bed *beds)
/* load all maf refs and sort to make unique */
{
struct bed *bed;
struct scoredRef *refs = NULL;

for (bed = beds; bed != NULL; bed = bed->next)
    loadBedMafRefs(conn, table, bed, &refs);
slUniqify(&refs, cmpScoredRef, scoredRefFree);

return refs;
}

static void copyMafs(char *db, struct sqlConnection *conn,
                     struct scoredRef *refs, FILE *outFh)
/* copy mafs */
{
struct mafFile *mf = NULL;
unsigned extFile = 0;
unsigned recCnt = 0;
struct scoredRef *ref;
struct mafAli *maf;

for (ref = refs; ref != NULL; ref = ref->next)
    {
    if ((maf == NULL) || (extFile != ref->extFile))
        {
        mafFileFree(&mf);
        mf = mafOpen(hExtFileName(db, "extFile", ref->extFile));
        extFile = ref->extFile;
        if (recCnt == 0)
            mafWriteStart(outFh, mf->scoring);
        }
    lineFileSeek(mf->lf, ref->offset, SEEK_SET);
    maf = mafNext(mf);
    if (maf == NULL)
        errAbort("unexpected EOF on MAF %s", mf->lf->fileName);
    mafWrite(outFh, maf);
    mafAliFree(&maf);
    recCnt++;
    }
mafFileFree(&mf);
}

static void mafFetch(char *db, char *table, char *overBed, char *mafOut)
/* mafFetch - get overlapping records from an MAF using an index table */
{
struct bed *beds;
struct sqlConnection *conn;
struct scoredRef *refs;
FILE *outFh;

beds = bedLoadNAll(overBed, 3);
slSort(&beds, bedCmp);

conn = hAllocConn(db);
refs = loadMafRefs(conn, table, beds);
bedFreeList(&beds);

outFh = mustOpen(mafOut, "w");
copyMafs(db, conn, refs, outFh);
carefulClose(&outFh);

scoredRefFreeList(&refs);
hFreeConn(&conn);
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
