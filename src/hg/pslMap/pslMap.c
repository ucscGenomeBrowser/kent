/* pslMap - map PSLs alignments to new targets using alignments of the old
 * target to the new target. */
#include "common.h"
#include "pslTransMap.h"
#include "options.h"
#include "linefile.h"
#include "hash.h"
#include "psl.h"
#include "dnautil.h"
#include "chain.h"

static char const rcsid[] = "$Id: pslMap.c,v 1.10 2006/02/26 20:06:10 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"suffix", OPTION_STRING},
    {"keepTranslated", OPTION_BOOLEAN},
    {"chainMapFile", OPTION_BOOLEAN},
    {"swapMap", OPTION_BOOLEAN},
    {"mapInfo", OPTION_STRING},
    {NULL, 0}
};

/* Values parsed from command line */
static char* suffix = NULL;
static unsigned mapOpts = pslTransMapNoOpts;
static boolean chainMapFile = FALSE;
static boolean swapMap = FALSE;
static char* mapInfoFile = NULL;

/* count of non-fatal errors */
static int errCount = 0;

static char *mapInfoHdr =
    "#srcQName\t" "srcQStart\t" "srcQEnd\t" "srcQSize\t"
    "srcTName\t" "srcTStart\t" "srcTEnd\t"
    "srcStrand\t" "srcAligned\t"
    "mappingQName\t" "mappingQStart\t" "mappingQEnd\t"
    "mappingTName\t" "mappingTStart\t" "mappingTEnd\t"
    "mappingStrand\t" "mappingId\t"
    "mappedQName\t" "mappedQStart\t" "mappedQEnd\t"
    "mappedTName\t" "mappedTStart\t" "mappedTEnd\t"
    "mappedStrand\t"
    "mappedAligned\t" "qStartTrunc\t" "qEndTrunc\n";

static void usage(char *msg)
/* usage msg and exit */
{
/* message got huge, so it's in a generate file */
static char *usageMsg =
#include "usage.msg"
    ;
errAbort("%s\n%s", msg, usageMsg);
}

struct mapAln
/* Mapping alignment, psl plus additional information. */
{
    struct mapAln *next;  /* link for entries for same query */
    struct psl *psl;  /* psl, maybe created from a chain */
    int id;           /* chain id, or psl file row  */
};

static struct mapAln *mapAlnNew(struct psl *psl, int id)
/* construct a new mapAln object */
{
struct mapAln *mapAln;
AllocVar(mapAln);
mapAln->psl = psl;
mapAln->id = id;
return mapAln;
}

/* macro to swap to variables */
#define swapVars(a, b, tmp) ((tmp) = (a), (a) = (b), (b) = (tmp))

static void swapPsl(struct psl *psl)
/* swap query and target in psl */
{
int i, itmp;
unsigned utmp;
char ctmp, *stmp; 
if (psl->strand[1] == '\0')
    psl->strand[1] = '+';  /* explict strand */
swapVars(psl->strand[0], psl->strand[1], ctmp);
swapVars(psl->qName, psl->tName, stmp);
swapVars(psl->qSize, psl->tSize, utmp);
swapVars(psl->qStart, psl->tStart, itmp);
swapVars(psl->qEnd, psl->tEnd, itmp);
for (i = 0; i < psl->blockCount; i++)
    swapVars(psl->qStarts[i], psl->tStarts[i], utmp);
}

static struct mapAln *chainToPsl(struct chain *ch)
/* convert a chain to a psl, ignoring match counts, etc */
{
struct psl *psl;
struct cBlock *cBlk;
int iBlk;
char strand[2];
strand[0] = ch->qStrand;
strand[1] = '\0';

psl = pslNew(ch->qName, ch->qSize, ch->qStart, ch->qEnd,
             ch->tName, ch->tSize, ch->tStart, ch->tEnd,
             strand, slCount(ch->blockList), 0);
for (cBlk = ch->blockList, iBlk = 0; cBlk != NULL; cBlk = cBlk->next, iBlk++)
    {
    psl->blockSizes[iBlk] = (cBlk->tEnd - cBlk->tStart);
    psl->qStarts[iBlk] = cBlk->qStart;
    psl->tStarts[iBlk] = cBlk->tStart;
    psl->match += psl->blockSizes[iBlk];
    }
psl->blockCount = iBlk;
if (swapMap)
    swapPsl(psl);
return mapAlnNew(psl, ch->id);
}

static struct hash* loadMapChains(char *chainFile)
/* read a chain file, convert to mapAln object and hash by query, linking multiple PSLs
 * for the same query.*/
{
struct hash* mapAlns = hashNew(20);
struct chain *ch;
struct lineFile *chLf = lineFileOpen(chainFile, TRUE);
struct hashEl *hel;
while ((ch = chainRead(chLf)) != NULL)
    {
    struct mapAln *mapAln = chainToPsl(ch);
    hel = hashStore(mapAlns, mapAln->psl->qName);
    slAddHead((struct mapAln**)&hel->val, mapAln);
    chainFree(&ch);
    }
lineFileClose(&chLf);
return mapAlns;
}

static struct hash* loadMapPsls(char *pslFile)
/* read a psl file and hash by query, linking multiple PSLs for the
 * same query.*/
{
struct hash* mapAlns = hashNew(20);
int id = 0;
struct psl* psl;
struct lineFile *pslLf = pslFileOpen(pslFile);
struct hashEl *hel;
while ((psl = pslNext(pslLf)) != NULL)
    {
    if (swapMap)
        swapPsl(psl);
    hel = hashStore(mapAlns, psl->qName);
    slSafeAddHead((struct mapAln**)&hel->val, mapAlnNew(psl, id));
    id++;
    }
lineFileClose(&pslLf);
return mapAlns;
}

