/* superStitcher - brings together short overlapping alignments into long
 * ones. */
#include "common.h"
#include "dnautil.h"
#include "jksql.h"
#include "hgap.h"
#include "fuzzyFind.h"
#include "maDbRep.h"
#include "supStitch.h"

struct ssInput
/* Stitcher alignment structure. */
    {
    struct ssInput *next;
    HGID qId;
    int qStart;
    int qEnd;
    signed char orientation;	/* Orientation relative to first contig */
    bool isEst;
    int tStart,tEnd;
    struct mrnaAli *ma;
    };

void ssInputFree(struct ssInput **pSsIn)
/* Free up ssInput structure. */
{
struct ssInput *ssIn;
if ((ssIn = *pSsIn) != NULL)
    {
    mrnaAliFree(&ssIn->ma);
    freez(pSsIn);
    }
}

void ssInputFreeList(struct ssInput **pList)
/* Free up list of ssInput structures. */
{
struct ssInput *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ssInputFree(&el);
    }
*pList = NULL;
}

int cmpSsAliQuery(const void *va, const void *vb)
/* Sort ssInput list by qId,orientation,qStart */
{
const struct ssInput *a = *((struct ssInput **)va);
const struct ssInput *b = *((struct ssInput **)vb);
int diff = a->qId - b->qId;
if (diff == 0)
    diff = a->orientation - b->orientation;
if (diff == 0)
    diff = a->qStart - b->qStart;
return diff;
}

void dumpFf(struct ffAli *aliList, struct dnaSeq *mrna, struct dnaSeq *geno)
/* Print out ali */
{
DNA *needle = mrna->dna;
DNA *haystack = geno->dna;
struct ffAli *ali;

printf("%s", mrna->name);
for (ali = aliList; ali != NULL; ali = ali->right)
    {
    printf(" (%d %d %d %d)", ali->nStart-needle, ali->nEnd-needle,
        ali->hStart-haystack, ali->hEnd-haystack);
    }
printf("\n");
}

struct contigInfo
/* Information on a particular contig. */
    {
    struct contigInfo *next;	/* Next in list. */
    HGID contigId;              /* Id of contig. */
    int rawAliCount;            /* Number of unstitched alignments. */
    HGID bacId;                 /* Id of enclosing bac. */
    char bacAcc[13];            /* Accession of enclosing bac. */
    int contigIx;               /* Position of contig within bac. */
    };

struct contigInfo *getContigInfo(struct sqlConnection *conn, char *sourceTable)
/* Get contig info from source table. */
{
struct sqlResult *sr;
char **row;
struct contigInfo *ciList = NULL, *ci;
char query[256];

sprintf(query, "select contig.id,count(*),bac.id,bac.acc,contig.piece "
    "from %s,contig,bac "
    "where psHits.tStartContig = contig.id and contig.bac = bac.id "
    "group by contig.id", sourceTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(ci);
    ci->contigId = sqlUnsigned(row[0]);
    ci->rawAliCount = sqlUnsigned(row[1]);
    ci->bacId = sqlUnsigned(row[2]);
    strcpy(ci->bacAcc, row[3]);
    ci->contigIx = sqlUnsigned(row[4]);
    slAddHead(&ciList, ci);
    }
sqlFreeResult(&sr);
slReverse(&ciList);
return ciList;
}

void saveBundle(struct ssBundle *bundle, char *genoAcc, int contigIx, FILE *destTab)
/* Save (stitched) contents of bundle to database. */
{
struct dnaSeq *mrnaSeq =  bundle->qSeq;
struct dnaSeq *genoSeq = bundle->genoSeq;
boolean isRc = (bundle->orientation < 0);
boolean isEst = bundle->isEst;
struct mrnaAli *ma;
struct ssFfItem *ffl;
struct ffAli *ff;

for (ffl = bundle->ffList; ffl != NULL; ffl = ffl->next)
    {
    ff = ffl->ff;
    ma = ffToMa(ffl->ff, mrnaSeq, bundle->qAcc, genoSeq, genoAcc, 
	 contigIx, isRc, isEst);
    mrnaAliTabOut(ma, destTab);
    mrnaAliFree(&ma);
    }
}


