/* agpAllToFaFile - Convert a .agp file to a .fa file containing all sequences*/

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"
#include "agpFrag.h"
#include "agpGap.h"


boolean doSort = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpAllToFaFile - Convert all sequences in an .agp file to a .fa file \n"
  "usage:\n"
  "   agpAllToFaFile in.agp in.fa out.fa\n"
  "   options:\n"
  "   -sizes=out - save chrom sizes in out.sizes and gap sizes in out.gaps\n"
  "   -sort - preserve the in.agp chrom order\n"
  );
}

char *chromSizeFile = NULL;        /* file for chrom sizes */
char *chromGapSizeFile = NULL;     /* file for chrom gap sizes */

void agpAllToFaFile(char *agpFile, char *faIn, char *faOut)
/* agpAllToFaFile - Convert a .agp file to a .fa file. */
{
struct lineFile *lf = lineFileOpen(agpFile, TRUE); /* input AGP file */
FILE *fIn, *fOut;                     /* input and output fasta file */
FILE *fSizes = NULL;                  /* output file w/ chrom sizes */
FILE *fGapSizes = NULL;               /* output file w/ chrom gap sizes */
char *line, *words[16];
int lineSize, wordCount;
int lastPos = 0;
struct agpFrag *agpList = NULL, *agp;
struct hash *chromHash = newHash(0);
struct hash *chromSizeHash = newHash(0);
struct hash *chromGapSizeHash = newHash(0);
struct hash *fragHash = newHash(17);
char *seqName = "";
DNA *dna = NULL;
struct dnaSeq *seq;
struct hashCookie cookie;
struct hashEl *hel;
int gap;                /* count of gap bases for a chrom */
struct slName *chromNames = NULL, *chromName=NULL;

/* read in AGP file, constructing hash of chrom agp lists */
verbose(1, "Reading %s\n", agpFile);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#' || line[0] == '\n')
        continue;
    wordCount = chopLine(line, words);
    if (wordCount < 5)
        errAbort("Bad line %d of %s\n", lf->lineIx, lf->fileName);
    if (words[4][0] != 'N' && words[4][0] != 'U')
	{
        /* not a gap */
	lineFileExpectWords(lf, 9, wordCount);
        seqName = words[0];
        agp = agpFragLoad(words);
        // file is 1-based but agpFragLoad() now assumes 0-based:
        agp->chromStart -= 1;
        agp->fragStart  -= 1;
        if (!hashFindVal(chromHash, seqName))
            lastPos = 0;
        if (agp->chromStart != lastPos)
            errAbort("Start doesn't match previous end line %d of %s\n",
                         lf->lineIx, lf->fileName);
        if (agp->chromEnd - agp->chromStart != agp->fragEnd - agp->fragStart)
            errAbort("Sizes don't match in %s and %s line %d of %s\n",
                        agp->chrom, agp->frag, lf->lineIx, lf->fileName);
        if (!hashFindVal(chromHash, seqName))
            {
	    if (agpList)
		slReverse(&agpList);
            /* new chrom */
            AllocVar(agpList);
            /* add to hashes of chrom agp lists and sizes */
            hashAdd(chromHash, seqName, agpList);
            hashAddInt(chromSizeHash, seqName, agp->chromEnd);
	    if (doSort)
		slNameAddHead(&chromNames, seqName);
            }
        slAddHead(&agpList, agp);
        lastPos = agp->chromEnd;
	}
    else
        {
        /* gap */
        gap = lineFileNeedNum(lf, words, 5);
        if (hashLookup(chromGapSizeHash, seqName) != NULL)
            {
            gap += hashIntVal(chromGapSizeHash, seqName);
            hashRemove(chromGapSizeHash, seqName);
            }
        hashAddInt(chromGapSizeHash, seqName, gap);
	lastPos = lineFileNeedNum(lf, words, 2);
	}
    if (hashLookup(chromSizeHash, seqName) != NULL)
        hashRemove(chromSizeHash, seqName);
    hashAddInt(chromSizeHash, seqName, lastPos);
    }
if (agpList)
    slReverse(&agpList);
/* read in input fasta file */
verbose(1, "Reading %s\n", faIn);
fIn = mustOpen(faIn, "r");
while (faReadMixedNext(fIn, TRUE, "bogus", TRUE, NULL, &seq))
    hashAdd(fragHash, seq->name, seq);

/* set up output files */
verbose(1, "Writing %s\n", faOut);
fOut = mustOpen(faOut, "w");
if (chromSizeFile != NULL)
    {
    verbose(1, "Writing %s\n", chromSizeFile);
    fSizes = mustOpen(chromSizeFile, "w");
    fGapSizes = mustOpen(chromGapSizeFile, "w");
    }


if (doSort)
    {
    slReverse(&chromNames);
    chromName=chromNames; 
    }
else
    cookie = hashFirst(chromHash); 

/* traverse hash, writing out sequence records */
while (TRUE)
    {
    if (doSort)
	{
    	if (chromName == NULL)
    	    break;
	hel = hashLookup(chromHash, chromName->name);
	}
    else
	{
    	if ((hel = hashNext(&cookie)) == NULL)
    	    break;
	}
	
    seqName = (char *)hel->name;
    agpList = (struct agpFrag *)hel->val;
    lastPos = hashIntVal(chromSizeHash, seqName);
    if (lastPos == 0)
        errAbort("%s not found\n", seqName);
    if (fSizes != NULL)
        {
        fprintf(fSizes, "%s\t%d\n", seqName, lastPos);
        if (hashLookup(chromGapSizeHash, seqName))
            {
            gap = hashIntVal(chromGapSizeHash, seqName);
            fprintf(fGapSizes, "%s\t%d\n", seqName, gap);
            }
        }
    dna = needLargeMem(lastPos+1);
    memset(dna, 'n', lastPos);
    dna[lastPos] = 0;
    for (agp = agpList->next; agp != NULL; agp = agp->next)
        {
        int size;
        verbose(2,"%s\n", agp->frag);
        seq = hashFindVal(fragHash, agp->frag);
        if (seq == NULL)
            errAbort("Couldn't find %s in %s", agp->frag, faIn);
        size = agp->fragEnd - agp->fragStart;
        memcpy(dna + agp->chromStart, seq->dna + agp->fragStart, size);
        if (agp->strand[0] == '-')
            reverseComplement(dna + agp->chromStart, size);
        }
    verbose(2,"Writing sequence %s, %d bases to %s\n", seqName, lastPos, faOut);
    faWriteNext(fOut, seqName, dna, lastPos);
    freeMem(dna);
    if (doSort)
	chromName=chromName->next;
    }

if (doSort)
    slNameFreeList(&chromNames);
    
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *sizeFileBase = NULL;

optionHash(&argc, argv);
if (argc != 4)
    usage();
sizeFileBase = optionVal("sizes", sizeFileBase);
if (sizeFileBase != NULL)
    {
    chromSizeFile = addSuffix(cloneString(sizeFileBase), ".sizes");
    chromGapSizeFile = addSuffix(cloneString(sizeFileBase), ".gaps");
    }
if (optionExists("sort"))
    doSort = TRUE;
agpAllToFaFile(argv[1], argv[2], argv[3]);
return 0;
}
