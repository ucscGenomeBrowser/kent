/* chainJoin - join chains that continue one another in the same order and orientation. */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"

int clMaxGap = 100;
char *clGuide = NULL;
int clMaxTrim = 1000;

void usage()
/* Explain usage and exit. */
{
errAbort(
"chainJoin - join chains that continue one another in the same order and orientation\n"
"usage:\n"
"   chainJoin in.chain out.chain\n"
"The input may be compressed.  Two chains are joined when they are on the same\n"
"target and query sequences and the same query strand, and the second chain\n"
"continues the first:\n"
"   - it starts at or after the end of the first chain on both assemblies, with a\n"
"     gap of at most maxGap bases on each.  The gaps may differ, which leaves an\n"
"     indel in the joined chain.\n"
"   - or its first block overlaps the last block of the first chain on the same\n"
"     diagonal.  The overlap must lie inside those two blocks, and they are merged\n"
"     into one block, so no other block is changed.\n"
"Joining is repeated until no more joins are possible.  A chain is never split.\n"
"When more than one chain could follow, the nearest on the target is joined, then\n"
"the nearest on the query, then the highest scoring.\n"
"The joined chain keeps the ID of the chain that starts first, and its score is\n"
"the sum of the scores.  Chains are written in the order they were read.\n"
"options:\n"
"   -maxGap=N - largest gap on either assembly between the two chains, default %d\n"
"   The -guide and -maxTrim options are experimental.  The guide is made by running\n"
"   axtChain again on the short chains; see Redmine #38835.\n"
"   -guide=guide.chain - before the joins above, join chains in the order that the\n"
"     chains in guide.chain hold them.  guide.chain is made by chaining the blocks\n"
"     of in.chain again, for example with chainToPsl and axtChain.  Leave the long\n"
"     chains (for example over 1 Mb on the target) out of it, so that their blocks\n"
"     cannot take the links between the short chains again.  Where two joined\n"
"     chains overlap, they are cut where the guide crosses from one to the other.\n"
"     The cut-off blocks are kept as chains of their own, so no aligned base is\n"
"     lost.  There is no limit on the gap between chains joined this way.\n"
"   -maxTrim=N - with -guide, join two chains only if the cut removes at most N\n"
"     aligned bases from each of them, default %d\n"
, clMaxGap, clMaxTrim);
}

static struct optionSpec options[] = {
    {"maxGap", OPTION_INT},
    {"guide", OPTION_STRING},
    {"maxTrim", OPTION_INT},
    {NULL, 0},
};

struct joinChain
/* A chain with the bookkeeping needed to join it. */
    {
    struct chain *chain;	/* The chain itself. */
    struct cBlock *lastBlock;	/* Last block of the chain. */
    boolean absorbed;		/* TRUE once joined onto another chain. */
    int inputIx;		/* Position in the input, to break ties when sorting. */
    struct joinChain *succ;	/* Chain the guide puts next, if any. */
    int succCut;		/* Target position to cut our tail at before joining succ. */
    int predCut;		/* Target position to cut our head at before joining onto pred. */
    struct joinChain *succProposed, *predProposed;	/* First partner the guide proposed. */
    boolean succMulti, predMulti;	/* TRUE if the guide proposed more than one partner. */
    boolean hasPred;		/* TRUE if a guide join onto another chain was accepted. */
    int guideStamp;		/* Last guide chain that this chain was seen in. */
    int guideRuns;		/* Number of separate stretches of that guide chain on us. */
    };

int joinChainCmpTStart(const void *va, const void *vb)
/* Compare to sort by target start. */
{
const struct joinChain *a = *((struct joinChain **)va);
const struct joinChain *b = *((struct joinChain **)vb);
int diff = a->chain->tStart - b->chain->tStart;
if (diff == 0)
    diff = a->inputIx - b->inputIx;
return diff;
}

