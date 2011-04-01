/* This program not used at the moment.  The way it was picking alt promoters, which was the
 * main thing it had over the approach cobbled together with a bunch of scripts, turned out
 * to lose too many genes.  Particularly the heavily expressed ones would tend to have one
 * isoform that wasn't complete at the 5' end.  As like as not this will change later after
 * more experimentation....*/

/* regCompanionPickGeneSet - Pick a simple gene set to use for the companion paper. 
 * This uses the criteria:
 * o canonical: Using the "canonical" isoform.
 * o noAltPro: Separated into ones with and without alternative promoters.  Most work done on
 *   ones without alternative promoters.
 * o multi-exon: for quite a number of reasons single exon genes are a special case.
 *   Often it's an indication the first exon is not actually sequenced for one thing.
 *   Often they are pseudo genes.  When real genes the biology behind them is often
 *   different. Also we can't check intron/exon transcription levels to help see if
 *   low level transcripts really are expressed.
 * o noInterleaving: Separated into ones with and without interleaving genes.  Most work done on 
 *   ones not interleaved. 
 * o promoter not within 1000 kb of promoter of another gene.
 */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "basicBed.h"
#include "txGraph.h"
#include "obscure.h"
#include "localmem.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanionPickGeneSet - Pick a simple gene set to use for the companion paper.\n"
  "usage:\n"
  "   regCompanionPickGeneSet inGenes.txg inGenes.bed inCanonical.lst outGenes.lst\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct slName *txGraphListAllAccessions(struct txGraph *txgList)
/* Return list of all accessions of sources in list of txGraphs. */
{
struct txGraph *txg;
struct slName *list = NULL;
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    int i;
    for (i=0; i<txg->sourceCount; ++i)
	slNameAddHead(&list, txg->sources[i].accession);
    }
slReverse(&list);
return list;
}

struct slName *bedListAllNames(struct bed *bedList)
/* Return list of all names in bedList */
{
struct slName *list = NULL;
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
   slNameAddHead(&list, bed->name);
slReverse(&list);
return list;
}

struct hash *makeUniqueHash(struct slName *list, char *source)
/* Make a hash out of list, and make sure all names are unique.  Complain about non-uniqueness
 * in source and die if not. Return hash*/
{
struct hash *hash = hashNew(0);
struct slName *el;
for (el = list; el != NULL; el = el->next)
    {
    char *name = el->name;
    if (hashLookup(hash, name))
        errAbort("ID %s is duplicated in %s", name, source);
    hashAdd(hash, name, NULL);
    }
return hash;
}

void makeSureInHash(struct hash *hash, struct slName *list, char *hashSource, char *listSource)
/* Make sure everything in list is in hash, issuing appropriate complaint and aborting if not. */
{
struct slName *el;
for (el = list; el != NULL; el = el->next)
    {
    char *name = el->name;
    if (!hashLookup(hash, name))
        errAbort("ID %s is in %s but not %s", name, listSource, hashSource);
    }
}


void insureUsingSameIds(struct slName *aList, char *aSource, struct slName *bList, char *bSource,
	struct slName *cList, char *cSource)
/* Make sure that everything in aList is unique and in bList, and vice versa. Also make sure
 * that everything in cList is in a and b lists.  (But c-list does not need to cover
 * everything). */
{
struct hash *aHash = makeUniqueHash(aList, aSource);
makeSureInHash(aHash, bList, aSource, bSource);
hashFree(&aHash);
struct hash *bHash = makeUniqueHash(bList, bSource);
makeSureInHash(bHash, aList, bSource, aSource);
makeSureInHash(bHash, cList, bSource, cSource);
hashFree(&bHash);
}

boolean hasAltPromoters(struct txGraph *txg, struct hash *bedHash, int mergeSize)
/* Return TRUE if there are different transcript sources in txg that start in
 * significantly different (more than mergeSize) places. */
{
/* We just check that all starts are within mergeSize of the overall txGraph start. */
int i;
if (txg->strand[0] == '+')
    {
    int start = txg->tStart;
    for (i=0; i<txg->sourceCount; ++i)
        {
	struct bed *bed = hashMustFindVal(bedHash, txg->sources[i].accession);
	if (bed->chromStart - start > mergeSize)
	    return TRUE;
	}
    }
else
    {
    int start = txg->tEnd;
    for (i=0; i<txg->sourceCount; ++i)
        {
	struct bed *bed = hashMustFindVal(bedHash, txg->sources[i].accession);
	if (start - bed->chromEnd > mergeSize)
	    return TRUE;
	}
    }
return FALSE;
}

void regCompanionPickGeneSet(char *inTxgFile, char *inBedFile, char *inCanonicalFile, char *outFile)
/* regCompanionPickGeneSet - Pick a simple gene set to use for the companion paper. */
{
/* Load in inputs, and verify that they belong together - that all IDs are found
 * once and only once in each. */
struct txGraph *txgList = txGraphLoadAll(inTxgFile);
struct bed *inBedList = bedLoadAll(inBedFile);
struct slName *txgIdList = txGraphListAllAccessions(txgList);
struct slName *bedIdList = bedListAllNames(inBedList);
struct slName *canonicalList = readAllLines(inCanonicalFile);
insureUsingSameIds(txgIdList, inTxgFile, bedIdList, inBedFile, canonicalList, inCanonicalFile);
verbose(1, "%s and %s loaded and are using same IDs\n", inTxgFile, inBedFile);

/* Make hash of beds for easy lookup. */
struct hash *inBedHash = hashNew(18);
struct bed *bed;
for (bed = inBedList; bed != NULL; bed = bed->next)
    hashAdd(inBedHash, bed->name, bed);

/* Build up list of txGraphs with no alt promoters. */
struct slRef *noAltTxgList = NULL;
struct txGraph *txg;
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    boolean hasAlt = hasAltPromoters(txg, inBedHash, 50);
    uglyf("%s %s:%d-%d alt? %s\n", txg->strand, txg->tName, txg->tStart, txg->tEnd, (hasAlt ? "yes" : "no"));
    }
uglyf("%d items in noAltTxgList\n", slCount(noAltTxgList));

struct hash *canonicalHash = makeUniqueHash(canonicalList, inCanonicalFile);
uglyf("%d els in canonicalHash\n", canonicalHash->elCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
regCompanionPickGeneSet(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
