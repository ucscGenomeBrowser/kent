/* hgMaf.c - Stuff to load up mafs from the browser database. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "maf.h"
#include "hdb.h"
#include "scoredRef.h"
#include "hgMaf.h"

static char const rcsid[] = "$Id: hgMaf.c,v 1.10 2008/05/31 13:44:21 braney Exp $";

int mafCmp(const void *va, const void *vb)
/* Compare to sort based on start of first component. */
{
const struct mafAli *a = *((struct mafAli **)va);
const struct mafAli *b = *((struct mafAli **)vb);
return a->components->start - b->components->start;
}


struct mafAli *mafLoadInRegion2(struct sqlConnection *conn, 
    struct sqlConnection *conn2, char *table, char *chrom, int start, int end)
/* Return list of alignments in region. */
{
char **row;
unsigned int extFileId = 0;
struct mafAli *maf, *mafList = NULL;
struct mafFile *mf = NULL;
int rowOffset;

struct sqlResult *sr = hRangeQuery(conn, table, chrom, 
    start, end, NULL, &rowOffset);

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct scoredRef ref;
    scoredRefStaticLoad(row + rowOffset, &ref);
    if (ref.extFile != extFileId)
	{
	char *path = hExtFileNameC(conn2, "extFile", ref.extFile);
	mafFileFree(&mf);
	mf = mafOpen(path);
	extFileId = ref.extFile;
	}
    lineFileSeek(mf->lf, ref.offset, SEEK_SET);
    maf = mafNext(mf);
    if (maf == NULL)
        internalErr();
    slAddHead(&mafList, maf);
    }
sqlFreeResult(&sr);
mafFileFree(&mf);
slReverse(&mafList);
/* hRangeQuery may return items out-of-order when bin is used in the query,
 * so sort here in order to avoid trouble at base-level view: */
slSort(&mafList, mafCmp);
return mafList;
}

struct mafAli *mafLoadInRegion(struct sqlConnection *conn, char *table,
	char *chrom, int start, int end)
{
struct sqlConnection *conn2 = hgAllocConn();
struct mafAli *ret = mafLoadInRegion2(conn, conn2, table, chrom, start, end);
hgFreeConn(&conn2);
return ret;
}

struct mafAli *axtLoadAsMafInRegion(struct sqlConnection *conn, char *table,
	char *chrom, int start, int end, 
	char *tPrefix, char *qPrefix, int tSize,  struct hash *qSizeHash)
/* Return list of alignments in region from axt external file as a maf. */
{
char **row;
unsigned int extFileId = 0;
struct lineFile *lf = NULL;
struct mafAli *maf, *mafList = NULL;
struct axt *axt;
int rowOffset;
struct sqlResult *sr = hRangeQuery(conn, table, chrom, 
    start, end, NULL, &rowOffset);

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct scoredRef ref;
    scoredRefStaticLoad(row + rowOffset, &ref);
    if (ref.extFile != extFileId)
	{
	char *path = hExtFileName("extFile", ref.extFile);
	lf = lineFileOpen(path, TRUE);
	extFileId = ref.extFile;
	}
    lineFileSeek(lf, ref.offset, SEEK_SET);
    axt = axtRead(lf);
    if (axt == NULL)
        internalErr();
    maf = mafFromAxt(axt, tSize, tPrefix, hashIntVal(qSizeHash, axt->qName), qPrefix);
    axtFree(&axt);
    slAddHead(&mafList, maf);
    }
sqlFreeResult(&sr);
lineFileClose(&lf);
slReverse(&mafList);
return mafList;
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

static int oneOrgCmp(const void *va, const void *vb)
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

static int countAlpha(char *s)
/* Count up number of alphabetic characters in string */
{
int count = 0;
char c;
while ((c = *s++) != 0)
    if (isalpha(c))
        ++count;
return count;
}

struct mafAli *hgMafFrag(
	char *database,     /* Database, must already have hSetDb to this */
	char *track, 	    /* Name of MAF track */
	char *chrom, 	    /* Chromosome (in database genome) */
	int start, int end, /* start/end in chromosome */
	char strand, 	    /* Chromosome strand. */
	char *outName, 	    /* Optional name to use in first component */
	struct slName *orderList /* Optional order of organisms. */
	)