struct chainGroup
/* All chains on one target, query and query strand, sorted by target start. */
    {
    struct chainGroup *next;
    struct joinChain **array;	/* Chains sorted by target start. */
    int count;			/* Number of chains. */
    int alloced;		/* Allocated size of array. */
    struct oldBlock *blocks;	/* With -guide, every block of the group sorted by tStart. */
    int blockCount;		/* Number of blocks. */
    int maxBlockSize;		/* Size of the largest block. */
    };

struct oldBlock
/* A block of an input chain, with the chain it belongs to. */
    {
    struct cBlock *block;
    struct joinChain *owner;
    };

int diagonal(struct cBlock *b)
/* Return the diagonal a block is on. */
{
return b->tStart - b->qStart;
}

int firstAtOrAfter(struct chainGroup *group, int tStart)
/* Return the index of the first chain in the group that starts at or after tStart. */
{
int lo = 0, hi = group->count;
while (lo < hi)
    {
    int mid = (lo + hi) / 2;
    if (group->array[mid]->chain->tStart < tStart)
        lo = mid + 1;
    else
        hi = mid;
    }
return lo;
}

boolean canFollow(struct joinChain *a, struct joinChain *b)
/* Return TRUE if chain b can be joined onto the end of chain a. */
{
struct cBlock *last = a->lastBlock;
struct cBlock *first = b->chain->blockList;
int tGap = first->tStart - last->tEnd;
int qGap = first->qStart - last->qEnd;
if (tGap > clMaxGap || qGap > clMaxGap)
    return FALSE;
if (tGap >= 0 && qGap >= 0)
    return TRUE;
/* An overlap is only allowed on one diagonal, inside the last block of a and the first
 * block of b, so b must start within that block and reach past the end of a. */
if (diagonal(first) != diagonal(last))
    return FALSE;
if (first->tStart < last->tStart)
    return FALSE;
if (first->tEnd < last->tEnd)
    return FALSE;
return TRUE;
}

int exactJoins = 0, gapJoins = 0, indelJoins = 0, overlapJoins = 0, choiceCount = 0;
int guideJoins = 0, guideRejected = 0, guideConflicts = 0, guideTrimmed = 0, guideSkipped = 0;

void joinOnto(struct joinChain *a, struct joinChain *b, boolean byGuide)
/* Move the blocks of chain b onto the end of chain a, and mark b absorbed.
 * b must not start before the end of a on either assembly, unless the two
 * blocks that meet are on one diagonal. */
{
struct chain *ac = a->chain, *bc = b->chain;
struct cBlock *last = a->lastBlock;
struct cBlock *first = bc->blockList;
int tGap = first->tStart - last->tEnd;
int qGap = first->qStart - last->qEnd;
if (tGap == qGap && tGap <= 0)
    {
    /* The two blocks are on one diagonal and touch or overlap: make them one. */
    last->tEnd = first->tEnd;
    last->qEnd = first->qEnd;
    last->score += first->score;
    last->next = first->next;
    freeMem(first);
    if (byGuide)
        guideJoins += 1;
    else if (tGap == 0)
        exactJoins += 1;
    else
        overlapJoins += 1;
    }
else
    {
    /* A gap, the same size on both assemblies or not, between the two blocks. */
    last->next = first;
    if (byGuide)
        guideJoins += 1;
    else if (tGap == qGap)
        gapJoins += 1;
    else
        indelJoins += 1;
    }
a->lastBlock = (last->next == NULL ? last : b->lastBlock);
ac->tEnd = bc->tEnd;
ac->qEnd = bc->qEnd;
ac->score += bc->score;
bc->blockList = NULL;
b->absorbed = TRUE;
verbose(2, "joined chain %d onto chain %d, target gap %d, query gap %d\n",
    bc->id, ac->id, tGap, qGap);
}

