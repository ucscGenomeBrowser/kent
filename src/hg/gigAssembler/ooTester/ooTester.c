/* ooTester - generate test case for ooGreedy. */

#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"


struct dnaSeq *randomSeq(int size)
/* Make up a random sequence of given size. */
{
struct dnaSeq *seq;
DNA *dna;
int i;
int base;


dnaUtilOpen();
AllocVar(seq);
seq->size = size;
seq->dna = dna = needLargeMem(size);
seq->name = cloneString("random");
for (i=0; i<size; ++i)
    {
    base = rand()&3;
    dna[i] = valToNt[base];
    }
return seq;
}

void shredFrags(FILE *f, char *cloneName, DNA *dna, int raftSize, int fragsPer,
	boolean rcRaft, int raftStart, int raftEnd)
/* Shred a raft into fragments. */
{
char fragName[512];
int domains = fragsPer+1;
int i;
boolean rcFrag;


for (i=0; i<fragsPer; ++i)
    {
    int start = i*raftSize/domains;
    int end = (i+2)*raftSize/domains;
    int size = end-start;
    DNA *frag;
    rcFrag = (i&1);
    if (rcRaft)
	rcFrag = !rcFrag;
    rcFrag = (rand()&1);	/* uglyf */
    sprintf(fragName, "%s.%c_%d_%d", cloneName, 
    	(rcFrag ? 'r' : 'f'),
	raftStart+start, raftStart+end);
    frag = dna+start;
    if (rcFrag)
	reverseComplement(frag, size);
    faWriteNext(f, fragName, frag, size);
    if (rcFrag)
	reverseComplement(frag, size);
    }
}

void shredSeq(char *outName, char *cloneName, 
	struct dnaSeq *whole, int raftCount, int fragsPer)
/* Shred whole sequence into raftCount non-overlapping pieces each of which has
 * fragsPer pieces. */
{
int wholeSize = whole->size;
DNA *wholeDna = whole->dna;
FILE *f = mustOpen(outName, "w");
int i;
boolean rcRaft;

for (i=0; i<raftCount; ++i)
    {
    int start = i*wholeSize/raftCount;
    int end = (i+1)*wholeSize/raftCount;
    int size = end-start;
    rcRaft = ((i>>1)&1);
    shredFrags(f, cloneName, wholeDna+start, size, fragsPer, rcRaft, start, end);
    }
fclose(f);
}

void makeRna(FILE *f, struct dnaSeq *seq, int startOff, int exonSize, 
	int intronSize, int exonCount, boolean isRc)
/* Make some dummy RNA and write to file. */
{
char rnaName[512];
int realExonCount = 0;
int seqSize = seq->size;
int i;
int pos = startOff;
DNA *rna, *pt;
int rnaSize;

isRc = (rand()&1);	/* uglyf */
for (i=0; i<exonCount; ++i)
    {
    pos += exonSize;
    if (pos >= seqSize)
	break;
    ++realExonCount;
    pos += intronSize;
    }
if (realExonCount <= 0)
    return;
rnaSize = realExonCount * exonSize;
rna = pt = needMem(rnaSize+1);

pos = startOff;
for (i=0; i<realExonCount; ++i)
    {
    memcpy(pt, seq->dna + pos, exonSize);
    pt += exonSize;
    pos += exonSize + intronSize;
    }
if (isRc)
    reverseComplement(rna, rnaSize);

sprintf(rnaName, "rna%c_%d_%d_%d", 
	(isRc ? 'R' : 'F'),
	startOff, exonSize, intronSize, exonCount);
faWriteNext(f, rnaName, rna, rnaSize);

freeMem(rna);
}

void fakeRna(char *fileName, struct dnaSeq *seq, int startOff,
	int exonSize, int intronSize, int igSize, int exonCount)
/* Write fake RNA sequence to file. */
{
FILE *f = mustOpen(fileName, "w");
int rnaSize = exonCount * exonSize + (exonCount-1)*intronSize;
int seqSize = seq->size;
int pos;
boolean isRc = FALSE;

for (pos = startOff; pos+rnaSize <= seqSize; pos += rnaSize+igSize)
    {
    makeRna(f, seq, pos, exonSize, intronSize, exonCount, isRc);
    isRc = !isRc;
    }
fclose(f);
}

void fakeOneBacEndPair(FILE *fa, FILE *pair, DNA *sDna, DNA *eDna, int endSize, 
	boolean isRc, int ix)
/* Write one fake bac end pair. */
{
char names[2][256];

sprintf(names[isRc], "be_f_%d", ix);
sprintf(names[1-isRc], "be_r_%d", ix);
fprintf(pair, "%s %s\n", names[0], names[1]);
faWriteNext(fa, names[0], sDna, endSize);
reverseComplement(eDna, endSize);
faWriteNext(fa, names[1], eDna, endSize);
reverseComplement(eDna, endSize);
}

void fakeBac(char *faName, char *pairName, struct dnaSeq *seq, 
	int start, int size, int nextPos)
/* Fake a set of fake bac ends. */
{
FILE *faFile = mustOpen(faName, "w");
FILE *pairFile = mustOpen(pairName, "w");
boolean isRc = FALSE;
int pos;
int bacSize = 100;
int seqSize = seq->size;
DNA *dna = seq->dna;
int i = 1;

for (pos = start; pos+size <= seqSize; pos += nextPos)
    {
    isRc = (rand()&1);  /* uglyf */
    if (isRc)
	reverseComplement(dna+pos, size);
    fakeOneBacEndPair(faFile, pairFile, 
    	dna+pos, dna+pos+size-bacSize, bacSize, isRc, i);
    if (isRc)
	reverseComplement(dna+pos, size);
    ++i;
    isRc = !isRc;
    }
fclose(faFile);
fclose(pairFile);
}

void writeLineToFile(char *fileName, char *line)
/* Write single line to file. */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "%s\n", line);
fclose(f);
}

void fakeInfo(char *fileName, char *contigName, char *cloneName)
/* Write out fake info file. */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "%s ORDERED\n", contigName);
fprintf(f, "%s 0\n", cloneName);
fclose(f);
}

void ooTester(char *testName)
/* Make up test set. */
{
struct dnaSeq *seq;
char fileName[512];

mkdir(testName,0775);
chdir(testName);
seq = randomSeq(10000);
faWrite("orig.fa", testName, seq->dna, seq->size);
shredSeq("x.fa", "x", seq, 10, 5);
fakeRna("mrna.fa", seq, 0, 100, 300, 400, 4);
fakeRna("est.fa", seq, 50, 100, 300, 300, 2);
fakeBac("bacEnds.fa", "bacEndPairs", seq, 200, 2000, 600);
fakeInfo("info", testName, "x");
writeLineToFile("geno.lst", "x.fa");
writeLineToFile("mrna.lst", "mrna.fa");
writeLineToFile("est.lst", "est.fa");
writeLineToFile("bacEnds.lst", "bacEnds.fa");
}

void usage()
/* Explain usage and exit. */
{
errAbort("ooTester - generate a test set for ooGreedy\n"
         "usage\n"
	 "    ooTester testName\n"
	 "This will create the directory testName and populate\n"
	 "it with a test set\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
ooTester(argv[1]);
}

