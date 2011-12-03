/* orfStats - Collect stats on orfs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "orfStats - Collect stats on orfs\n"
  "usage:\n"
  "   orfStats refSeq.fa refSeq.cds orf.stats\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct cdsInfo
/* Cds position and related stuff. */
    {
    struct cdsInfo *next;
    char *name;		/* Allocated in hash */
    int size;		/* Size of mRNA */
    int cdsStart;	/* Start of coding. */
    int cdsEnd;		/* End of coding. */
    };

struct hash *readCdsFile(char *fileName)
/* Read in file into a hash of cdsInfo. */
{
if (fileName == NULL)
    return NULL;
else
    {
    struct hash *hash = newHash(16);
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    char *row[4];
    while (lineFileRow(lf, row))
	{
	struct cdsInfo *cds;
	AllocVar(cds);
	hashAddSaveName(hash, row[0], cds, &cds->name);
	cds->size = lineFileNeedNum(lf, row, 1);
	cds->cdsStart = lineFileNeedNum(lf, row, 2);
	cds->cdsEnd = lineFileNeedNum(lf, row, 3);
	}
    return hash;
    }
}

void addPairs(int m[], DNA *dna, int start, int end)
/* Add counts of pairs to matrix. */
{
int i;
int prev, cur;
if (start == 0) start = 1;
prev = ntVal[(int)dna[start]];
for (i=start+1; i<end; ++i)
    {
    cur = ntVal[(int)dna[i]];
    if (prev >= 0 && cur >= 0)
       {
       int ix = (prev*4 + cur);
       m[ix] += 1;
       }
    prev = cur;
    }
}

void addCodons(int m[], DNA *dna, int start, int end)
/* Add codon counts. */
{
int i;
int c1,c2,c3;

end -= 2;
for (i=start; i<end; i += 3)
    {
    c1 = ntVal[(int)dna[i]];
    c2 = ntVal[(int)dna[i+1]];
    c3 = ntVal[(int)dna[i+2]];
    if (c1 >= 0 && c2 >= 0 && c3 >= 0)
        {
	int ix = c1*4*4 + c2*4 + c3;
	m[ix] += 1;
	}
    }
}

void saveMarkov1(int m[], char *label, FILE *f)
/* Save 1st order markov model based on counts. */
{
int i, j, ix;
fprintf(f, "%s\n", label);
for (i=0; i<4; ++i)
    {
    int rowCount = 0;
    double rowScale;
    ix = i*4;
    for (j=0; j<4; ++j)
        {
	rowCount += m[ix];
	++ix;
	}
    if (rowCount > 0)
        rowScale = 1.0/rowCount;
    else
        rowScale = 0;
    ix = i*4;
    fprintf(f, "%c", valToNt[i]);
    for (j=0; j<4; ++j)
        {
	fprintf(f, " %f", m[ix] * rowScale);
	++ix;
	}
    fprintf(f, "\n");
    }
}

void killCodon(int m[], char *codon)
/* Set codon count to zero */
{
int ix = ntVal[(int)codon[0]]*4*4 + ntVal[(int)codon[1]]*4 + ntVal[(int)codon[2]];
m[ix] = 0;
}

void saveCodons(int m[], char *label, FILE *f)
/* Save codon frequencies. */
{
int total = 0;
double scale;
int i,j,k, ix;

fprintf(f, "%s\n", label);
for (i=0; i<4*4*4; ++i)
    total += m[i];
if (total > 0)
    scale = 1.0/total;
else
    scale = 0;
ix = 0;
for (i=0; i<4; ++i)
    for (j=0; j<4; ++j)
        for (k=0; k<4; ++k)
	    {
	    fprintf(f, "%c%c%c %f\n", valToNt[i], valToNt[j], valToNt[k], scale*m[ix]);
	    ++ix;
	    }
}

void saveKozak(int kozak[10][4], char *label, FILE *f)
/* Save kozak consensus. */
{
int i,j;
fprintf(f, "%s\n", label);
fprintf(f, "#");
for (i=0; i<4; ++i)
    fprintf(f, "    %c    ", valToNt[i]);
fprintf(f, "\n");
for (i=0; i<10; ++i)
    {
    int rowTotal = 0;
    double rowScale;
    for (j=0; j<4; ++j)
	rowTotal += kozak[i][j];
    rowScale = 1.0/rowTotal;
    for (j=0; j<4; ++j)
	fprintf(f, "%f ", rowScale * kozak[i][j]);
    fprintf(f, "\n");
    }
}

void orfStats(char *faFile, char *cdsFile, char *outFile)
/* orfStats - Collect stats on orfs. */
{
struct hash *cdsHash = readCdsFile(cdsFile);
struct lineFile *lf = lineFileOpen(faFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct dnaSeq seq;
static int codons[4*4*4];
static int utr5[4*4], utr3[4*4], kozak[10][4];

ZeroVar(&seq);
while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    struct cdsInfo *cds = hashMustFindVal(cdsHash, seq.name);
    int cdsStart = cds->cdsStart;
    if (cdsStart >= 5 && seq.dna[cdsStart] == 'a' 
    	&& seq.dna[cdsStart+1] == 't' && seq.dna[cdsStart+2] == 'g')
        {
	int i, ix;
	addPairs(utr5, seq.dna, 0, cdsStart);
	addPairs(utr3, seq.dna, cds->cdsEnd, seq.size);
	addCodons(codons, seq.dna, cdsStart+3, cds->cdsEnd-3);
	for (i=0; i<10; ++i)
	   {
	   int val;
	   ix = i + cds->cdsStart - 5;
	   val = ntVal[(int)(seq.dna[ix])];
	   if (val >= 0)
	       kozak[i][val] += 1;
	   }
	}
    }
saveMarkov1(utr5, "utr5", f);
fprintf(f, "\n");
saveKozak(kozak, "kozak", f);
fprintf(f, "\n");
saveMarkov1(utr3, "utr3", f);
fprintf(f, "\n");
killCodon(codons, "tag");
killCodon(codons, "taa");
killCodon(codons, "tga");
saveCodons(codons, "codons", f);
fprintf(f, "\n");
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
dnaUtilOpen();
if (argc != 4)
    usage();
orfStats(argv[1], argv[2], argv[3]);
return 0;
}
