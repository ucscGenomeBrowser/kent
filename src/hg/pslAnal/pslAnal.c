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
#include "psl.h"

struct acc
{
  struct acc *next;
  char *name;
};

struct indel
{
  struct indel *next;
  int size;
  char *chrom;
  int chromStart;
  int chromEnd;
  char *mrna;
  int mrnaStart;
  int mrnaEnd;
  int genMrna;
  struct acc *genMrnaAcc;
  int genEst;
  struct acc *genEstAcc;
  int mrnaMrna;
  struct acc *mrnaMrnaAcc;
  int mrnaEst;
  struct acc *mrnaEstAcc;
  int noMrna;
  struct acc *noMrnaAcc;
  int noEst;
  struct acc *noEstAcc;
};

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
  int cdsMatch;
  int cdsMismatch;
  int cdsGap;
  int unalignedCds;
  int singleIndel;
  int tripleIndel;
  int totalIndel;
  int indelCount;
  int indels[256];
  struct indel *indelList;
  int snp;
  int thirdPos;
  int loci;
} *piList = NULL;

struct hash *cdsStarts = NULL;
struct hash *cdsEnds = NULL;
struct hash *loci = NULL;
struct hash *rnaSeqs = NULL;

int indels[128];

void readCds(struct lineFile *cf)
/* Read in file of coding region starts and stops 
   Convert start to 0-based to make copmarison with psl easier */
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
    start = sqlUnsigned(words[1]) - 1;
    end = sqlUnsigned(words[2]);
    hashAddInt(cdsStarts, name, start);
    hashAddInt(cdsEnds, name, end);
    }
}

void readRnaSeq(char *filename)
/* Read in file of mRNA fa sequences */
{
struct dnaSeq *seqList, *oneSeq;
int start, end, size;

rnaSeqs = newHash(16);
seqList = faReadAllDna(filename);
for (oneSeq = seqList; oneSeq != NULL; oneSeq = oneSeq->next)
    hashAdd(rnaSeqs, oneSeq->name, oneSeq);
}

void readLoci(struct lineFile *lf)
/* Read in file of loci id's, primarily from LocusLink */
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
/* Count the number of introns in an alignment where an intron is
   defined as a gap greater than 30 bases */
{
int ret = 0, i;

for (i = 1; i < count; i++) 
    if (starts[i] - (starts[i-1] + sizes[i-1]) > 30)
	ret++;
return(ret);
}

int countStdSplice(struct psl *psl, DNA *seq)
/* For each intron, determine whether it has a canonical splice site
   Return the number of introns that do */
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
/* Determine whether a given position is known to be a SNP */ 
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

void cdsCompare(struct sqlConnection *conn, struct pslInfo *pi, struct dnaSeq *rna, struct dnaSeq *dna)
/* Compare the alignment of the coding region of an mRNA to the genomic sequence */
{
int i,j;
DNA *r, *d; 

r = rna->dna;
d = dna->dna;
pi->cdsMatch = pi->cdsMismatch = 0;
/* Look at all alignment blocks */
for (i = 0; i < pi->psl->blockCount; i++) 
    {
    int qstart = pi->psl->qStarts[i];
    int tstart = pi->psl->tStarts[i] - pi->psl->tStarts[0];
    /* Compare each base */
    for (j = 0; j < pi->psl->blockSizes[i]; j++) 
      /* Check if it is in the coding region */
	if (((qstart+j) >= pi->cdsStart) && ((qstart+j) < pi->cdsEnd))
	  /* Bases match */
	  if ((char)r[qstart+j] == (char)d[tstart+j])
	    pi->cdsMatch++;
          /* Check if mismatch is due to a SNP */
	  else if (snpBase(conn,pi->psl->tName,tstart+j+pi->psl->tStarts[0])) 
	    {
	      pi->cdsMatch++;
	      pi->snp++;
	    }
	  else
	    {
	      pi->cdsMismatch++;
	      /* Check if mismatch is in a codon wobble position */
	      if (((qstart+j-pi->cdsStart+1)%3)==0)
		pi->thirdPos++;
	    }
    }
}

