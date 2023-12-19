/* refGraphTest - Test out some reference genome graph stuff.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "diGraph.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "refGraphTest - Test out some reference genome graph stuff.\n"
  "usage:\n"
  "   refGraphTest nodes.tab edges.psl output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct annoSeq
/* An annotated sequence */
    {
    struct annoSeq *next;
    char *name;	    /* Chromosome or other name */
    int size;	    /* Size of sequence in bases */
    };

struct annoSeq *readChromSizes(char *fileName)
/* Read in chrom.sizes file and return a corresponding list of annoSeq's */
{
struct annoSeq *list = NULL;
struct annoSeq *annoSeq;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
while (lineFileRow(lf, row))
   {
   AllocVar(annoSeq);
   annoSeq->name = cloneString(row[0]);
   annoSeq->size = lineFileNeedNum(lf, row, 1);
   slAddHead(&list, annoSeq);
   }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct dgNode *findOrMakeNode(struct diGraph *g, struct hash *seqHash, char *name)
/* Return node associated with seq */
{
struct dgNode *node = dgFindNode(g, name);
if (node == NULL)
    {
    struct annoSeq *seq = hashFindVal(seqHash, name);
    if (seq == NULL)
        errAbort("Can't find sequence %s", name);
    node = dgAddNode(g, name, seq);
    }
return node;
}

boolean isAltName(char *name)
/* Return true if it's an alt name */
{
return endsWith(name, "_alt");
}

boolean isFixName(char *name)
/* Return true if it's a fix name */
{
return endsWith(name, "_fix");
}

void addAltZeroVal(struct hash *hash, char *name)
/* Add name to hash with NULL val if it isn't in there already, and if
 * it is an alt */
{
if (isAltName(name))
    {
    if (hashLookup(hash, name) == NULL)
        hashAdd(hash, name, NULL);
    }
}
        
void saveBestAlt(struct hash *hash, char *name, int size, int start, int end, struct psl *psl)
/* Save best psl to hash if it's an alt */
{
if (isAltName(name))
    {
    struct hashEl *hel = hashLookup(hash, name);
    if (hel == NULL)
	internalErr();
    struct psl *oldPsl = hel->val;
    if (oldPsl == NULL)
        hel->val = psl;
    else
        {
	verbose(2, "duplicate psl %s: old score %d, new score %d\n", name, pslScore(oldPsl), pslScore(psl));
	if (pslScore(psl) > pslScore(oldPsl))
	    hel->val = psl;
	}
    }
}

void checkAllHaveVal(struct hash *hash, char *hashName)
/* Make sure all elements of hash have values */
{
struct hashEl *hel;
struct hashCookie cookie = hashFirst(hash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (hel->val == NULL)
        errAbort("%s has no full value in %s", hel->name, hashName);
    }
}

void addPslsFromHash(struct hash *hash, struct psl **pList)
/* Add psl values from hash into list */
{
struct hashEl *hel;
struct hashCookie cookie = hashFirst(hash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct psl *psl = hel->val;
    slAddHead(pList, psl);
    }
}

boolean fixedPsl(struct psl *psl)
/* Return TRUE if psl involves a fix */
{
return isFixName(psl->qName) || isFixName(psl->tName);
}

struct psl *pslFilterPartial(struct psl *list)
/* Return a new list with the items that only partially cover the alts removed */
{
/* Make up a hash for all alts, initialized to zero counts */
struct hash *qAltHash = hashNew(0);
struct hash *tAltHash = hashNew(0);
struct psl *psl;

for (psl = list; psl != NULL; psl = psl->next)
    {
    if (!fixedPsl(psl))
	{
	addAltZeroVal(qAltHash, psl->qName);
	addAltZeroVal(tAltHash, psl->tName);
	}
    }
if (qAltHash->elCount != tAltHash->elCount)
    errAbort("Got %d alts in query side, %d in target, they should be the same, aborting.", 
	qAltHash->elCount, tAltHash->elCount);

for (psl = list; psl != NULL; psl = psl->next)
    {
    if (!fixedPsl(psl))
	{
	saveBestAlt(qAltHash, psl->qName, psl->qSize, psl->qStart, psl->qEnd, psl);
	saveBestAlt(tAltHash, psl->tName, psl->tSize, psl->tStart, psl->tEnd, psl);
	}
    }

checkAllHaveVal(qAltHash, "query");
checkAllHaveVal(tAltHash, "target");

struct psl *newList = NULL;
addPslsFromHash(qAltHash, &newList);
addPslsFromHash(tAltHash, &newList);

hashFree(&qAltHash);
hashFree(&tAltHash);
return newList;
}

struct hash *chromsAndPslsToGraphs(struct annoSeq *seqList, struct hash *seqHash, struct psl *pslList)
/* Return hash of diGraphs, one for each chromosome, keyed by chromosome name */
{
/* Make chromosome hash out of sequences with no underbars in name */
struct hash *chromHash = hashNew(0);
struct annoSeq *seq;
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    if (strchr(seq->name, '_') == NULL)
	{
	struct diGraph *g = dgNew();
        hashAdd(chromHash, seq->name, g);
	dgAddNode(g, seq->name, seq);
	}
    }

struct psl *psl;
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    /* Find appropriate graph */
    struct diGraph *g = NULL;
    if (isAltName(psl->qName))
        {
	g = hashFindVal(chromHash, psl->tName);
	if (psl->qStart != 0 || psl->qEnd != psl->qSize)
	    {
	    verbose(2, "%s alignment %d-%d of %d is not full\n", psl->qName, psl->qStart, psl->qEnd, psl->qSize);
	    }
	}
    else
        {
	g = hashFindVal(chromHash, psl->qName);
	if (psl->tStart != 0 || psl->tEnd != psl->tSize)
	    {
	    verbose(2, "%s alignment %d-%d of %d is not full\n", psl->tName, psl->tStart, psl->tEnd, psl->tSize);
	    }
	}
    if (g == NULL)
        errAbort("Neither %s nor %s are chromosomes in psl\n", psl->qName, psl->tName); 

    /* Find nodes associated with query and target */
    struct dgNode *tn = findOrMakeNode(g, seqHash, psl->tName);
    struct dgNode *qn = findOrMakeNode(g, seqHash, psl->qName);

    /* Connect nodes with edge marked with psl. */
    dgConnectWithVal(g, tn, qn, psl);
    }
return chromHash;
}

void refGraphTest(char *nodesFile, char *aliFile, char *outputFile)
/* refGraphTest - Test out some reference genome graph stuff.. */
{
FILE *f = mustOpen(outputFile, "w");

/* Read in annotated sequence list */
struct annoSeq *seq, *seqList = readChromSizes(nodesFile);
struct hash *seqHash = hashNew(0);
for (seq = seqList; seq != NULL; seq = seq->next)
    hashAdd(seqHash, seq->name, seq);

/* Read in psl's */
struct psl *pslList = pslLoadAll(aliFile);
fprintf(f, "# %g raw alt seq alignments in %s\n", slCount(pslList)/2.0, aliFile);

pslList = pslFilterPartial(pslList);
fprintf(f, "# %g filtered seq alignments in %s\n", slCount(pslList)/2.0, aliFile);

struct hash *chromHash = chromsAndPslsToGraphs(seqList, seqHash, pslList);
fprintf(f, "# %s from %s and %s has %d chromosomes\n", outputFile, nodesFile, aliFile, chromHash->elCount);
struct psl *psl;
for (psl = pslList; psl != NULL; psl = psl->next)
     {
     pslTabOut(psl, f);
     }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
refGraphTest(argv[1], argv[2], argv[3]);
return 0;
}