struct joinChain *bestFollower(struct chainGroup *group, struct joinChain *a)
/* Return the nearest chain on the target that can be joined onto the end of a, or NULL.
 * Ties go to the nearest on the query, then to the highest score. */
{
struct joinChain *best = NULL;
int candidates = 0;
int i;
for (i = firstAtOrAfter(group, a->lastBlock->tStart); i < group->count; ++i)
    {
    struct joinChain *b = group->array[i];
    if (b->chain->tStart > a->chain->tEnd + clMaxGap)
        break;
    if (b == a || b->absorbed || !canFollow(a, b))
        continue;
    candidates += 1;
    /* Take the nearest, so a join never steps over a chain that could follow. */
    struct chain *bc = b->chain;
    if (best == NULL || bc->tStart < best->chain->tStart
        || (bc->tStart == best->chain->tStart
            && (bc->qStart < best->chain->qStart
                || (bc->qStart == best->chain->qStart && bc->score > best->chain->score))))
        best = b;
    }
if (candidates > 1)
    choiceCount += 1;
return best;
}

int oldBlockCmp(const void *va, const void *vb)
/* Compare to sort old blocks by target start. */
{
const struct oldBlock *a = va, *b = vb;
return a->block->tStart - b->block->tStart;
}

void indexBlocks(struct chainGroup *group)
/* Make the sorted array of all blocks in the group. */
{
int i, count = 0;
for (i = 0; i < group->count; ++i)
    count += slCount(group->array[i]->chain->blockList);
AllocArray(group->blocks, count);
for (i = 0; i < group->count; ++i)
    {
    struct joinChain *jc = group->array[i];
    struct cBlock *b;
    for (b = jc->chain->blockList; b != NULL; b = b->next)
        {
        struct oldBlock *ob = &group->blocks[group->blockCount++];
        ob->block = b;
        ob->owner = jc;
        group->maxBlockSize = max(group->maxBlockSize, b->tEnd - b->tStart);
        }
    }
qsort(group->blocks, group->blockCount, sizeof(group->blocks[0]), oldBlockCmp);
}

int firstBlockAtOrAfter(struct chainGroup *group, int tStart)
/* Return the index of the first block in the group that starts at or after tStart. */
{
int lo = 0, hi = group->blockCount;
while (lo < hi)
    {
    int mid = (lo + hi) / 2;
    if (group->blocks[mid].block->tStart < tStart)
        lo = mid + 1;
    else
        hi = mid;
    }
return lo;
}

int basesFrom(struct chain *chain, int tCut)
/* Return the number of aligned bases in the chain at or after tCut on the target. */
{
int total = 0;
struct cBlock *b;
for (b = chain->blockList; b != NULL; b = b->next)
    if (b->tEnd > tCut)
        total += b->tEnd - max(b->tStart, tCut);
return total;
}

int chainBases(struct chain *chain)
/* Return the number of aligned bases in the chain. */
{
return basesFrom(chain, chain->tStart);
}

struct guideRun
/* A stretch of a guide chain that lies on the blocks of one input chain. */
    {
    struct joinChain *owner;
    int tStart, tEnd;		/* Target range of the stretch. */
    };

struct guideProposal
/* A join that the guide proposes. */
    {
    struct guideProposal *next;
    struct joinChain *a, *b;	/* Join b onto the end of a. */
    int aCut, bCut;		/* Cut a's tail at aCut and b's head at bCut first. */
    };