struct dnaSeq *getDna(struct psl *psl, int gstart, int gend, int *start, int *end, char *strand)
/* Get the genomic DNA that corresponds to an indel, and determine the corresponding \
   start and end positions for this sequence in the query sequence */ 
{
struct dnaSeq *ret;
int gs, ge, off, i;

/* Look in all blocks for the indel */
for (i = 0; i < psl->blockCount; i++) 
    {
    /* If the block contains the indel */   
    if (((psl->tStarts[i] + psl->blockSizes[i]) >= gstart) && (psl->tStarts[i] <= gend))
       {
       /* Determine the start position offset */
       if (gstart > psl->tStarts[i])
	  {
	  *start = psl->qStarts[i] + (gstart - psl->tStarts[i]);
	  gs = gstart;
	  } 
       else 
	  gs = psl->tStarts[i];
       /* Determine the end position offset */
       if (gend < (psl->tStarts[i]+psl->blockSizes[i]))
	  {
	  *end = psl->qStarts[i] + gend - psl->tStarts[i];
	   ge = gend;
	  }
       else
	  ge = psl->tStarts[i]+psl->blockSizes[i];
	}
    }
/*printf("Extracting %s:%d-%d of %d-%d\n",psl->tName, gstart, gend, gstart, gend);*/ 
ret = hDnaFromSeq(psl->tName, gstart, gend, dnaLower);
/* If opposite strand alignment, reverse the start and end positions in the mRNA */
  if (psl->strand[0] == '-') 
    {
      int tmp = *start;
      *start = psl->qSize - *end;
      *end = psl->qSize - tmp;
      reverseComplement(ret->dna, ret->size);
    }
strcpy(strand, psl->strand); 
return(ret);
}

void searchTrans(struct sqlConnection *conn, char *table, char *mrnaName, struct dnaSeq *rna, struct indel *ni, char *strand)
/* Find all mRNA's or EST's that align in the region of an indel */
{
  struct sqlResult *sr;
  char **row;
  int offset, start=0, end=0;
  struct psl *psl;
  struct dnaSeq *seq, *gseq, *mseq;
  struct acc *acc;
  char thisStrand[2];

  /* Determine the indel sequence, plus one base on each side */
  AllocVar(mseq);
  mseq->dna = rna->dna + ni->mrnaStart - 2;
  *(mseq->dna+ni->mrnaEnd-ni->mrnaStart+4) = '\0';
  mseq->size = ni->mrnaEnd - ni->mrnaStart + 4;
  /*printf("Indel at %d-%d (%d) in %s:%d-%d bases %s\n", ni->mrnaStart, ni->mrnaEnd, rna->size, ni->chrom, ni->chromStart, ni->chromEnd, mseq->dna);*/
  /* Find all sequences that span this region */
  sr = hRangeQuery(conn, table, ni->chrom, ni->chromStart-2, ni->chromEnd+2, NULL, &offset);
  while ((row = sqlNextRow(sr)) != NULL) 
    {
      psl = pslLoad(row+offset);
      if (strcmp(psl->qName,mrnaName))
         {
         start = end = 0;
         /* Get the DNA sequence for the indel */
         gseq = getDna(psl, ni->chromStart-2, ni->chromEnd+2, &start, &end, thisStrand);
         if (gseq->dna) 
	     {
	     /* Get the corresponding mRNA or EST  sequence */
	     seq = hRnaSeq(psl->qName);
	     seq->dna = seq->dna + start;
	     *(seq->dna+end-start) = '\0';
	     seq->size = end - start;
	     if (thisStrand[0] != strand[0])
	        {
	        reverseComplement(seq->dna, seq->size);
	        reverseComplement(gseq->dna, gseq->size);
	        }
	     /*printf("Comparing %s:%d-%d(%d) bases %s with genomic %s\n",psl->qName, start, end, seq->size, seq->dna, gseq->dna);*/
	     AllocVar(acc);
	     acc->name = seq->name;
	     if (!strcmp(gseq->dna, seq->dna)) 
	        if (!strcmp(table, "mrna"))
	        {
		ni->genMrna++;
		slAddHead(&ni->genMrnaAcc, acc);
		} 
	        else 
		{
		    ni->genEst++;
		    slAddHead(&ni->genEstAcc, acc);
		}
	     else  
	       if (!strcmp(mseq->dna, seq->dna)) 
		 if (!strcmp(table, "mrna"))
		   {
		     ni->mrnaMrna++;
		     slAddHead(&ni->mrnaMrnaAcc, acc);
		   } 
		 else 
		   {
		     ni->mrnaEst++;
		     slAddHead(&ni->mrnaEstAcc, acc);
		   }
	       else
		 if (!strcmp(table, "mrna"))
		   {
		     ni->noMrna++;
		     slAddHead(&ni->noMrnaAcc, acc);
		   } 
		 else 
		   {
		     ni->noEst++;
		     slAddHead(&ni->noEstAcc, acc);
		   }
	     }
	 }
      pslFree(&psl);
    }
  sqlFreeResult(&sr);
}

