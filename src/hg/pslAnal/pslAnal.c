/*
  File: pslAnal.c
  Author: Terry Furey
  Date: 5/2/2003
  Description: Calculates characteristics of a file of psl alignments
*/

#include "common.h"
#include "linefile.h"
#include "memalloc.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "fuzzyFind.h"
#include "genePred.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "psl.h"
#include "snp.h"
#include "fa.h"

struct pslInfo
{
    struct pslInfo *next;
    struct psl *psl;
    float pctId;
    float coverage;
    int cdsStart;
    int cdsEnd;
    int cdsSize;
    float cdsPctId;
    float cdsCoverage;
    int introns;
    int stdSplice;
    int unalignedCds;
    int singleIndel;
    int tripleIndel;
    int totalIndel;
    int snp;
    int cdsMatch;
    int cdsMismatch;
    int thirdPos;
    int loci;
    int indelCount;
    int indels[256];
} *piList = NULL;

struct hash *cdsStarts = NULL;
struct hash *cdsEnds = NULL;
struct hash *loci = NULL;
struct hash *rnaSeqs = NULL;

int indels[128];

void readCds(struct lineFile *cf)
{
int lineSize, wordCount;
char *line;
char *words[4];
char *name;
int start;
int end;

cdsStarts = newHash(16);
cdsEnds = newHash(16);

while (lineFileChopNext(cf, words, 3))
    {
    name = cloneString(words[0]);
    start = sqlUnsigned(words[1]);
    end = sqlUnsigned(words[2]);
    hashAddInt(cdsStarts, name, start);
    hashAddInt(cdsEnds, name, end);
    }
}

void readRnaSeq(char *filename)
{
struct dnaSeq *seqList, *oneSeq;
int start, end, size;

rnaSeqs = newHash(16);
seqList = faReadAllDna(filename);
for (oneSeq = seqList; oneSeq != NULL; oneSeq = oneSeq->next)
    hashAdd(rnaSeqs, oneSeq->name, oneSeq);
}

void readLoci(struct lineFile *lf)
{
int lineSize, wordCount;
char *line;
char *words[4];
char *name;
int thisLoci;

loci = newHash(16);

while (lineFileChopNext(lf, words, 2))
    {
    name = cloneString(words[0]);
    thisLoci = sqlUnsigned(words[1]);
    hashAddInt(loci, name, thisLoci);
    }
}

int countIntrons(unsigned count, unsigned *sizes, unsigned *starts)
{
int ret = 0, i;

for (i = 1; i < count; i++) 
    if (starts[i] - (starts[i-1] + sizes[i-1]) > 30)
	ret++;
return(ret);
}

int countStdSplice(struct psl *psl, DNA *seq)
{
int count=0, i;

for (i=1; i<psl->blockCount; ++i)
    {
    int iStart, iEnd, blockSize = psl->blockSizes[i-1];
    if ((psl->qStarts[i-1] + blockSize == psl->qStarts[i]) && 
	(psl->tStarts[i] - (psl->tStarts[i-1] + psl->blockSizes[i-1]) > 30))
	{
	iStart = psl->tStarts[i-1] + psl->blockSizes[i-1] - psl->tStart;
	iEnd = psl->tStarts[i] - psl->tStart;
	if (psl->strand[0] == '+')
	    {
	    if ((seq[iStart] == 'g' && seq[iStart+1] == 't' && seq[iEnd-2] == 'a' && seq[iEnd-1] == 'g') ||
		(seq[iStart] == 'g' && seq[iStart+1] == 'c' && seq[iEnd-2] == 'a' && seq[iEnd-1] == 'g'))
		count++;
	    }
	else 
	    {
	    if ((seq[iStart] == 'c' && seq[iStart+1] == 't' && seq[iEnd-2] == 'a' && seq[iEnd-1] == 'c') ||
		(seq[iStart] == 'c' && seq[iStart+1] == 't' && seq[iEnd-2] == 'g' && seq[iEnd-1] == 'c'))
		count++;
	    }
	}
    }
return(count);
}

int snpBase(struct sqlConnection *conn, char *chr, int position)
{
char query[256];
struct sqlResult *sr;
char **row;
struct snp *snp;  

sprintf(query, "select * from snpTsc where chrom = '%s' and chromStart = %d", chr, position); 
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL) 
{
  sqlFreeResult(&sr);
  return(1);
}
sprintf(query, "select * from snpNih where chrom = '%s' and chromStart = %d", chr, position); 
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL) 
{
  sqlFreeResult(&sr);
  return(1);
}