void readGuide(char *guideName, struct hash *groupHash, struct guideProposal **pList)
/* Read the guide chains and add the joins that they propose to *pList. */
{
struct lineFile *lf = lineFileOpen(guideName, TRUE);
struct chain *guide;
int guideIx = 0, runAlloced = 16;
struct guideRun *runs;
AllocArray(runs, runAlloced);
while ((guide = chainRead(lf)) != NULL)
    {
    ++guideIx;
    char key[512];
    safef(key, sizeof(key), "%s\t%s\t%c", guide->tName, guide->qName, guide->qStrand);
    struct chainGroup *group = hashFindVal(groupHash, key);
    if (group == NULL)
        {
        chainFree(&guide);
        continue;
        }
    /* Split the guide into stretches that lie on the blocks of one input chain each. */
    int runCount = 0;
    struct cBlock *gb;
    for (gb = guide->blockList; gb != NULL; gb = gb->next)
        {
        int i;
        for (i = firstBlockAtOrAfter(group, gb->tStart - group->maxBlockSize);
             i < group->blockCount; ++i)
            {
            struct oldBlock *ob = &group->blocks[i];
            struct cBlock *b = ob->block;
            if (b->tStart >= gb->tEnd)
                break;
            int start = max(b->tStart, gb->tStart), end = min(b->tEnd, gb->tEnd);
            if (start >= end || diagonal(b) != diagonal(gb))
                continue;
            struct joinChain *owner = ob->owner;
            if (runCount > 0 && runs[runCount-1].owner == owner)
                runs[runCount-1].tEnd = end;
            else
                {
                if (owner->guideStamp != guideIx)
                    {
                    owner->guideStamp = guideIx;
                    owner->guideRuns = 0;
                    }
                owner->guideRuns += 1;
                if (runCount == runAlloced)
                    {
                    ExpandArray(runs, runAlloced, 2 * runAlloced);
                    runAlloced *= 2;
                    }
                runs[runCount].owner = owner;
                runs[runCount].tStart = start;
                runs[runCount].tEnd = end;
                ++runCount;
                }
            }
        }
    /* Where the guide comes back to a chain it left, it does not hold the input
     * chains in order, so make no join next to that chain. */
    int i;
    for (i = 1; i < runCount; ++i)
        {
        if (runs[i-1].owner->guideRuns > 1 || runs[i].owner->guideRuns > 1
            || runs[i].tStart < runs[i-1].tEnd)
            guideSkipped += 1;
        else
            {
            struct guideProposal *gp;
            AllocVar(gp);
            gp->a = runs[i-1].owner;
            gp->aCut = runs[i-1].tEnd;
            gp->b = runs[i].owner;
            gp->bCut = runs[i].tStart;
            slAddHead(pList, gp);
            }
        }
    chainFree(&guide);
    }
freeMem(runs);
lineFileClose(&lf);
}

int nextId = 0;
struct chain *leftoverList = NULL;

struct chain *leftoverChain(struct chain *orig, struct cBlock *blockList, int origBases)
/* Make a chain of the blocks cut off orig, and give it a share of orig's score. */
{
struct chain *chain;
AllocVar(chain);
chain->blockList = blockList;
chain->tName = cloneString(orig->tName);
chain->tSize = orig->tSize;
chain->qName = cloneString(orig->qName);
chain->qSize = orig->qSize;
chain->qStrand = orig->qStrand;
struct cBlock *last = slLastEl(blockList);
chain->tStart = blockList->tStart;
chain->qStart = blockList->qStart;
chain->tEnd = last->tEnd;
chain->qEnd = last->qEnd;
chain->id = ++nextId;
chain->score = orig->score * chainBases(chain) / origBases;
orig->score -= chain->score;
slAddHead(&leftoverList, chain);
return chain;
}

struct cBlock *splitBlock(struct cBlock *b, int tCut)
/* Split block b at tCut on the target, and return the new second half. */
{
struct cBlock *nb;
AllocVar(nb);
nb->tStart = tCut;
nb->qStart = b->qStart + (tCut - b->tStart);
nb->tEnd = b->tEnd;
nb->qEnd = b->qEnd;
nb->next = b->next;
b->tEnd = nb->tStart;
b->qEnd = nb->qStart;
b->next = nb;
return nb;
}

void cutTail(struct joinChain *jc, int tCut)
/* Cut off the blocks of jc at or after tCut and keep them as a chain of their own. */
{
struct chain *chain = jc->chain;
if (tCut >= chain->tEnd)
    return;
int origBases = chainBases(chain);
struct cBlock *b, *prev = NULL;
for (b = chain->blockList; b != NULL; prev = b, b = b->next)
    if (b->tEnd > tCut)
        break;
if (b->tStart < tCut)
    {
    prev = b;
    b = splitBlock(b, tCut);
    }
if (prev == NULL)
    errAbort("guide cut would remove all of chain %d", chain->id);
prev->next = NULL;
jc->lastBlock = prev;
chain->tEnd = prev->tEnd;
chain->qEnd = prev->qEnd;
leftoverChain(chain, b, origBases);
guideTrimmed += 1;
}