struct indel *createIndel(struct sqlConnection *conn, char *mrna, int mstart, int mend, char* chr, int gstart, int gend, 
			  struct dnaSeq *rna, char *strand)
/* Create a record of an indel */
{
  struct indel *ni;
 
  AllocVar(ni);
  if ((mend - mstart) > 0) 
    ni->size = mend - mstart;
  else
    ni->size = gend - gstart;
  ni->chrom = chr;
  ni->chromStart = gstart;
  ni->chromEnd = gend;
  ni->mrna = mrna;
  ni->mrnaStart = mstart;
  ni->mrnaEnd = mend;
  ni->genMrna = 0;
  ni->genMrnaAcc = NULL;
  ni->genEst = 0;
  ni->genEstAcc = NULL;
  ni->mrnaMrna = 0;
  ni->mrnaMrnaAcc = NULL;
  ni->mrnaEst = 0;
  ni->mrnaEstAcc = NULL;
  ni->noMrna = 0;
  ni->noMrnaAcc = NULL;
  ni->noEst = 0;
  ni->noEstAcc = NULL;
  
  /* Determine whether mRNAs and ESTs support genomic or mRNA sequence in indel region */
  searchTrans(conn, "mrna", mrna, rna, ni, strand);
  searchTrans(conn, "est", mrna, rna, ni, strand);

  return(ni);
}