static int pslAlignedBases(struct psl *psl)
/* get number of aligned bases in a psl.  Need because we can't set the stats
 * in the mapped psl */
{
int i, cnt = 0;
for (i = 0; i < psl->blockCount; i++)
    cnt += psl->blockSizes[i];
return cnt;
}

static void writeMapInfo(FILE* fh, struct psl *inPsl, struct mapAln *mapAln, 
                         struct psl* mappedPsl)
/* write mapInfo row */
{
/* srcQName srcQStart srcQEnd srcQSize */
fprintf(fh, "%s\t%d\t%d\t%d\t",
        inPsl->qName, inPsl->qStart, inPsl->qEnd, inPsl->qSize);
/* srcTName srcTStart srcTEnd srcStrand srcAligned */
fprintf(fh, "%s\t%d\t%d\t%s\t%d\t",
        inPsl->tName, inPsl->tStart, inPsl->tEnd, inPsl->strand,
        pslAlignedBases(inPsl));
/* mappingQName mappingQStart mappingQEnd */
fprintf(fh, "%s\t%d\t%d\t",
        mapAln->psl->qName, mapAln->psl->qStart, mapAln->psl->qEnd);
/* mappingTName mappingTStart mappingTEnd mappingStrand mappingId */
fprintf(fh, "%s\t%d\t%d\t%s\t%d\t",
        mapAln->psl->tName, mapAln->psl->tStart, mapAln->psl->tEnd,
        mapAln->psl->strand, mapAln->id);
/* mappedQName mappedQStart mappedQEnd mappedTName mappedTStart mappedTEnd mappedStrand */
fprintf(fh, "%s\t%d\t%d\t%s\t%d\t%d\t%s\t",
        mappedPsl->qName, mappedPsl->qStart, mappedPsl->qEnd, 
        mappedPsl->tName, mappedPsl->tStart, mappedPsl->tEnd,
        mappedPsl->strand);
/* mappedAligned qStartTrunc qEndTrunc */
fprintf(fh, "%d\t%d\t%d\n",
        pslAlignedBases(mappedPsl), 
        (mappedPsl->qStart-inPsl->qStart), (inPsl->qEnd-mappedPsl->qEnd));
}

static void addQNameSuffix(struct psl *psl)
/* add the suffix to the mapped psl qName */
{
char *oldQName = psl->qName;
char *newQName = needMem(strlen(psl->qName)+strlen(suffix)+1);
strcpy(newQName, oldQName);
strcat(newQName, suffix);
psl->qName = newQName;
freeMem(oldQName);
}

static void mapPslPair(struct psl *inPsl, struct mapAln *mapAln,
                       FILE* outPslFh, FILE *mapInfoFh)
/* map one pair of query and target PSL */
{
struct psl* mappedPsl;
if (inPsl->tSize != mapAln->psl->qSize)
    {
    fprintf(stderr, "Non-fatal error: inPsl %s tSize (%d) != mapPsl %s qSize (%d)\n",
            inPsl->tName, inPsl->tSize, mapAln->psl->qName, mapAln->psl->qSize);
    errCount++;
    return;
    }

mappedPsl = pslTransMap(mapOpts, inPsl, mapAln->psl);

/* only output if blocks were actually mapped */
if (mappedPsl != NULL)
    {
    if (suffix != NULL)
        addQNameSuffix(mappedPsl);
    pslTabOut(mappedPsl, outPslFh);
    if (mapInfoFh != NULL)
        writeMapInfo(mapInfoFh, inPsl, mapAln, mappedPsl);
    }
pslFree(&mappedPsl);
}

static void mapQueryPsl(struct psl* inPsl, struct hash *mapAlns,
                        FILE* outPslFh, FILE *mapInfoFh)
/* map a query psl to all targets  */
{
struct mapAln *mapAln;

for (mapAln = hashFindVal(mapAlns, inPsl->tName); mapAln != NULL; mapAln = mapAln->next)
    mapPslPair(inPsl, mapAln, outPslFh, mapInfoFh);
}

static void pslMap(char* inPslFile, char *mapFile, char *outPslFile)
/* map SNPs to proteins */
{
struct hash *mapAlns;
struct psl* inPsl;
struct lineFile* inPslLf = pslFileOpen(inPslFile);
FILE *outPslFh, *mapInfoFh = NULL;

if (chainMapFile)
    mapAlns = loadMapChains(mapFile);
else
    mapAlns = loadMapPsls(mapFile);

outPslFh = mustOpen(outPslFile, "w");
if (mapInfoFile != NULL)
    {
    mapInfoFh = mustOpen(mapInfoFile, "w");
    fputs(mapInfoHdr, mapInfoFh);
    }
while ((inPsl = pslNext(inPslLf)) != NULL)
    {
    mapQueryPsl(inPsl, mapAlns, outPslFh, mapInfoFh);
    pslFree(&inPsl);
    }
carefulClose(&mapInfoFh);
carefulClose(&outPslFh);
lineFileClose(&inPslLf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("Error: wrong number of arguments");
suffix = optionVal("suffix", NULL);
if (optionExists("keepTranslated"))
    mapOpts |= pslTransMapKeepTrans;
chainMapFile = optionExists("chainMapFile");
swapMap = optionExists("swapMap");
mapInfoFile = optionVal("mapInfo", NULL);
pslMap(argv[1], argv[2], argv[3]);

if (errCount > 0)
    {
    fprintf(stderr, "Error: %d non-fatal errors\n", errCount);
    return 1;
    }
else
    return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