void cutHead(struct joinChain *jc, int tCut)
/* Cut off the blocks of jc before tCut and keep them as a chain of their own. */
{
struct chain *chain = jc->chain;
if (tCut <= chain->tStart)
    return;
int origBases = chainBases(chain);
struct cBlock *b, *prev = NULL;
for (b = chain->blockList; b != NULL; prev = b, b = b->next)
    if (b->tEnd > tCut)
        break;
if (b == NULL)
    errAbort("guide cut would remove all of chain %d", chain->id);
if (b->tStart < tCut)
    {
    prev = b;
    b = splitBlock(b, tCut);
    }
struct cBlock *cutList = chain->blockList;
if (jc->lastBlock == prev)
    jc->lastBlock = b;	/* The last block was split, and b is its second half. */
prev->next = NULL;
chain->blockList = b;
chain->tStart = b->tStart;
chain->qStart = b->qStart;
leftoverChain(chain, cutList, origBases);
guideTrimmed += 1;
}

void guideJoin(char *guideName, struct hash *groupHash, struct chainGroup *groupList)
/* Join the chains that the guide holds in order. */
{
struct chainGroup *group;
for (group = groupList; group != NULL; group = group->next)
    indexBlocks(group);
struct guideProposal *gp, *gpList = NULL;
readGuide(guideName, groupHash, &gpList);

/* A chain may follow, and be followed by, only one chain. */
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    struct joinChain *a = gp->a, *b = gp->b;
    if (a->succProposed == NULL)
        a->succProposed = b;
    else if (a->succProposed != b)
        a->succMulti = TRUE;
    if (b->predProposed == NULL)
        b->predProposed = a;
    else if (b->predProposed != a)
        b->predMulti = TRUE;
    }
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    struct joinChain *a = gp->a, *b = gp->b;
    if (a->succ == b)
        continue;	/* Another guide chain proposed the same join. */
    if (a->succMulti || b->predMulti)
        {
        guideConflicts += 1;
        continue;
        }
    if (basesFrom(a->chain, gp->aCut) > clMaxTrim
        || chainBases(b->chain) - basesFrom(b->chain, gp->bCut) > clMaxTrim)
        {
        guideRejected += 1;
        continue;
        }
    a->succ = b;
    a->succCut = gp->aCut;
    b->predCut = gp->bCut;
    b->hasPred = TRUE;
    }
slFreeList(&gpList);

/* Two guide chains may each use a different part of one chain.  Then the cut at its
 * head must still come before the cut at its tail. */
for (group = groupList; group != NULL; group = group->next)
    {
    int i;
    for (i = 0; i < group->count; ++i)
        {
        struct joinChain *jc = group->array[i];
        if (jc->hasPred && jc->succ != NULL && jc->predCut >= jc->succCut)
            {
            jc->succ->hasPred = FALSE;
            jc->succ = NULL;
            guideConflicts += 1;
            }
        }
    }

/* Walk each run of joins from the chain that starts it. */
for (group = groupList; group != NULL; group = group->next)
    {
    int i;
    for (i = 0; i < group->count; ++i)
        {
        struct joinChain *head = group->array[i], *cur;
        if (head->hasPred || head->succ == NULL)
            continue;
        cutTail(head, head->succCut);
        for (cur = head; cur->succ != NULL; cur = cur->succ)
            {
            struct joinChain *b = cur->succ;
            cutHead(b, b->predCut);
            if (b->succ != NULL)
                cutTail(b, b->succCut);
            verbose(2, "guide joins chain %d onto chain %d, target gap %d, query gap %d\n",
                b->chain->id, head->chain->id, b->chain->tStart - head->chain->tEnd,
                b->chain->qStart - head->chain->qEnd);
            joinOnto(head, b, TRUE);
            }
        }
    freez(&group->blocks);
    }
}