void cdsIndels(struct sqlConnection *conn, struct pslInfo *pi, struct dnaSeq *rna)
/* Find indels in coding region of mRNA alignment */
{
  int i, j;
  int unaligned=0, prevqend=0, prevtend=0, qstart=0, tstart=0;
  struct indel *ni, *niList=NULL;
  int cdsS = pi->cdsStart, cdsE = pi->cdsEnd;
  
  /* Check all blocks for indels */
  for (i = 1; i < pi->psl->blockCount; i++)
  {  
    /* Find insertions in the mRNA sequence */
    unaligned = 0;
    prevqend = 0;
    prevtend = 0;
    qstart = pi->psl->qStarts[i];
    tstart = pi->psl->tStarts[i];
    /* If block is in the cds region */
    if ((qstart >= cdsS) && (qstart <= cdsE))
      {
        /* Determine where previous block left off in the alignment */
	prevqend = pi->psl->qStarts[i-1] + pi->psl->blockSizes[i-1];
	prevtend = pi->psl->tStarts[i-1] + pi->psl->blockSizes[i-1];
	/* Adjust if previous end is not in coding region */
	if (prevqend < pi->cdsStart)
	  {
	    int diff = cdsS - prevqend;
	    prevqend += diff;
	    prevtend += diff;
	  }
	unaligned = qstart - prevqend;
	/* Check if unaligned part is a gap in the mRNA alignment, not an insertion */
	if (unaligned > 30)
	  pi->cdsGap += unaligned;
	/* Check if there is an indel */
	else if (unaligned > 0)
	  {
	    if (unaligned == 1) 
	      pi->singleIndel++;
	    if ((unaligned%3) == 0) 
	      pi->tripleIndel += unaligned/3;
	    pi->totalIndel += unaligned;
	    pi->unalignedCds += unaligned;
	    pi->indels[pi->indelCount] = unaligned;
	    pi->indelCount++;
	    if ((pi->cdsPctId >= 0.98) && (pi->cdsCoverage >= 0.80))
	      indels[unaligned]++;
	    if (pi->indelCount > 256) 
	      errAbort("Indel count too high");
	    if (pi->psl->strand[0] == '-') {
	      int temp = tstart;
	      tstart = pi->psl->tSize - prevtend;
	      prevtend = pi->psl->tSize - temp;
	    }
            /* Create an indel record for this */
	    ni = createIndel(conn, pi->psl->qName, prevqend, qstart, pi->psl->tName, prevtend, tstart, rna, pi->psl->strand); 
	    slAddHead(&niList,ni);
	  }
      }
    
    /* Find deletions in the mRNA sequence */
    unaligned = 0;
    prevqend = 0;
    prevtend = 0;
    /* Check if in the coding region */
    if ((qstart >= cdsS) && (qstart <= cdsE))
      {
        /* Determine where previous block left off in the alignment */
	prevqend = pi->psl->qStarts[i-1] + pi->psl->blockSizes[i-1];
	prevtend = pi->psl->tStarts[i-1] + pi->psl->blockSizes[i-1];
	/* Adjust if previous end is not in coding region */
	if (prevqend < pi->cdsStart)
	  {
	    int diff = cdsS - prevqend;
	    prevqend += diff;
	    prevtend += diff;
	  }
	unaligned = tstart - prevtend;
	/* Check if unaligned part is an intron */
	if (unaligned > 30)
	  pi->cdsGap += unaligned;
	/* Check if there is an indel */
	else if (unaligned != 0) 
	  {
	    if (unaligned == 1) 
	      pi->singleIndel++;
	    if ((unaligned%3) == 0) 
	      pi->tripleIndel++;
	    pi->totalIndel -= unaligned;
	    pi->indels[pi->indelCount] = unaligned;
	    pi->indelCount++;
	    if (pi->indelCount > 256) 
	      errAbort("Indel count too high");
	    if ((pi->cdsPctId >= 0.98) && (pi->cdsCoverage >= 0.80))
	      indels[unaligned]++;
	    if (pi->psl->strand[0] == '-') {
	      int temp = tstart;
	      tstart = pi->psl->tSize - prevtend;
	      prevtend = pi->psl->tSize - temp;
	    }
	    /* Create an indel record for this */
	    ni = createIndel(conn, pi->psl->qName, prevqend, qstart, pi->psl->tName, prevtend, tstart, rna, pi->psl->strand); 
	    slAddHead(&niList,ni);
	  }
      }
  }
  slReverse(&niList);
  pi->indelList = niList;
}

void processCds(struct sqlConnection *conn, struct pslInfo *pi, struct dnaSeq *rna, struct dnaSeq *dna)
/* Examine closely the alignment of the coding region */
{
  printf("Processing %s\n", pi->psl->qName);
  /* Compare the actual aligned parts */
  cdsCompare(conn, pi, rna, dna);
  pi->cdsPctId = (float)(pi->cdsMatch)/(pi->cdsMatch + pi->cdsMismatch);
  pi->cdsCoverage = (float)(pi->cdsMatch + pi->cdsMismatch)/(pi->cdsSize);
  /* Determine indels in the alignment */
  cdsIndels(conn, pi, rna);
} 

