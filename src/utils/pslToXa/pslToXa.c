/* pslToXa - Convert from psl to xa alignment format. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "dnaseq.h"
#include "fa.h"
#include "nib.h"
#include "psl.h"
#include "xAli.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslToXa - Convert from psl to xa alignment format\n"
  "usage:\n"
  "   pslToXa [options] in.psl out.xa qSeqDir tSeqDir\n"
  "options:\n"
  "   -masked - use lower case characters for masked-out bases\n"
  );
}

struct seqFile
/* Info about a file. */
    {
    struct seqFile *next;
    char *name;	/* Name (allocated in hash) */
    FILE *f;	/* File handle. */
    boolean isNib;	/* Is it a nib file. */
    int nibSize;	/* Nib file size. */
    };

struct seqInFile
/* A structure that ties a sequence to a position in a file. */
    {
    struct seqInFile *next;	/* Next in list. */
    char *name;		/* Sequence name (allocated in hash) */
    struct seqFile *file;	/* File this is in. */
    size_t startInFile;	        /* Position in file (just for fa files). */
    };

void addNib(struct seqFile *seqFile, char *prefix, struct hash *seqHash)
/* Add sequences from nib file. */
{
char fileName[128];
char seqName[256];
struct seqInFile *sif;

nibOpenVerify(seqFile->name, &seqFile->f, &seqFile->nibSize);
splitPath(seqFile->name, NULL, fileName, NULL);
sprintf(seqName, "%s%s", prefix, fileName);
AllocVar(sif);
hashAddSaveName(seqHash, seqName, sif, &sif->name);
sif->file = seqFile;
}

void addFa(struct seqFile *seqFile, char *prefix, struct hash *seqHash)
/* Add sequences from fa file. */
{
errAbort("Fasta format not yet implemented");
}

void scanFilesInDir(char *dir, char *prefix,
	struct hash *fileHash, struct hash *seqHash)
/* Scan through files in directory. */
{
struct fileInfo *dirList = NULL, *dirEl;
struct seqFile *seqFile;

fprintf(stderr, "Scanning %s", dir);
fflush(stderr);

/* Read directory. */
dirList = listDirX(dir, "*.nib", TRUE);
if (dirList == NULL)
    dirList = listDirX(dir, "*.fa", TRUE);
if (dirList == NULL)
    dirList = listDirX(dir, "*", TRUE);

for (dirEl = dirList; dirEl != NULL; dirEl = dirEl->next)
    {
    char *name = dirEl->name;
    fprintf(stderr, ".");
    fflush(stderr);
    if (!hashLookup(fileHash, name))
        {
	AllocVar(seqFile);
	hashAddSaveName(fileHash, name, seqFile, &seqFile->name);
	seqFile->isNib = endsWith(name, ".nib");
	if (seqFile->isNib)
	     addNib(seqFile, prefix, seqHash); 
	else
	     addFa(seqFile, prefix, seqHash);
	}
    }
slFreeList(&dirList);
fprintf(stderr, "\n");
}


void outputSeqBlocks(int options, FILE *f, struct hash *seqHash,
     char *name, char strand, int start, int end, 
     int blockCount, unsigned *starts, unsigned *sizes)
/* Output sequence blocks as comma separated list. */
{
struct seqInFile *sif = hashMustFindVal(seqHash, name);
struct seqFile *seqFile = sif->file;
struct dnaSeq *seq = NULL;
DNA *dna;
int seqOffset, seqSizeLoaded, seqSizeTotal;
int blockIx;

if (!seqFile->isNib)
    errAbort("Only nibs currently supported");

/* Load from nib. */
    {
    seqOffset = start;
    seqSizeLoaded = end-start;
    seqSizeTotal = seqFile->nibSize;
    seq = nibLdPartMasked(options, seqFile->name, seqFile->f, seqFile->nibSize,
         start, seqSizeLoaded);
    }
dna = seq->dna;
if (strand == '-')
    {
    reverseComplement(seq->dna, seq->size);
    seqOffset = seqSizeTotal - end;
    }
for (blockIx = 0; blockIx < blockCount; ++blockIx)
    {
    mustWrite(f, dna + starts[blockIx] - seqOffset, sizes[blockIx]);
    fputc(',', f);
    }
freeDnaSeq(&seq);
}

void outputConverted(int options, struct psl *psl, FILE *f,
                     struct hash *seqHash)
/* Output psl in xa format. */
{
char seqName[256];
/* Print out fields that are shared with psl. */
pslOutput(psl, f, '\t', '\t');

/* Print out query sequence. */
sprintf(seqName, "q.%s", psl->qName);
outputSeqBlocks(options, f, seqHash, seqName, psl->strand[0], 
	psl->qStart, psl->qEnd,
	psl->blockCount, psl->qStarts, psl->blockSizes);
fputc('\t', f);

/* Print out target sequence. */
sprintf(seqName, "t.%s", psl->tName);
outputSeqBlocks(options, f, seqHash, seqName, psl->strand[1], 
	psl->tStart, psl->tEnd,
	psl->blockCount, psl->tStarts, psl->blockSizes);
fputc('\n', f);
}

void pslToXa(int options, char *inName, char *outName, char *qSeqDir, char *tSeqDir)
/* pslToXa - Convert from psl to xa alignment format. */
{
struct hash *fileHash = newHash(0);
struct hash *seqHash = newHash(18);
struct lineFile *lf = pslFileOpen(inName);
FILE *f = mustOpen(outName, "w");
struct psl *psl;

scanFilesInDir(qSeqDir, "q.", fileHash, seqHash);
scanFilesInDir(tSeqDir, "t.", fileHash, seqHash);
while ((psl = pslNext(lf)) != NULL)
    {
    outputConverted(options, psl, f, seqHash);
    pslFree(&psl);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
int options = 0;
optionHash(&argc, argv);
if (argc != 5)
    usage();
if (optionExists("masked"))
    options = NIB_MASK_MIXED;
pslToXa(options, argv[1], argv[2], argv[3], argv[4]);
return 0;
}
