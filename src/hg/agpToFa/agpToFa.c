/* agpToFa - Convert a .agp file to a .fa file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "agpFrag.h"
#include "agpGap.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpToFa - Convert a .agp file to a .fa file\n"
  "usage:\n"
  "   agpToFa in.agp agpSeq out.fa freezeDir\n"
  "Where agpSeq matches a sequence name in in.agp\n"
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
  );
}


/* Overridable options. */
char *subDirs = "extras,fin,draft,predraft";

void simpleMultiFillInSequence(char *seqFile, struct agpFrag *agpList,
    DNA *dna, int dnaSize)
/* Fill in DNA array with sequences from one multi-record fasta file. */
{
struct hash *fragHash = newHash(17);
struct agpFrag *agp;
struct dnaSeq *seq;
FILE *f;

f = mustOpen(seqFile, "r");
while (faReadNext(f, "bogus", TRUE, NULL, &seq))
    {
    hashAdd(fragHash, seq->name, seq);
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
    memcpy(dna + agp->chromStart, seq->dna + agp->fragStart, size);
    }
}

void simpleFillInSequence(char *seqDir, struct agpFrag *agpList,
    DNA *dna, int dnaSize)
/* Fill in DNA array with sequences from simple clones. */
{
struct agpFrag *agp;

for (agp = agpList; agp != NULL; agp = agp->next)
    {
    char clone[128];
    char path[512];
    struct dnaSeq *seq;
    int size;
    strcpy(clone, agp->frag);
    chopSuffix(clone);
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

void getCloneDna(struct clone *clone, struct hash *fragHash)
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

void readTrans(char *transFile, char *faDir, struct hash *cloneHash, struct hash *fragHash)
/* Read in transFile into hashes. */
{
struct lineFile *lf = lineFileOpen(transFile, TRUE);
char *row[3];
char *s, *e, *parts[3], *subParts[2];
int i, partCount, subCount;
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

void gsFillInSequence(char *gsDir, struct agpFrag *agpList, DNA *dna, int dnaSize)
/* Fill in DNA array with sequences from 'freeze' */
{
struct agpFrag *agp;
int subCount;
char *sub[64];
char transFile[512], faDir[512];
struct hash *cloneHash = newHash(15), *fragHash = newHash(17);
int i;
struct clone *clone;
struct frag *frag;

/* Read in trans-files to hash */
subCount = chopString(subDirs, ",", sub, ArraySize(sub));
if (subCount >= ArraySize(sub))
    errAbort("Too many subDirs.");
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

void agpToFa(char *agpFile, char *agpSeq, char *faOut, char *seqDir)
/* agpToFa - Convert a .agp file to a .fa file. */
{
struct lineFile *lf = lineFileOpen(agpFile, TRUE);
char *line, *words[16];
int lineSize, wordCount;
int lastPos = 0;
struct agpFrag *agpList = NULL, *agp;
DNA *dna;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#' || line[0] == '\n')
        continue;
    wordCount = chopLine(line, words);
    if (wordCount < 5)
        errAbort("Bad line %d of %s\n", lf->lineIx, lf->fileName);
    if (words[4][0] != 'N')
	{
	lineFileExpectWords(lf, 9, wordCount);
	if (sameWord(words[0], agpSeq))
	    {
	    agp = agpFragLoad(words);
	    if (agp->chromStart != lastPos)
		errAbort("Start doesn't match previous end line %d of %s\n", lf->lineIx, lf->fileName);
	    if (agp->chromEnd - agp->chromStart != agp->fragEnd - agp->fragStart)
		errAbort("Sizes don't match in %s and %s line %d of %s\n",
		    agp->chrom, agp->frag, lf->lineIx, lf->fileName);
	    slAddHead(&agpList, agp);
	    lastPos = agp->chromEnd;
	    }
	}
    else
        {
	lastPos = lineFileNeedNum(lf, words, 2);
	}
    }
slReverse(&agpList);
if (lastPos == 0)
    errAbort("%s not found in %s\n", agpSeq, agpFile);
dna = needLargeMem(lastPos+1);
memset(dna, 'n', lastPos);
dna[lastPos] = 0;
if (cgiVarExists("simpleMulti"))
    {
    simpleMultiFillInSequence(seqDir, agpList, dna, lastPos);
    }
else if (cgiVarExists("simple"))
    {
    simpleFillInSequence(seqDir, agpList, dna, lastPos);
    }
else
    {
    gsFillInSequence(seqDir, agpList, dna, lastPos);
    }
printf("Writing %d bases to %s\n", lastPos, faOut);
faWrite(faOut, agpSeq, dna, lastPos);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
subDirs = cgiUsualString("subDirs", cloneString(subDirs));
if (argc != 5)
    usage();
agpToFa(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
