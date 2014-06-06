/* agpToFa - Convert a .agp file to a .fa file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "agpFrag.h"
#include "agpGap.h"
#include "options.h"


static void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpToFa - Convert a .agp file to a .fa file\n"
  "usage:\n"
  "   agpToFa in.agp agpSeq out.fa freezeDir\n"
  "Where agpSeq matches a sequence name in in.agp (or is \"all\")\n"
  "and seqDir is where program looks for sequence.\n"
  "\n"
  "This is currently a fairly limited implementation.\n"
  "It only works on finished clones\n"
  "\n"
  "options:\n"
  "   -simple - treat freezeDir as a simple directory full of\n"
  "             .fa files.  In this case .fa files must be named\n"
  "             accession.fa and only have one record\n"
  "   -subDirs=predraft,draft,fin,extras  Explicitly specify comma separated\n"
  "             list of freeze subdirectories.\n"
  "   -simpleMulti - treat freezeDir as a single file, multi-record FASTA\n"
  "                  which contains all fragment records.\n"
  "   -simpleMultiMixed - same as simpleMulti, but preserves \n"
  "                  mixed-case sequence\n"
  "   -verbose=N - N=2 display illegal coordinates in agp files\n"
  );
}

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"simple", OPTION_BOOLEAN},
    {"subDirs", OPTION_STRING},
    {"simpleMulti", OPTION_BOOLEAN},
    {"simpleMultiMixed", OPTION_BOOLEAN},
    {NULL, 0}
};

/* Overridable options. */
static char *subDirs = "extras,fin,draft,predraft";

static void simpleMultiFillInSequence(boolean preserveCase, char *seqFile, 
                        struct agpFrag *agpList, DNA *dna, int dnaSize)
/* Fill in DNA array with sequences from one multi-record fasta file. */
{
static struct hash *fragHash = NULL;
struct agpFrag *agp = NULL;
struct dnaSeq *seq = NULL;
int illegals = 0;

if (fragHash == NULL)
    {
    struct lineFile *lf = lineFileOpen(seqFile, TRUE);
    struct dnaSeq *seqList = faReadAllMixedInLf(lf);
    fragHash = newHash(17);
    for (seq = seqList;  seq != NULL;  seq = seq->next)
	{
	if (!preserveCase)
	    tolowers(seq->dna);
	hashAdd(fragHash, seq->name, seq);
	}
    lineFileClose(&lf);
    }

for (agp = agpList; agp != NULL; agp = agp->next)
    {
    int size;
    seq = hashFindVal(fragHash, agp->frag);
    if (seq == NULL)
	errAbort("Couldn't find %s in %s", agp->frag, seqFile);
    size = agp->fragEnd - agp->fragStart;
    if (agp->strand[0] == '-')
	reverseComplement(seq->dna + agp->fragStart, size);
    verbose(3, "dna: %lu, start: %u, size: %d, seqDna: %lu, start: %u, "
	    "size: %d %s %d\n",
	    (unsigned long) dna, agp->chromStart, dnaSize,
	    (unsigned long) seq->dna, agp->fragStart,
	    size, agp->frag, seq->size);
    if ((agp->chromStart + size) > dnaSize)
	{
	warn("frag size %d + dest chromStart %u  = %d > %d dest dna size\n",
		size, agp->chromStart, size+agp->chromStart, dnaSize);
	errAbort("fragment copy size exceeds destination dna size");
	}
    if ((agp->fragStart + size) > seq->size)
	{
	verbose(2, "#\t%s start:%u end:%u seqSize: %d\n",
		agp->frag, agp->fragStart, agp->fragEnd, seq->size);
	++illegals;
	}
    else
	memcpy(dna + agp->chromStart, seq->dna + agp->fragStart, size);
    }
if (illegals > 0)
    {
    warn("%d illegal coordinates found in agp files.\n", illegals);
    warn("sequence file: %s\n", seqFile);
    errAbort("Fragment copy is more than available fragment sequence.");
    }
}

static void simpleFillInSequence(char *seqDir, struct agpFrag *agpList,
    DNA *dna, int dnaSize)
/* Fill in DNA array with sequences from simple clones. */
{
struct agpFrag *agp;
char underline = '_';

for (agp = agpList; agp != NULL; agp = agp->next)
    {
    char clone[128];
    char path[512];
    struct dnaSeq *seq;
    int size;
    strcpy(clone, agp->frag);
    chopSuffixAt(clone,underline);
    sprintf(path, "%s/%s.fa", seqDir, clone);
    seq = faReadAllDna(path);
    if (slCount(seq) != 1)
	errAbort("Can only handle exactly one clone in %s.", path);
    size = agp->fragEnd - agp->fragStart;
    if (agp->strand[0] == '-')
	reverseComplement(seq->dna + agp->fragStart, size);
    memcpy(dna + agp->chromStart, seq->dna + agp->fragStart, size);
    freeDnaSeq(&seq);
    }
}

