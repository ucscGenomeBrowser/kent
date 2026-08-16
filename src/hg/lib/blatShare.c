/* blatShare.c - reopen durable BLAT bigPsl results.  Shared by hgBlat (which rebuilds the results
 * table for a shared "?u=&s=" link) and hgc (which rebuilds one base-by-base alignment for a shared
 * alignment link), so both read the durable custom track through the same code and the same
 * server-file security check. */

/* Copyright (C) 2024 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "localmem.h"
#include "psl.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "bigPsl.h"
#include "cart.h"
#include "chromAlias.h"
#include "hgConfig.h"
#include "portable.h"
#include "trashDir.h"
#include "blatShare.h"

struct psl *pslListFromBigPslFile(char *bbFileName)
/* Read every alignment out of a bigPsl bigBed file into a psl list, in query-display order.
 * Used to rebuild the Table view for a shared link from the durable custom track. */
{
struct bbiFile *bbi = bigBedFileOpenAlias(bbFileName, chromAliasFindAliases);
struct lm *lm = lmInit(0);
struct psl *pslList = NULL;
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct bigBedInterval *bb, *ivList = bigBedIntervalQuery(bbi, chrom->name, 0, chrom->size, 0, lm);
    for (bb = ivList; bb != NULL; bb = bb->next)
        {
        /* pslFromBigPsl ignores its seqTypeField arg (it reads seqType from the record itself). */
        struct psl *psl = pslFromBigPsl(chrom->name, bb, 0, NULL, NULL);
        /* pslFromBigPsl always yields a two-char (query,target) strand with the target normalized
         * to '+'; a DNA BLAT is shown with a single-char strand, so drop the redundant target '+'
         * for non-protein queries to match the classic/fresh result page. */
        if (!pslIsProtein(psl) && psl->strand[1] == '+')
            psl->strand[1] = 0;
        slAddHead(&pslList, psl);
        }
    }
bbiChromInfoFreeList(&chromList);
lmCleanup(&lm);
bbiFileClose(&bbi);
return pslList;
}

struct psl *pslFromBigPslFileMatch(char *bbFileName, char *chrom, int tStart, char *qName,
                                   char **retSeq, char **retCds)
/* Return the single alignment in a bigPsl bigBed matching chrom:tStart and qName, together with its
 * stored query sequence (retSeq) and CDS (retCds) when those out pointers are non-NULL, or NULL if
 * there is no such alignment.  Caller frees the returned psl (and any returned seq and cds). */
{
struct bbiFile *bbi = bigBedFileOpenAlias(bbFileName, chromAliasFindAliases);
struct lm *lm = lmInit(0);
struct psl *result = NULL;
if (retSeq != NULL)
    *retSeq = NULL;
if (retCds != NULL)
    *retCds = NULL;
/* Every record overlapping [tStart, tStart+1) is returned; keep the one that actually starts at
 * tStart and carries the requested query name. */
struct bigBedInterval *bb, *ivList = bigBedIntervalQuery(bbi, chrom, tStart, tStart + 1, 0, lm);
for (bb = ivList; bb != NULL; bb = bb->next)
    {
    if (bb->start != tStart)
        continue;
    char *seq = NULL, *cds = NULL;
    struct psl *psl = pslFromBigPsl(chrom, bb, 0, &seq, &cds);
    if (sameString(psl->qName, qName))
        {
        if (!pslIsProtein(psl) && psl->strand[1] == '+')
            psl->strand[1] = 0;
        result = psl;
        if (retSeq != NULL)
            *retSeq = seq;
        else
            freeMem(seq);
        if (retCds != NULL)
            *retCds = cds;
        else
            freeMem(cds);
        break;
        }
    pslFree(&psl);
    freeMem(seq);
    freeMem(cds);
    }
lmCleanup(&lm);
bbiFileClose(&bbi);
return result;
}

char *blatFindPinnedBigPsl(struct cart *cart)
/* Return a cloned path to the BLAT bigPsl bigBed that hgc's buildBigPsl pinned in the cart
 * (blatLastBigBed), or NULL.  Reading the pinned value is unambiguous even when the cart holds
 * several BLAT custom tracks from earlier searches.  A shared session (?u=&s=) can carry an arbitrary
 * value here, so only ever accept a local file the server itself placed under its trash dir, or under
 * the durable session-data dir (where a saved session's trash files are moved) - never a remote URL
 * or an arbitrary local path.  This is the trash/sessionData allow-list from isValidBigDataUrl(). */
{
char *f = cartOptionalString(cart, "blatLastBigBed");
if (f == NULL)
    return NULL;
if (isTrashOrSessionDataPath(f))
    return cloneString(f);
return NULL;
}
