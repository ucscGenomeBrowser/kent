/* ooMapMend - Refine map by looking at clone sequence overlaps.. 
 * Really just an experiment destined to be integrated back into 
 * ooGreedy. */

struct pslRef
/* A reference to a psl. */
    {
    struct pslRef *next;
    struct psl *psl;
    };

#include "../inc/ooGreedySub.inc"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ooMapMend - Refine map by looking at clone sequence overlaps.\n"
  "usage:\n"
  "   ooMapMend contigDir\n");
}

void ocpLoad(char *fileName, struct hash *cloneHash, struct hash *ocpHash, struct overlappingClonePair **retOcpList)
/* Create ocp structures based on tab separated file, and load into
 * ocpHash. */
{
struct overlappingClonePair *ocp, *ocpList = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[3];
char *ocpName;

while (lineFileRow(lf, words))
    {
    AllocVar(ocp);
    slAddHead(&ocpList, ocp);
    ocp->a = hashMustFindVal(cloneHash, words[0]);
    ocp->b = hashMustFindVal(cloneHash, words[1]);
    ocp->overlap = atoi(words[2]);
    hashAdd(ocpHash, ocpHashName(words[0], words[1]), ocp);
    }
lineFileClose(&lf);
slReverse(&ocpList);
*retOcpList = ocpList;

printf("Loaded %d clone overlaps in %s\n", slCount(ocpList), fileName);
}

struct dlList *makeInitialBels(struct oogClone *cloneList)
/* Create a bel for each clone - linking it to clone and putting
 * it on doubly linked list. */
{
struct oogClone *clone;
struct dlList *list = newDlList();
struct bargeEl *bel;

for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    AllocVar(bel);
    bel->clone = clone;
    clone->bel = bel;
    bel->dlNode = dlAddValTail(list, bel);
    }
return list;
}

struct cloneEnd *newCloneEnd(char *cloneVer, char *fragId, char *side, struct hash *fragHash)
/* Allocate and fill in a new clone end structure. */
{
char fragName[64];
struct cloneEnd *ce;
AllocVar(ce);
sprintf(fragName, "%s_%s", cloneVer, fragId);
ce->frag = hashMustFindVal(fragHash, fragName);
ce->side = side[0];
return ce;
}

void loadEndsFile(char *fileName, struct hash *cloneHash, struct hash *fragHash)
/* Load in end info from file into clone->bel->xEnd, etc */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[8];
int wordCount, singleCount = 0, doubleCount = 0;
char cloneVer[64];
struct oogClone *clone;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    if (wordCount != 5 && wordCount != 7)
        errAbort("Expecting 5 or 7 words line %d of %s", lf->lineIx, lf->fileName);
    clone = hashMustFindVal(cloneHash, words[0]);
    sprintf(cloneVer, "%s%s", words[0], words[1]);
    clone->xEnd = newCloneEnd(cloneVer, words[3], words[4], fragHash);
    if (wordCount > 5)
        {
	clone->yEnd = newCloneEnd(cloneVer, words[5], words[6], fragHash);
	++doubleCount;
	}
    else
        ++singleCount;
    }
lineFileClose(&lf);
printf("Loaded %d single ends, %d double ends from %s\n", singleCount, doubleCount, fileName);
}

#ifdef OLD
struct barge *makeBargeList(struct oogClone *cloneList, struct hash *cloneHash,
	struct overlappingClonePair *ocpList, struct hash *ocpHash,
	char *endsFile)
