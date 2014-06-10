/* netContigs - get query contigs in a chrom level net file */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "chainNet.h"
#include "liftUp.h"


struct hash *chromHash; 
struct hash *contigNames;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netContigs - get query contigs in a chrom level net file\n"
  "usage:\n"
  "   netContigs file.net file.lft\n"
  );
}

void addContig(char *contig)
{
//uglyf("found contig: %s\n", contig);
if (hashLookup(contigNames, contig) == NULL)
    {
    //uglyf("adding contig: %s\n", contig);
    hashAdd(contigNames, contig, contig);
    }
}

void getFillContig(struct cnFill *fill)
{
struct liftSpec *lift, *prevLift = NULL;
struct hashEl *el;

//uglyf("fill->qName=%s, fill->qStart=%d\n", fill->qName, fill->qStart);
if ((el = hashLookup(chromHash, fill->qName)) == NULL)
    errAbort("Unknown chrom: %s\n", fill->qName);
for (lift = (struct liftSpec *)el->val; lift != NULL; prevLift = lift, lift = lift->next)
    {
    //uglyf("lift->oldName=%s, lift->offset=%d\n", lift->oldName, lift->offset);
    if (fill->qStart < lift->offset)
        {
        addContig(prevLift->oldName);
        break;
        }
    }
    /* last contig */
    addContig(prevLift->oldName);
}


void rLower(struct cnFill *fillList)
{
struct cnFill *fill;

/* Recursively get contig for query in fill item */
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    if (fill->chainId)
        getFillContig(fill);
    if (fill->children)
        rLower(fill->children);
    }
}

void printContig(struct hashEl *el)
{
    printf("%s\n", el->name);
}

void netContigs(char *netFile, char *liftFile)
/* netContigs - get query-side contigs from a chrom-level net file */
{
struct lineFile *lf = lineFileOpen(netFile, TRUE);
struct chainNet *net;

struct liftSpec *lift, *prevLift, *lifts;
char *chrom;

/* read lift file and split into a hash of per-chrom lists */
lifts = readLifts(liftFile);
prevLift = NULL;
for (lift = lifts; lift != NULL; lift = lift->next)
    {
    //uglyf("reading lift: %s\n", lift->oldName);
    if (hashLookup(chromHash, lift->newName) == NULL)
        {
        /* new chrom */
        //uglyf("adding chrom: %s\n", lift->newName);
        hashAdd(chromHash, lift->newName, lift);
        /* terminate previous list */
        /* NOTE: expects input sorted by chrom */
        if (prevLift != NULL)
            prevLift->next = NULL;
        }
    prevLift = lift;
    }

/* read in nets and convert query side to contig coords */
//uglyf("reading in nets\n");
while ((net = chainNetRead(lf)) != NULL)
    {
    rLower(net->fillList);
    chainNetFree(&net);
    }
/* print accumulated contigs */
hashTraverseEls(contigNames, printContig);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
chromHash = newHash(0);
contigNames = newHash(0);
netContigs(argv[1], argv[2]);
return 0;
}
