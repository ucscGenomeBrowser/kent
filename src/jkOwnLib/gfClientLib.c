/* gfClientLib - stuff that both blat and pcr clients of
 * genoFind use. */
/* Copyright 2001-2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "dnaseq.h"
#include "fa.h"
#include "nib.h"
#include "twoBit.h"
#include "repMask.h"
#include "gfClientLib.h"

void gfClientFileArray(char *fileName, char ***retFiles, int *retFileCount)
/* Check if file if .2bit or .nib or .fa.  If so return just that
 * file in a list of one.  Otherwise read all file and treat file
 * as a list of filenames.  */
{
boolean gotSingle = FALSE;
char *buf;		/* This will leak memory but won't matter. */

if (nibIsFile(fileName) || twoBitIsSpec(fileName)
    || sameString(fileName, "stdin")
    || endsWith(fileName, ".Z")
    || endsWith(fileName, ".gz")
    || endsWith(fileName, ".bz2"))
    gotSingle = TRUE;
/* Detect .fa files (where suffix is not standardized)
 * by first character being a '>'. */
else
    {
    FILE *f = mustOpen(fileName, "r");
    char c = fgetc(f);
    fclose(f);
    if (c == '>')
        gotSingle = TRUE;
    }
if (gotSingle)
    {
    char **files;
    *retFiles = AllocArray(files, 1);
    files[0] = cloneString(fileName);
    *retFileCount = 1;
    return;
    }
else
    {
    readAllWords(fileName, retFiles, retFileCount, &buf);
    }
}


void gfClientUnmask(struct dnaSeq *seqList)
/* Unmask all sequences. */
{
struct dnaSeq *seq;
for (seq = seqList; seq != NULL; seq = seq->next)
    faToDna(seq->dna, seq->size);
}

static void maskFromOut(struct dnaSeq *seqList, char *outFile, float minRepDivergence)
/* Mask DNA sequence by putting bits more than 85% identical to
 * repeats as defined in RepeatMasker .out file to upper case. */
{
struct lineFile *lf = lineFileOpen(outFile, TRUE);
struct hash *hash = newHash(0);
struct dnaSeq *seq;
char *line;

for (seq = seqList; seq != NULL; seq = seq->next)
    hashAdd(hash, seq->name, seq);
if (!lineFileNext(lf, &line, NULL))
    errAbort("Empty mask file %s\n", lf->fileName);
if (!startsWith("There were no", line))	/* No repeats is ok. Not much work. */
    {
    if (!startsWith("   SW", line))
	errAbort("%s isn't a RepeatMasker .out file.", lf->fileName);
    if (!lineFileNext(lf, &line, NULL) || !startsWith("score", line))
	errAbort("%s isn't a RepeatMasker .out file.", lf->fileName);
    lineFileNext(lf, &line, NULL);  /* Blank line. */
    while (lineFileNext(lf, &line, NULL))
	{
	char *words[32];
	struct repeatMaskOut rmo;
	int wordCount;
	int seqSize;
	int repSize;
	wordCount = chopLine(line, words);
	if (wordCount < 14)
	    errAbort("%s line %d - error in repeat mask .out file\n", lf->fileName, lf->lineIx);
	repeatMaskOutStaticLoad(words, &rmo);
	/* If repeat is more than 15% divergent don't worry about it. */
	if (rmo.percDiv + rmo.percDel + rmo.percInc <= minRepDivergence)
	    {
	    if((seq = hashFindVal(hash, rmo.qName)) == NULL)
		errAbort("%s is in %s but not corresponding sequence file, files out of sync?\n", 
			rmo.qName, lf->fileName);
	    seqSize = seq->size;
	    if (rmo.qStart <= 0 || rmo.qStart > seqSize || rmo.qEnd <= 0 
	    	|| rmo.qEnd > seqSize || rmo.qStart > rmo.qEnd)
		{
		warn("Repeat mask sequence out of range (%d-%d of %d in %s)\n",
		    rmo.qStart, rmo.qEnd, seqSize, rmo.qName);
		if (rmo.qStart <= 0)
		    rmo.qStart = 1;
		if (rmo.qEnd > seqSize)
		    rmo.qEnd = seqSize;
		}
	    repSize = rmo.qEnd - rmo.qStart + 1;
	    if (repSize > 0)
		toUpperN(seq->dna + rmo.qStart - 1, repSize);
	    }
	}
    }
freeHash(&hash);
lineFileClose(&lf);
}

static void maskNucSeqList(struct dnaSeq *seqList, char *seqFileName, char *maskType,
	boolean hardMask, float minRepDivergence)
/* Apply masking to simple nucleotide sequence by making masked nucleotides
 * upper case (since normal DNA sequence is lower case for us). */
{
struct dnaSeq *seq;
char *outFile = NULL, outNameBuf[512];

if (sameWord(maskType, "upper"))
    {
    /* Already has dna to be masked in upper case. */
    }
else if (sameWord(maskType, "lower"))
    {
    for (seq = seqList; seq != NULL; seq = seq->next)
	toggleCase(seq->dna, seq->size);
    }
else
    {
    /* Masking from a RepeatMasker .out file. */
    if (sameWord(maskType, "out"))
	{
	sprintf(outNameBuf, "%s.out", seqFileName);
	outFile = outNameBuf;
	}
    else
	{
	outFile = maskType;
	}
    gfClientUnmask(seqList);
    maskFromOut(seqList, outFile, minRepDivergence);
    }
if (hardMask)
    {
    for (seq = seqList; seq != NULL; seq = seq->next)
	upperToN(seq->dna, seq->size);
    }
}

bioSeq *gfClientSeqList(int fileCount, char *files[], 
	boolean isProt, boolean isTransDna, char *maskType, 
	float minRepDivergence, boolean showStatus)
/* From an array of .fa and .nib file names, create a
 * list of dnaSeqs, which are set up so that upper case is masked and lower case
 * is unmasked sequence. (This is the opposite of the input, but set so that
 * we can use lower case as our primary DNA sequence, which historically predates
 * our use of lower case masking.)  Protein sequence on the other hand is
 * all upper case. */

{
int i;
char *fileName;
bioSeq *seqList = NULL, *seq;
boolean doMask = (maskType != NULL);

for (i=0; i<fileCount; ++i)
    {
    struct dnaSeq *list = NULL, sseq;
    ZeroVar(&sseq);
    fileName = files[i];
    if (nibIsFile(fileName))
	list = nibLoadAllMasked(NIB_MASK_MIXED|NIB_BASE_NAME, fileName);
    else if (twoBitIsSpec(fileName))
	list = twoBitLoadAll(fileName);
    else if (isProt)
      list = faReadAllPep(fileName);
    else
      list = faReadAllMixed(fileName);

    /* If necessary mask sequence from file. */
    if (doMask)
	{
	maskNucSeqList(list, fileName, maskType, isTransDna, minRepDivergence);
	}
    else
        {
	/* If not masking send everything to proper case here. */
	for (seq = list; seq != NULL; seq = seq->next)
	    {
	    if (isProt)
		faToProtein(seq->dna, seq->size);
	    else
		faToDna(seq->dna, seq->size);
	    }
	}
    /* Move local list to end of bigger list. */
    seqList = slCat(seqList, list);
    }
if (showStatus)
    {
    /* Total up size and sequence count and report. */
    int count = 0; 
    unsigned long totalSize = 0;
    for (seq = seqList; seq != NULL; seq = seq->next)
        {
	totalSize += seq->size;
	count += 1;
	}
    printf("Loaded %lu letters in %d sequences\n", totalSize, count);
    }
return seqList;
}