return(0);
}


void processCds(struct sqlConnection *conn, struct pslInfo *pi, DNA *rna, DNA *dna)
{
int qBInsert = 0, i, j;

pi->cdsMatch = pi->cdsMismatch = 0;
for (i = 0; i < pi->psl->blockCount; i++) 
    {
    int qstart = pi->psl->qStarts[i];
    int tstart = pi->psl->tStarts[i] - pi->psl->tStarts[0];
    for (j = 0; j < pi->psl->blockSizes[i]; j++) 
	if (((qstart+j) >= (pi->cdsStart - 1)) && ((qstart+j) < pi->cdsEnd))
	if ((char)rna[qstart+j] == (char)dna[tstart+j])
	    pi->cdsMatch++;
	else if (snpBase(conn,pi->psl->tName,tstart+j+pi->psl->tStarts[0])) 
	    {
	    pi->cdsMatch++;
	    pi->snp++;
	    }
        else
	    {
	    pi->cdsMismatch++;
	    if (((qstart+j-pi->cdsStart+1)%3)==0)
		pi->thirdPos++;
	    }
    }
pi->cdsPctId = (float)(pi->cdsMatch)/(pi->cdsMatch + pi->cdsMismatch);
pi->cdsCoverage = (float)(pi->cdsMatch + pi->cdsMismatch)/(pi->cdsSize);

for (i = 1; i < pi->psl->blockCount; i++)
    {
    int unaligned = 0;
    int end = 0;
    int qstart = pi->psl->qStarts[i]+1;
    int tstart = pi->psl->tStarts[i]+1;
    if ((qstart >= pi->cdsStart) && (qstart <= pi->cdsEnd))
	{
	end = pi->psl->qStarts[i-1] + pi->psl->blockSizes[i-1];
	if (end < (pi->cdsStart - 1))
	    end = pi->cdsStart - 1;
	unaligned = qstart - end - 1;
	if (unaligned == 1) 
	    pi->singleIndel++;
	if ((unaligned > 0) && ((unaligned%3) == 0)) 
	    pi->tripleIndel += unaligned/3;
	pi->totalIndel += unaligned;
	pi->unalignedCds += unaligned;
	if ((unaligned <= 30) && (unaligned > 0) && (pi->indelCount < 256)) 
	    {
	    pi->indels[pi->indelCount] = unaligned;
	    pi->indelCount++;
	    if (pi->indelCount > 256) 
		errAbort("Indel count too high");
	    }
	if ((unaligned <= 30) &&  (pi->cdsPctId >= 0.98) && (pi->cdsCoverage >= 0.80))
	    {
	    indels[unaligned]++;
	    }
	}

    unaligned = 0;
    end = 0;
    if ((qstart >= pi->cdsStart) && (qstart <= pi->cdsEnd))
	{
	end = pi->psl->tStarts[i-1] + pi->psl->blockSizes[i-1];
	if ((pi->psl->qStarts[i-1] + pi->psl->blockSizes[i-1]) < (pi->cdsStart - 1))
	    end += (pi->cdsStart - 1) - (pi->psl->qStarts[i-1] + pi->psl->blockSizes[i-1]);
	unaligned = tstart - end - 1;
	if ((unaligned <= 30) && (unaligned > 0)) 
	    {
	    if (unaligned == 1) 
		pi->singleIndel++;
	    if ((unaligned%3) == 0) 
		pi->tripleIndel++;
	    pi->totalIndel -= unaligned;
	    /*pi->unalignedCds += unaligned;*/
	    if ((unaligned <= 30) && (pi->indelCount < 256)) 
		{
		pi->indels[pi->indelCount] = unaligned;
		pi->indelCount++;
		if (pi->indelCount > 256) 
		    errAbort("Indel count too high");
		}
	    if ((unaligned <= 30) &&  (pi->cdsPctId >= 0.98) && (pi->cdsCoverage >= 0.80))
		{
		indels[unaligned]++;
		}
	    }
	}
    }
} 