void chainJoin(char *inName, char *outName)
/* chainJoin - join chains that continue one another in the same order and orientation. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
struct hash *groupHash = hashNew(0);
struct chainGroup *groupList = NULL;
struct slRef *jcRefList = NULL;
struct chain *chain;
int inCount = 0;
while ((chain = chainRead(lf)) != NULL)
    {
    if (chain->blockList == NULL)
        errAbort("chain %d has no blocks", chain->id);
    struct joinChain *jc;
    AllocVar(jc);
    jc->chain = chain;
    jc->lastBlock = slLastEl(chain->blockList);
    jc->inputIx = inCount;
    nextId = max(nextId, chain->id);
    refAdd(&jcRefList, jc);

    char key[512];
    safef(key, sizeof(key), "%s\t%s\t%c", chain->tName, chain->qName, chain->qStrand);
    struct chainGroup *group = hashFindVal(groupHash, key);
    if (group == NULL)
        {
        AllocVar(group);
        group->alloced = 16;
        AllocArray(group->array, group->alloced);
        hashAdd(groupHash, key, group);
        slAddHead(&groupList, group);
        }
    if (group->count == group->alloced)
        {
        ExpandArray(group->array, group->alloced, 2 * group->alloced);
        group->alloced *= 2;
        }
    group->array[group->count++] = jc;
    inCount += 1;
    }
lineFileClose(&lf);
slReverse(&jcRefList);

struct chainGroup *group;
for (group = groupList; group != NULL; group = group->next)
    qsort(group->array, group->count, sizeof(group->array[0]), joinChainCmpTStart);
if (clGuide != NULL)
    {
    guideJoin(clGuide, groupHash, groupList);
    /* Cuts moved the starts of some chains, so sort again for the joins below. */
    for (group = groupList; group != NULL; group = group->next)
        qsort(group->array, group->count, sizeof(group->array[0]), joinChainCmpTStart);
    }
for (group = groupList; group != NULL; group = group->next)
    {
    int i;
    for (i = 0; i < group->count; ++i)
        {
        struct joinChain *a = group->array[i];
        if (a->absorbed)
            continue;
        struct joinChain *b;
        while ((b = bestFollower(group, a)) != NULL)
            joinOnto(a, b, FALSE);
        }
    }

FILE *f = mustOpen(outName, "w");
int outCount = 0;
struct slRef *ref;
for (ref = jcRefList; ref != NULL; ref = ref->next)
    {
    struct joinChain *jc = ref->val;
    if (jc->absorbed)
        continue;
    chainWrite(jc->chain, f);
    outCount += 1;
    }
slReverse(&leftoverList);
struct chain *left;
for (left = leftoverList; left != NULL; left = left->next)
    {
    chainWrite(left, f);
    outCount += 1;
    }
carefulClose(&f);
verbose(1, "%d chains in, %d chains out: %d exact joins, %d across a gap, %d across an indel, "
    "%d across an overlap\n", inCount, outCount, exactJoins, gapJoins, indelJoins, overlapJoins);
if (clGuide != NULL)
    verbose(1, "guide: %d joins, %d chain ends cut, %d joins refused by -maxTrim, %d refused "
        "because a chain end had more than one partner, %d refused because the guide "
        "returns to one of the two chains or the two overlap on the target\n",
        guideJoins, guideTrimmed, guideRejected, guideConflicts, guideSkipped);
if (choiceCount > 0)
    verbose(1, "%d times more than one chain could follow; the nearest one was used\n",
        choiceCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clMaxGap = optionInt("maxGap", clMaxGap);
clGuide = optionVal("guide", clGuide);
clMaxTrim = optionInt("maxTrim", clMaxTrim);
if (clMaxTrim < 0)
    errAbort("-maxTrim must not be negative");
if (clMaxGap < 0)
    errAbort("-maxGap must not be negative");
chainJoin(argv[1], argv[2]);
return 0;
}