/* Create a list of barges out of connected fragments, but don't yet order
 * clones inside barges. */
{
struct dlList *orderedList = newDlList();
struct dlList *enclosedList = newDlList();
struct dlList *indyList = makeInitialBels(cloneList);
struct dlList *outcastList = newDlList();
struct bargeEl *bel;
struct oogClone *clone;
struct dlNode *aNode, *bNode, *nextNode;
struct barge *bargeList = NULL, *barge;


/* Move clones that are enclosed into enclosed list. */
markEnclosedClones(ocpList);
for (aNode = indyList->head; aNode->next != NULL; aNode = nextNode)
    {
    nextNode = aNode->next;
    bel = aNode->val;
    if (bel->enclosed)
	{
	dlRemove(aNode);
	dlAddTail(enclosedList, aNode);
	}
    }

/* Get an ordered section and build a barge on it. */
while (findOrderedSection(indyList, orderedList, outcastList, ocpHash))
    {
    fprintf(logFile, " %d indy %d ordered %d outcast\n", dlCount(indyList), 
	dlCount(orderedList), dlCount(outcastList));
    barge = bargeFromListOfBels(orderedList, ocpHash);
    slAddHead(&bargeList, barge);
    freeDlList(&orderedList);
    orderedList = newDlList();
    outcastsToEnclosed(outcastList, enclosedList, 0.80);
    dlCat(indyList, outcastList);
    }
mergeEnclosed(enclosedList);
for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    slSort(&barge->cloneList, cmpBargeElOffset);
    if (bargeLooksReversed(barge))
	flipBarge(barge);
    }
freeDlList(&orderedList);
freeDlList(&indyList);
freeDlList(&enclosedList);
freeDlList(&outcastList);

slReverse(&bargeList);
return bargeList;
}
#endif /* OLD */

void addPslRef(struct pslRef **pList, struct psl *psl)
/* Allocate a pslRef and add it to list. */
{
struct pslRef *ref;
AllocVar(ref);
ref->psl = psl;
slAddHead(pList, ref);
}

void directToUlrHash(struct cloneEnd *ce, struct hash *uEndHash, struct hash *lEndHash, struct hash *rEndHash)
/* Put ce onto appropriate hash depending on ce->side. */
{
char *fragName;

if (ce != NULL)
    {
    fragName = ce->frag->name;
    if (ce->side == 'L')
	hashAdd(lEndHash, fragName, ce);
    else if (ce->side == 'R')
	hashAdd(rEndHash, fragName, ce);
    else
	hashAdd(uEndHash, fragName, ce);
    }
}

enum {endSeqSize = 1000};

int passPslThroughUlrHashes(struct psl *psl, struct hash *uEndHash, struct hash *lEndHash, struct hash *rEndHash, 
    char *fragName, int fragSize, int start, int end)
/* Put psl associated with fragment on cloneEnd via appropriate hashes. 
 * Returns count of refs added. */
{
struct cloneEnd *ce;
boolean gotU = FALSE;
int count = 0;

if (rangeIntersection(start, end, 0, endSeqSize))
    {
    if ((ce = hashFindVal(lEndHash, fragName)) != NULL)
	{
	addPslRef(&ce->pslRefList, psl);
	++count;
	}
    if ((ce = hashFindVal(uEndHash, fragName)) != NULL)
	{
	addPslRef(&ce->pslRefList, psl);
	++count;
	gotU = TRUE;
	}
    }
if (rangeIntersection(start, end, fragSize-endSeqSize, fragSize))
    {
    if ((ce = hashFindVal(rEndHash, fragName)) != NULL)
	{
	addPslRef(&ce->pslRefList, psl);
	++count;
	}
    if ((ce = hashFindVal(uEndHash, fragName)) != NULL)
	{
	if (!gotU)
	    {
	    addPslRef(&ce->pslRefList, psl);
	    ++count;
	    }
	}
    }
return count;
}

void addPslRefsToEnds(struct oogClone *cloneList, struct psl *selfPslList, struct hash *fragHash)
/* Put alignments that involve clone ends onto bel's for handy
 * use. */
{
struct hash *uEndHash = newHash(0);     /* Unknown end alignments. */
struct hash *lEndHash = newHash(0);	/* Left end alignments. */
struct hash *rEndHash = newHash(0);     /* Right end alignments. */
struct oogClone *clone;
struct oogFrag *qFrag, *tFrag;
struct psl *psl;
int pslRefCount = 0;

/* Make up a hashes to access end fragments. */
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    directToUlrHash(clone->xEnd, uEndHash, lEndHash, rEndHash);
    directToUlrHash(clone->yEnd, uEndHash, lEndHash, rEndHash);
    }