void stitch(char *sourceTable, char *destTable)
/* Glue together mRNAs in source table into mRNAs in dest table */
{
struct sqlConnection *conn = hgStartUpdate();
struct sqlResult *sr;
char **row;
char query[128];
FILE *destTab = hgCreateTabFile(destTable);
struct contigInfo *ciList, *ci;
struct hgContig *contig;
struct ssFfItem *ffl;
int origCount = 0;
int newCount = 0;


ciList = getContigInfo(conn, sourceTable);
for (ci = ciList; ci != NULL; ci = ci->next)
    {
    struct ssInput *inList = NULL, *ssIn;
    struct ssBundle *bundleList = NULL, *bundle;
    HGID lastId = 0;
    struct dnaSeq *genoSeq = NULL, *mrnaSeq = NULL;
    struct mrnaAli *ma;

    /* Fetch DNA for a range of targets. */
    printf("Processing %d alignments in %s contig %d\n", 
    	ci->rawAliCount, ci->bacAcc, ci->contigIx);
    contig = hgGetContig(ci->bacAcc, ci->contigIx);
    genoSeq = hgContigSeq(contig);

    /* Get all the alignments from source table that hit a
     * particular range of target. */
    sprintf(query, "select * from %s where tStartContig = %lu", 
    	sourceTable, ci->contigId); 
    sr = sqlGetResult(conn,query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	AllocVar(ssIn);
	ssIn->ma = ma = mrnaAliLoad(row);
	ssIn->orientation = ma->orientation;
	ssIn->qId = ma->qId;
	ssIn->qStart = ma->qStart;
	ssIn->qEnd = ma->qEnd;
	ssIn->tStart = ma->tStartPos;	/* Adjust coordinates. */
	ssIn->tEnd = ma->tEndPos;            /* Adjust coordinates. */
	slAddTail(&inList, ssIn);
	}
    sqlFreeResult(&sr);
    slSort(&inList, cmpSsAliQuery);
    origCount += slCount(inList);

    /* Break list into bundles that share the same query sequence. */
    for (ssIn = inList; ssIn != NULL; ssIn = ssIn->next)
	{
	if (lastId != ssIn->qId)
	    {
	    lastId = ssIn->qId;
	    AllocVar(bundle);
	    bundle->qId = ssIn->qId;
	    strcpy(bundle->qAcc, ssIn->ma->qAcc);
	    bundle->orientation = ssIn->orientation;
	    bundle->qSeq = mrnaSeq = hgRnaSeq(bundle->qAcc);
	    if (bundle->orientation < 0)
		reverseComplement(mrnaSeq->dna, mrnaSeq->size);
	    bundle->genoSeq = genoSeq;
	    slAddHead(&bundleList, bundle);
	    }
	AllocVar(ffl);
	ffl->ff = maToFf(ssIn->ma, bundle->qSeq->dna, bundle->genoSeq->dna);
	slAddHead(&bundle->ffList, ffl);
	}
    ssInputFreeList(&inList);
    slReverse(&bundleList);

    /* Stitch together individual bundles. */
    for (bundle = bundleList; bundle != NULL; bundle = bundle->next)
	{
	struct dnaSeq *mrnaSeq = bundle->qSeq;
	newCount += ssStitch(bundle);
	saveBundle(bundle, ci->bacAcc, ci->contigIx, destTab);
	freeDnaSeq(&bundle->qSeq);
	}
    freeDnaSeq(&genoSeq);
    ssBundleFreeList(&bundleList);
    }
slFreeList(&ciList);
fclose(destTab);
hgLoadTabFile(conn, destTable);
hgEndUpdate(&conn, "Stitching %d alignments into %d", 
   origCount, newCount);   
}


int main(int argc, char *argv[])
/* Initialize libraries. Process command line */
{
dnaUtilOpen();
stitch("psHits", "mrnaAli");
}