struct clone 
/* Info about a clone. */
    {
    struct clone *next;	/* Next in list. */
    char *name;		/* Clone name including version. */
    struct frag *fragList;	/* List of fragments. */
    char *faFile;	/* Location of .fa file. */
    int size;		/* Total size. */
    DNA *dna;		/* Bases, just filled in as needed. */
    };

struct frag
/* Info about a fragment. */
    {
    struct frag *next;	/* Next in list. */
    char *name;		/* Frag name, UCSC format. */
    int start;		/* Start position, 0 based. */
    int end;		/* One past end. */
    struct clone *clone; /* Clone this belongs to. */
    };

static void getCloneDna(struct clone *clone, struct hash *fragHash)
/* Read in clone DNA from file in format with one record per
 * clone contig.   Make clone->dna so that it is same as
 * non-fragmented clone file. */
{
struct dnaSeq *seqList = faReadAllDna(clone->faFile), *seq;
int fragSize;
clone->dna = needLargeMem(clone->size+1);
clone->dna[clone->size] = 0;
uglyf("GetCloneDna %s\n", clone->faFile);
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    struct frag *frag = hashFindVal(fragHash, seq->name);
    if (frag == NULL)
        errAbort("Couldn't find %s from %s in trans files", seq->name, clone->faFile);
    assert(frag->end <= clone->size);
    fragSize = frag->end - frag->start;
    assert(fragSize >= 0);
    if (fragSize != seq->size)
        errAbort("Size mismatch (%d vs %d) between trans and .ffa files on %s", 
		fragSize, seq->size, frag->name);
    memcpy(clone->dna + frag->start,  seq->dna,  fragSize);
    }
freeDnaSeqList(&seqList);
}

static void readTrans(char *transFile, char *faDir, struct hash *cloneHash, struct hash *fragHash)
/* Read in transFile into hashes. */
{
struct lineFile *lf = lineFileOpen(transFile, TRUE);
char *row[3];
char *parts[3], *subParts[2];
int partCount, subCount;
char faName[512];
struct clone *clone;
struct frag *frag;

printf("Reading %s\n", transFile);
while (lineFileRow(lf, row))
    {
    char *cloneName = row[1];
    char *e = strchr(cloneName, '~');
    if (e == NULL)
        errAbort("Missing ~ line %d of %s", lf->lineIx, lf->fileName);
    *e++ = 0;
    if ((clone = hashFindVal(cloneHash, cloneName)) == NULL)
        {
	AllocVar(clone);
	hashAddSaveName(cloneHash, cloneName, clone, &clone->name);
	chopSuffix(cloneName);
	sprintf(faName, "%s/%s.fa", faDir, cloneName);
	cloneName = NULL;
	clone->faFile = cloneString(faName);
	}
    AllocVar(frag);
    hashAddSaveName(fragHash, row[0], frag, &frag->name);
    partCount = chopString(row[2], "(:)", parts, ArraySize(parts));
    if (partCount != 2)
        errAbort("Expecting (ACCESSION.VER:START..STOP) line %d of %s", lf->lineIx, lf->fileName);
    subCount = chopString(parts[1], ".", subParts, ArraySize(subParts));
    if (subCount != 2)
        errAbort("Expecting START..STOP line %d of %s", lf->lineIx, lf->fileName);
    frag->start = atoi(subParts[0])-1;
    frag->end = atoi(subParts[1]);
    frag->clone = clone;
    if (clone->size < frag->end)
        clone->size = frag->end;
    slAddTail(&clone->fragList, frag);
    }
lineFileClose(&lf);
}