struct pslInfo *processPsl(struct sqlConnection *conn, struct psl *psl)
/* Analyze an alignment of a mRNA to the genomic sequence */
{
struct pslInfo *pi;
struct dnaSeq *rnaSeq;
struct dnaSeq *dnaSeq;

AllocVar(pi);
pi->psl = psl;
pi->pctId = (float)(psl->match + psl->repMatch)/(psl->match + psl->repMatch + psl->misMatch);
pi->coverage = (float)(psl->match + psl->misMatch + psl->repMatch)/(psl->qSize);
if (!hashLookup(cdsStarts, psl->qName))
    pi->cdsStart = 0;  
else
    pi->cdsStart = hashIntVal(cdsStarts, psl->qName);
if (!hashLookup(cdsEnds, psl->qName))
    pi->cdsEnd = psl->qSize; 
else
    pi->cdsEnd = hashIntVal(cdsEnds, psl->qName);
pi->cdsSize = pi->cdsEnd - pi->cdsStart;
pi->introns = countIntrons(psl->blockCount, psl->blockSizes, psl->tStarts);
pi->loci = hashIntVal(loci, psl->qName);
pi->indelCount = 0;
pi->totalIndel = 0;

/* Get the corresponding sequences */
rnaSeq = hashFindVal(rnaSeqs, psl->qName);
dnaSeq = hDnaFromSeq(psl->tName, psl->tStart, psl->tEnd, dnaLower);
pi->stdSplice = countStdSplice(psl, dnaSeq->dna);

/* Reverse compliment genomic and psl record if aligned on opposite strand */
if (psl->strand[0] == '-') 
  {
   reverseComplement(dnaSeq->dna, dnaSeq->size);
   pslRcBoth(pi->psl);
  }

/* Analyze the coding region */
processCds(conn, pi, rnaSeq, dnaSeq);

freeDnaSeq(&dnaSeq);
return(pi);
}


void doFile(struct lineFile *pf)
/* Process all records in a psl file of mRNA alignments */
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
/* Output results of the mRNA alignment analysis */
{
struct pslInfo *pi;
 struct indel *indel;
 struct acc *acc;
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

    printf("Writing out indels\n");
    for (indel = pi->indelList; indel != NULL; indel=indel->next)
      {
	printf("Indel of size %d in %s:%d-%d vs. %s:%d-%d\n",
	       indel->size, indel->mrna, indel->mrnaStart, indel->mrnaEnd,
	       indel->chrom, indel->chromStart, indel->chromEnd);
	fprintf(in, "Indel of size %d in %s:%d-%d vs. %s:%d-%d\n",
		indel->size, indel->mrna, indel->mrnaStart, indel->mrnaEnd,
		indel->chrom, indel->chromStart, indel->chromEnd);
	fprintf(in, "\t%d mRNAs support genomic: ", indel->genMrna);
	for (acc = indel->genMrnaAcc; acc != NULL; acc = acc->next)
	  fprintf(in, "%s ", acc->name);
	fprintf(in, "\n\t%d ESTs support genomic: ",indel->genEst);
	for (acc = indel->genEstAcc; acc != NULL; acc = acc->next)
	  fprintf(in, "%s ", acc->name);
	fprintf(in, "\n\t%d mRNAs support %s: ", indel->mrnaMrna, indel->mrna);
	for (acc = indel->mrnaMrnaAcc; acc != NULL; acc = acc->next)
	  fprintf(in, "%s ", acc->name);
	fprintf(in, "\n\t%d ESTs support %s: ",indel->mrnaEst, indel->mrna);
	for (acc = indel->mrnaEstAcc; acc != NULL; acc = acc->next)
	  fprintf(in, "%s ", acc->name);
	fprintf(in, "\n\t%d mRNAs support neither: ", indel->noMrna);
	for (acc = indel->noMrnaAcc; acc != NULL; acc = acc->next)
	  fprintf(in, "%s ", acc->name);
	fprintf(in, "\n\t%d ESTs support neither: ",indel->noEst);
	for (acc = indel->noEstAcc; acc != NULL; acc = acc->next)
	  fprintf(in, "%s ", acc->name);
	fprintf(in, "\n\n");
      }
    }

/*for (i = 1; i <= 100; i++) 
  fprintf(in, "%d\t%d\n",i,indels[i]);*/
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
printf("Reading CDS file\n");
readCds(cf);
printf("Reading FA file\n");
readRnaSeq(faFile);
printf("Reading loci file\n");
readLoci(lf);
printf("Processing psl file\n");
doFile(pf);
printf("Writing out file\n");
writeOut(of, in);

lineFileClose(&pf);
fclose(of);
fclose(in);

return 0;
}