struct pslInfo *processPsl(struct sqlConnection *conn, struct psl *psl)
{
struct pslInfo *pi;
struct dnaSeq *rnaSeq;
struct dnaSeq *dnaSeq;

AllocVar(pi);
pi->psl = psl;
pi->pctId = (float)(psl->match + psl->repMatch)/(psl->match + psl->repMatch + psl->misMatch);
pi->coverage = (float)(psl->match + psl->misMatch + psl->repMatch)/(psl->qSize);
if (!hashLookup(cdsStarts, psl->qName))
    pi->cdsStart = 1;  
else
    pi->cdsStart = hashIntVal(cdsStarts, psl->qName);
if (!hashLookup(cdsEnds, psl->qName))
    pi->cdsEnd = psl->qSize; 
else
    pi->cdsEnd = hashIntVal(cdsEnds, psl->qName);
pi->cdsSize = pi->cdsEnd - pi->cdsStart + 1;
pi->introns = countIntrons(psl->blockCount, psl->blockSizes, psl->tStarts);
pi->loci = hashIntVal(loci, psl->qName);
pi->indelCount = 0;
pi->totalIndel = 0;

rnaSeq = hashFindVal(rnaSeqs, psl->qName);
dnaSeq = hDnaFromSeq(psl->tName, psl->tStart, psl->tEnd, dnaLower);
pi->stdSplice = countStdSplice(psl, dnaSeq->dna);

if (psl->strand[0] == '-') 
    {
    reverseComplement(dnaSeq->dna, dnaSeq->size);
    pslRcBoth(pi->psl);
    }
processCds(conn, pi, rnaSeq->dna, dnaSeq->dna);
if (psl->strand[0] == '-')
    pslRcBoth(pi->psl);

/* freeDnaSeq(&rnaSeq); */
freeDnaSeq(&dnaSeq);

return(pi);
}


void doFile(struct lineFile *pf)
{
int lineSize;
char *line;
char *words[32];
int wordCount;
struct psl *psl;
struct pslInfo *pi;
struct sqlConnection *conn = hAllocConn();

while (lineFileNext(pf, &line, &lineSize))
    {
    wordCount = chopTabs(line, words);
    if (wordCount != 21)
	errAbort("Bad line %d of %s\n", pf->lineIx, pf->fileName);
    psl = pslLoad(words);
    pi = processPsl(conn, psl);
    slAddHead(&piList,pi);
    }
slReverse(&piList);
hFreeConn(&conn);
}

void writeOut(FILE *of, FILE *in)
{
struct pslInfo *pi;
int i;

fprintf(of, "Acc\tChr\tStart\tEnd\tmStart\tmEnd\tSize\tLoci\tCov\tID\tCdsStart\tCdsEnd\tCdsCov\tCdsID\tCdsMatch\tCdsMismatch\tSnp\tThirdPos\tIntrons\tStdSplice\tUnCds\tSingle\tTriple\tTotal\tIndels\n");
fprintf(of, "10\t10\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10\n");
for (pi = piList; pi != NULL; pi = pi->next)
    {
    fprintf(of, "%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t", pi->psl->qName, pi->psl->tName, pi->psl->tStart, 
	    pi->psl->tEnd,pi->psl->qStart,pi->psl->qEnd,pi->psl->qSize,pi->loci);
    fprintf(of, "%.4f\t%.4f\t%d\t%d\t%.4f\t%.4f\t%d\t%d\t%d\t%d\t%d\t%d\t", 
	    pi->coverage, pi->pctId, pi->cdsStart, 
	    pi->cdsEnd, pi->cdsCoverage, pi->cdsPctId, pi->cdsMatch, 
	    pi->cdsMismatch, pi->snp, pi->thirdPos, pi->introns, pi->stdSplice);
    fprintf(of, "%d\t%d\t%d\t%d\t", pi->unalignedCds, pi->singleIndel, pi->tripleIndel, pi->totalIndel);
    for (i = 0; i < pi->indelCount; i++)
	fprintf(of, "%d,", pi->indels[i]);
    fprintf(of, "\n");
    }
for (i = 1; i <= 100; i++) 
    fprintf(in, "%d\t%d\n",i,indels[i]);
}

int main(int argc, char *argv[])
{
struct lineFile *pf, *cf, *lf;
FILE *of, *in;
char *faFile;

if (argc < 6)
    {
    fprintf(stderr, "USAGE: pslAnal <psl file> <cds file> <loci file> <fa file> <out file name> <indel file>\n");
    return 1;
    }

pf = pslFileOpen(argv[1]);
cf = lineFileOpen(argv[2], FALSE);
lf = lineFileOpen(argv[3], FALSE);
faFile = argv[4];
of = mustOpen(argv[5], "w");
in = mustOpen(argv[6], "w");

hSetDb("hg15");
readCds(cf);
readRnaSeq(faFile);
readLoci(lf);
doFile(pf);
writeOut(of, in);

lineFileClose(&pf);
fclose(of);
fclose(in);

return 0;
}