/* Loop through and put end fragment alignments on the appropriate
 * list:
 *     L - for first endSeqSize bases.
 *     R - for last endSeqSize bases.
 *     U - for first or last endSeqSize bases. */
for (psl = selfPslList; psl != NULL; psl = psl->next)
    {
    /* Only consider cases where psl's are aligning to separate clones. */
    qFrag = hashMustFindVal(fragHash, psl->qName);
    tFrag = hashMustFindVal(fragHash, psl->tName);
    if (qFrag->clone == tFrag->clone)
        continue;

    pslRefCount += passPslThroughUlrHashes(psl, uEndHash, lEndHash, rEndHash, 
    	psl->qName, psl->qSize, psl->qStart, psl->qEnd);
    pslRefCount += passPslThroughUlrHashes(psl, uEndHash, lEndHash, rEndHash, 
    	psl->tName, psl->tSize, psl->tStart, psl->tEnd);
    }

printf("Found %d psls (of %d) that hit clone ends\n", pslRefCount, slCount(selfPslList));
freeHash(&uEndHash);
freeHash(&lEndHash);
freeHash(&rEndHash);
}

struct barge *makeBarges(struct dlList *belList, struct hash *ocpHash)
/* Create a list of barges out of connected fragments, but don't yet order
 * clones inside barges. */
{
}

void ooMapMend(char *contigDir)
/* ooMapMend - Refine map by looking at clone sequence overlaps.. */
{
char fileName[512], genoListName[512];
struct hash *cloneHash = newHash(0);
struct oogClone *cloneList = NULL, *clone;
struct hash *fragHash = newHash(16);
struct oogFrag *fragList = NULL, *frag;
struct hash *ocpHash = newHash(0);
struct overlappingClonePair *ocpList = NULL, *ocp;
struct hash *ntContigHash = newHash(3);	/* Deliberately kept empty. */
char *ctgName = NULL, *infoType = NULL;
struct barge *bargeList = NULL;
struct dlList *belList = NULL;
struct psl *selfPslList = NULL;

/* Set up logging of error and status messages. */
sprintf(fileName, "%s/ooMapMend.log", contigDir);
logFile = mustOpen(fileName, "w");
pushWarnHandler(warnHandler);

/* Load in clones, fragments, and preliminary map positions. */
sprintf(fileName, "%s/info", contigDir);
sprintf(genoListName, "%s/geno.lst", contigDir);
printf("Loading clones and original map...\n");
loadAllClones(fileName, genoListName, fragHash, cloneHash, ntContigHash,
    &fragList, &cloneList, &ctgName, &infoType);
printf("Loaded %d clones in %s (type %s)\n", slCount(cloneList), ctgName, infoType);

/* Load up list of alignments of frags against each other. */
sprintf(fileName, "%s/self.psl", contigDir);
selfPslList = pslLoadAll(fileName);
printf("Loaded %d alignments from %s\n", slCount(selfPslList), fileName);

/* Load up file of overlapping pairs and create around it for fast access. */
sprintf(fileName, "%s/ocp.87", contigDir);
ocpLoad(fileName, cloneHash, ocpHash, &ocpList);

/* Set up a wrapper 'bargeEl' structure that will help us clump together clones
 * into barges. */
belList = makeInitialBels(cloneList);
printf("Made %d bels\n", dlCount(belList));

/* Load up information about which fragments are ends of clones, and
 * which alignments hit clone ends. */
sprintf(fileName, "%s/cloneEnds", contigDir);
loadEndsFile(fileName, cloneHash, fragHash);
addPslRefsToEnds(cloneList, selfPslList, fragHash);

bargeList = makeBarges(belList, ocpHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 2)
    usage();
dnaUtilOpen();
ooMapMend(argv[1]);
return 0;
}