static void gsFillInSequence(char *gsDir, struct agpFrag *agpList, DNA *dna, int dnaSize)
/* Fill in DNA array with sequences from 'freeze' */
{
struct agpFrag *agp;
int subCount;
char *sub[64];
char transFile[512], faDir[512];
struct hash *cloneHash = newHash(15), *fragHash = newHash(17);
int i;
struct clone *clone;

/* Read in trans-files to hash */
subCount = chopString(subDirs, ",", sub, ArraySize(sub));
if (subCount >= ArraySize(sub))
    errAbort("Too many subDirs, limit 64.");
for (i=0; i<subCount; ++i)
    {
    sprintf(faDir, "%s/%s/fa", gsDir, sub[i]);
    sprintf(transFile, "%s/%s/trans", gsDir, sub[i]);
    readTrans(transFile, faDir, cloneHash, fragHash);
    }

/* Read sequences and copy them to output DNA array. */
printf("Reading source sequences from %s\n", gsDir);
for (agp = agpList; agp != NULL; agp = agp->next)
    {
    DNA *dest;
    int size;
    if ((clone = hashFindVal(cloneHash, agp->frag)) == NULL)
        errAbort("Couldn't find %s in %s", agp->frag, gsDir);
    if (clone->dna == NULL)
	getCloneDna(clone, fragHash);
    dest = dna + agp->chromStart;
    size = agp->chromEnd - agp->chromStart;
    memcpy(dest, clone->dna + agp->fragStart, size);
    if (agp->strand[0] == '-')
        reverseComplement(dest, size);
    }
}

void agpToFaOne(struct agpFrag **pAgpList, char *agpFile, char *agpSeq,
		char *seqDir, int lastPos, FILE *f)
/* Given one sequence's worth of AGP in pAgpList, process it into FASTA
 * and write to f. */
{
DNA *dna = NULL;

slReverse(pAgpList);
if (lastPos == 0)
    errAbort("%s not found in %s\n", agpSeq, agpFile);
dna = needHugeMem(lastPos+1);
memset(dna, 'n', lastPos);
dna[lastPos] = 0;
if (optionExists("simpleMulti"))
    {
    simpleMultiFillInSequence(0, seqDir, *pAgpList, dna, lastPos);
    }
else if (optionExists("simpleMultiMixed"))
    {
    simpleMultiFillInSequence(1, seqDir, *pAgpList, dna, lastPos);
    }
else if (optionExists("simple"))
    {
    simpleFillInSequence(seqDir, *pAgpList, dna, lastPos);
    }
else
    {
    gsFillInSequence(seqDir, *pAgpList, dna, lastPos);
    }
verbose(2,"Writing %s (%d bases)\n", agpSeq, lastPos);
faWriteNext(f, agpSeq, dna, lastPos);
agpFragFreeList(pAgpList);
}


static void agpToFa(char *agpFile, char *agpSeq, char *faOut, char *seqDir)
/* agpToFa - Convert a .agp file to a .fa file. */
{
struct lineFile *lf = lineFileOpen(agpFile, TRUE);
char *line, *words[16];
int lineSize, wordCount;
int lastPos = 0;
struct agpFrag *agpList = NULL, *agp;
FILE *f = mustOpen(faOut, "w");
char *prevChrom = NULL;

verbose(2,"#\tprocessing AGP file: %s\n", agpFile);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == 0 || line[0] == '#' || line[0] == '\n')
        continue;
    wordCount = chopLine(line, words);
    if (wordCount < 5)
        errAbort("Bad line %d of %s: need at least 5 words, got %d\n",
		 lf->lineIx, lf->fileName, wordCount);
    if (! (sameWord("all", agpSeq) || sameWord(words[0], agpSeq)))
	continue;
    if (prevChrom != NULL && !sameString(prevChrom, words[0]))
	{
	agpToFaOne(&agpList, agpFile, prevChrom, seqDir, lastPos, f);
	lastPos = 0;
	}
    if (words[4][0] != 'N' && words[4][0] != 'U')
	{
	lineFileExpectAtLeast(lf, 9, wordCount);
	agp = agpFragLoad(words);
	/* file is 1-based but agpFragLoad() now assumes 0-based: */
	agp->chromStart -= 1;
	agp->fragStart  -= 1;
	if (agp->chromStart != lastPos)
	    errAbort("Start doesn't match previous end line %d of %s\n",
		     lf->lineIx, lf->fileName);
	if (agp->chromEnd - agp->chromStart != agp->fragEnd - agp->fragStart)
	    errAbort("Sizes don't match in %s and %s line %d of %s\n",
		     agp->chrom, agp->frag, lf->lineIx, lf->fileName);
	slAddHead(&agpList, agp);
	lastPos = agp->chromEnd;
	}
    else
        {
	lastPos = lineFileNeedNum(lf, words, 2);
	}
    if (prevChrom == NULL || !sameString(prevChrom, words[0]))
	{
	freeMem(prevChrom);
	prevChrom = cloneString(words[0]);
	}
    }
agpToFaOne(&agpList, agpFile, prevChrom, seqDir, lastPos, f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);

subDirs = optionVal("subDirs", cloneString(subDirs));
if (verboseLevel() > 1)
    {
    verbose(2,"#\tsubDirs: %s\n", subDirs);
    }
if (argc != 5)
    usage();
agpToFa(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
