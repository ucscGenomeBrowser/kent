/* mafFrag - Extract maf sequences for a region from database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "dnaseq.h"
#include "maf.h"
#include "hdb.h"
#include "scoredRef.h"

static char const rcsid[] = "$Id: mafFrag.c,v 1.1 2003/10/25 01:57:02 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafFrag - Extract maf sequences for a region from database\n"
  "usage:\n"
  "   mafFrag database mafTrack chrom start end strand out.maf\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


int mafCmp(const void *va, const void *vb)
/* Compare to sort based on start of first component. */
{
const struct mafAli *a = *((struct mafAli **)va);
const struct mafAli *b = *((struct mafAli **)vb);
return a->components->start - b->components->start;
}

static void mafCheckFirstComponentSrc(struct mafAli *mafList, char *src)
/* Check that the first component of each maf has given src. */
{
struct mafAli *maf;
for (maf = mafList; maf != NULL; maf = maf->next)
    {
    if (!sameString(maf->components->src, src))
        errAbort("maf first component isn't %s", src);
    }
}

static void mafCheckFirstComponentStrand(struct mafAli *mafList, char strand)
/* Check that the first component of each maf has given strand. */
{
struct mafAli *maf;
for (maf = mafList; maf != NULL; maf = maf->next)
    {
    if (maf->components->strand != strand)
        errAbort("maf first component isn't %c", strand);
    }
}

struct oneOrg
/* Info on one organism. */
    {
    struct oneOrg *next;
    char *name;		/* Name - allocated in hash */
    int order;		/* Help sort organisms. */
    struct dyString *dy;	/* Associated alignment for this organism. */
    boolean hit;	/* Flag to see if hit this time around. */
    };

int oneOrgCmp(const void *va, const void *vb)
/* Compare to sort based on order. */
{
const struct oneOrg *a = *((struct oneOrg **)va);
const struct oneOrg *b = *((struct oneOrg **)vb);
return a->order - b->order;
}

static void fillInMissing(struct oneOrg *nativeOrg, struct oneOrg *orgList,
	struct dnaSeq *native, int seqStart, int curPos, int aliStart)
/* Fill in alignment strings in orgList with native sequence
 * for first organism, and dots for rest. */
{
int fillSize = aliStart - curPos;
int offset = curPos - seqStart;
struct oneOrg *org;
if (nativeOrg == NULL)
    return;
dyStringAppendN(nativeOrg->dy, native->dna + offset, fillSize);
for (org = orgList; org != NULL; org = org->next)
    {
    if (org != nativeOrg)
	dyStringAppendMultiC(org->dy, '.', fillSize);
    }
}

void mafFrag(char *database, char *track, char *chrom, 
	int start, int end, char *strand, char *outMaf)
/* mafFrag- Extract maf sequences for a region from database.  */
{
struct sqlConnection *conn = hAllocConn();
struct dnaSeq *native = hChromSeq(chrom, start, end);
struct mafAli *maf, *mafList = mafLoadInRegion(conn, track, chrom, start, end);
char masterSrc[128];
struct hash *orgHash = newHash(10);
struct oneOrg *orgList = NULL, *org, *nativeOrg = NULL;
int curPos = start, symCount = 0;

safef(masterSrc, sizeof(masterSrc), "%s.%s", database, chrom);
mafCheckFirstComponentSrc(mafList, masterSrc);
mafCheckFirstComponentStrand(mafList, '+');
slSort(&mafList, mafCmp);

for (maf = mafList; maf != NULL; maf = maf->next)
    {
    struct mafComp *mc, *mcMaster = maf->components;
    struct mafAli *subMaf = NULL;
    int order = 0;
    if (curPos < mcMaster->start) 
	{
	fillInMissing(nativeOrg, orgList, native, start, 
		curPos, mcMaster->start);
	symCount += mcMaster->start - curPos;
	}
    if (mafNeedSubset(maf, masterSrc, start, end))
	subMaf = mafSubset(maf, masterSrc, start, end);
    else
        subMaf = maf;
    for (mc = subMaf->components; mc != NULL; mc = mc->next, ++order)
        {
	/* Extract name up to dot into 'orgName' */
	char buf[128], *e, *orgName;
	e = strchr(mc->src, '.');
	if (e == NULL)
	    orgName = mc->src;
	else
	    {
	    int len = e - mc->src;
	    if (len >= sizeof(buf))
	        errAbort("organism/database name %s too long", mc->src);
	    memcpy(buf, mc->src, len);
	    buf[len] = 0;
	    orgName = buf;
	    }

	/* Look up dyString corresponding to  org, and create a
	 * new one if necessary. */
	org = hashFindVal(orgHash, orgName);
	if (org == NULL)
	    {
	    AllocVar(org);
	    slAddHead(&orgList, org);
	    hashAddSaveName(orgHash, orgName, org, &org->name);
	    org->dy = dyStringNew(native->size*1.5);
	    dyStringAppendMultiC(org->dy, '.', symCount);
	    if (nativeOrg == NULL)
	        nativeOrg = org;
	    }
	if (order > org->order)
	    org->order = order;
	org->hit = TRUE;

	/* Fill it up with alignment. */
	dyStringAppendN(org->dy, mc->text, subMaf->textSize);
	}
    for (org = orgList; org != NULL; org = org->next)
        {
	if (!org->hit)
	    dyStringAppendMultiC(org->dy, '.', subMaf->textSize);
	org->hit = FALSE;
	}
    symCount += subMaf->textSize;
    curPos = mcMaster->start + mcMaster->size;

    if (subMaf != maf)
        mafAliFree(&subMaf);
    }
if (curPos < end)
    fillInMissing(nativeOrg, orgList, native, start, curPos, end);

slSort(&orgList, oneOrgCmp);
for (org = orgList; org != NULL; org = org->next)
    {
    uglyf("%s\n", org->dy->string);
    }
uglyf("%d bases, %d mafs in region\n", native->size, slCount(mafList));
freeHash(&orgHash);
}

void mafFragCheck(char *database, char *track, 
	char *chrom, char *startString, char *endString,
	char *strand, char *outMaf)
/* mafFragCheck - Check parameters and convert to binary.
 * Call mafFrag. */
{
int start, end, chromSize;
if (!isdigit(startString[0]) || !isdigit(endString[0]))
    errAbort("%s %s not numbers", startString, endString);
start = atoi(startString);
end = atoi(endString);
hSetDb(database);
if (end <= start)
    errAbort("end before start!");
chromSize = hChromSize(chrom);
if (end > chromSize)
   errAbort("End past chromSize (%d > %d)", end, chromSize);
mafFrag(database, track, chrom, start, end, strand, outMaf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 8)
    usage();
mafFragCheck(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
return 0;
}
