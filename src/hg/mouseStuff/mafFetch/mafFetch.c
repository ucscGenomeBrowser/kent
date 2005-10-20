/* mafFetch - get overlapping records from an MAF using an index table. */
#include "common.h"
#include "options.h"
#include "bed.h"
#include "hdb.h"
#include "maf.h"
#include "scoredRef.h"

static char const rcsid[] = "$Id: mafFetch.c,v 1.2 2005/10/20 09:03:12 markd Exp $";

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

struct scoredRef *loadMafRefs(struct sqlConnection *conn, char *table,
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

void copyMafs(struct sqlConnection *conn, struct scoredRef *refs, FILE *outFh)
/* copy mafs */
{
struct mafFile *maf = NULL;
unsigned extFile = 0;
unsigned recCnt = 0;
struct scoredRef *ref;
struct mafAli *ali;

for (ref = refs; ref != NULL; ref = ref->next)
    {
    if ((maf == NULL) || (extFile != ref->extFile))
        {
        mafFileFree(&maf);
        maf = mafOpen(hExtFileName("extFile", ref->extFile));
        extFile = ref->extFile;
        if (recCnt == 0)
            mafWriteStart(outFh, maf->scoring);
        }
    lineFileSeek(maf->lf, ref->offset, SEEK_SET);
    ali = mafNext(maf);
    if (ali == NULL)
        errAbort("unexpected EOF on MAF %s", maf->lf->fileName);
    mafWrite(outFh, ali);
    mafAliFree(&ali);
    recCnt++;
    }
mafFileFree(&maf);
}

void mafFetch(char *db, char *table, char *overBed, char *mafOut)
/* mafFetch - get overlapping records from an MAF using an index table */
{
struct bed *beds;
struct sqlConnection *conn;
struct scoredRef *refs;
FILE *outFh;

hSetDb(db);
beds = bedLoadNAll(overBed, 3);
slSort(&beds, bedCmp);

conn = hAllocConn();
refs = loadMafRefs(conn, table, beds);
bedFreeList(&beds);

outFh = mustOpen(mafOut, "w");
copyMafs(conn, refs, outFh);
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