/* mafFrag- Extract maf sequences for a region from database.  
 * This creates a somewhat unusual MAF that extends from start
 * to end whether or not there are actually alignments.  Where
 * there are no alignments (or alignments missing a species)
 * a . character fills in.   The score is always zero, and
 * the sources just indicate the species.  You can mafFree this
 * as normal. */
{
int chromSize = hChromSize(chrom);
struct sqlConnection *conn = hAllocConn();
struct dnaSeq *native = hChromSeq(chrom, start, end);
struct mafAli *maf, *mafList = mafLoadInRegion(conn, track, chrom, start, end);
char masterSrc[128];
struct hash *orgHash = newHash(10);
struct oneOrg *orgList = NULL, *org, *nativeOrg = NULL;
int curPos = start, symCount = 0;
struct slName *name;
int order = 0;

/* Check that the mafs are really copacetic, the particular
 * subtype we think is in the database that this (relatively)
 * simple code can handle. */
safef(masterSrc, sizeof(masterSrc), "%s.%s", database, chrom);
mafCheckFirstComponentSrc(mafList, masterSrc);
mafCheckFirstComponentStrand(mafList, '+');
slSort(&mafList, mafCmp);

/* Prebuild organisms if possible from input orderList. */
for (name = orderList; name != NULL; name = name->next)
    {
    AllocVar(org);
    slAddHead(&orgList, org);
    hashAddSaveName(orgHash, name->name, org, &org->name);
    org->dy = dyStringNew(native->size*1.5);
    org->order = order++;
    if (nativeOrg == NULL)
        nativeOrg = org;
    }
if (orderList == NULL)
    {
    AllocVar(org);
    slAddHead(&orgList, org);
    hashAddSaveName(orgHash, database, org, &org->name);
    org->dy = dyStringNew(native->size*1.5);
    if (nativeOrg == NULL)
        nativeOrg = org;
    }

/* Go through all mafs in window, mostly building up
 * org->dy strings. */
for (maf = mafList; maf != NULL; maf = maf->next)
    {
    struct mafComp *mc, *mcMaster = maf->components;
    struct mafAli *subMaf = NULL;
    order = 0;
    if (curPos < mcMaster->start) 
	{
	fillInMissing(nativeOrg, orgList, native, start, 
		curPos, mcMaster->start);
	symCount += mcMaster->start - curPos;
	}
    if (curPos < mcMaster->start + mcMaster->size) /* Prevent worst 
    						    * backtracking */
	{
	if (mafNeedSubset(maf, masterSrc, curPos, end))
	    {
	    subMaf = mafSubset(maf, masterSrc, curPos, end);
	    if (subMaf == NULL)
	        continue;
	    }
	else
	    subMaf = maf;
	for (mc = subMaf->components; mc != NULL; mc = mc->next, ++order)
	    {
	    /* Extract name up to dot into 'orgName' */
	    char buf[128], *e, *orgName;

	    if ((mc->size == 0) || (mc->srcSize == 0)) /* skip over components without sequence */
		continue;

	    mc->leftStatus = mc->rightStatus = 0; /* squash annotation */

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
		if (orderList != NULL)
		   errAbort("%s is not in orderList", orgName);
		AllocVar(org);
		slAddHead(&orgList, org);
		hashAddSaveName(orgHash, orgName, org, &org->name);
		org->dy = dyStringNew(native->size*1.5);
		dyStringAppendMultiC(org->dy, '.', symCount);
		if (nativeOrg == NULL)
		    nativeOrg = org;
		}
	    if (orderList == NULL && order > org->order)
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
    }
if (curPos < end)
    {
    fillInMissing(nativeOrg, orgList, native, start, curPos, end);
    symCount += end - curPos;
    }
mafAliFreeList(&mafList);

slSort(&orgList, oneOrgCmp);
if (strand == '-')
    {
    for (org = orgList; org != NULL; org = org->next)
	reverseComplement(org->dy->string, org->dy->stringSize);
    }

/* Construct our maf */
AllocVar(maf);
maf->textSize = symCount;
for (org = orgList; org != NULL; org = org->next)
    {
    struct mafComp *mc;
    AllocVar(mc);
    if (org == orgList)
        {
	if (outName != NULL)
	    {
	    mc->src = cloneString(outName);
	    mc->srcSize = native->size;
	    mc->strand = '+';
	    mc->start = 0;
	    mc->size = native->size;
	    }
	else
	    {
	    mc->src = cloneString(masterSrc);
	    mc->srcSize = chromSize;
	    mc->strand = strand;
	    if (strand == '-')
	       reverseIntRange(&start, &end, chromSize);
	    mc->start = start;
	    mc->size = end-start;
	    }
	}
    else
        {
	int size = countAlpha(org->dy->string);
	mc->src = cloneString(org->name);
	mc->srcSize = size;
	mc->strand = '+';
	mc->start = 0;
	mc->size = size;
	}
    mc->text = cloneString(org->dy->string);
    dyStringFree(&org->dy);
    slAddHead(&maf->components, mc);
    }
slReverse(&maf->components);

slFreeList(&orgList);
freeHash(&orgHash);
hFreeConn(&conn);
return maf;
}

struct consWiggle *wigMafWiggles(struct trackDb *tdb)
/* get conservation wiggle table names and labels from trackDb setting,
   ignoring those where table doesn't exist */
{
char *fields[20];
int fieldCt;
int i;
char *wigTable, *wigLabel;
struct consWiggle *wig, *wigList = NULL;
char *setting = trackDbSetting(tdb, CONS_WIGGLE);
if (!setting)
    return NULL;
fieldCt = chopLine(cloneString(setting), fields);
for (i = 0; i < fieldCt; i += 3)
    {
    wigTable = fields[i];
    if (hTableExists(wigTable));
        {
        AllocVar(wig);
        wig->table = cloneString(wigTable);
        wigLabel = (i+1 == fieldCt ? DEFAULT_CONS_LABEL : fields[i+1]);
        wig->leftLabel = cloneString(wigLabel);
        wigLabel = (i+2 >= fieldCt ? wig->leftLabel : fields[i+2]);
        wig->uiLabel = cloneString(wigLabel);
        slAddTail(&wigList, wig);
        }
    }
return wigList;
}

char *wigMafWiggleVar(struct trackDb *tdb, struct consWiggle *wig)
/* Return name of cart variable for this cons wiggle */
{
char option[128];
safef(option, sizeof option, "%s.cons.%s", tdb->tableName, wig->leftLabel);
return (cloneString(option));
}
