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
#include "psl.h"
#include "snp.h"
#include "fa.h"
#include "psl.h"
#include "options.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"db", OPTION_STRING},
    {"verbose", OPTION_BOOLEAN},
    {"indels", OPTION_BOOLEAN},
    {"mismatches", OPTION_BOOLEAN},
    {"codonsub", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean verbose = FALSE;
boolean indelReport = FALSE;
boolean mismatchReport = FALSE;
boolean codonSubReport = FALSE;

struct acc
{
  struct acc *next;
  char name[16];
};

/* indel object is now used for tracking several, these constants
 * defined the type */
#define INDEL    1
#define MISMATCH 2
#define CODONSUB 3

struct indel
{
  struct indel *next;
  int size;
  char *chrom;
  int chromStart;  /* note: [1..n] coordinates */
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

  /* fields used if this is tracking codon substitutions*/
  int codonGenPos[3];  /* position of the codon bases */
  char genCodon[4];
  char mrnaCodon[4];
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
  struct indel *mmList;
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
  int synSub;
  int nonSynSub;
  int synSubSnp;
  int nonSynSubSnp;
  int loci;
  struct indel *codonSubList;
} *piList = NULL;

struct hash *cdsStarts = NULL;
struct hash *cdsEnds = NULL;
struct hash *loci = NULL;
struct hash *rnaSeqs = NULL;

int indels[128];

void accFree(struct acc **acc)
/* Free a dynamically allocated acc */
{
struct acc *a;

if ((a = *acc) == NULL) return;
freez(acc);
}

void accFreeList(struct acc **aList)
/* Free a dynamically allocated list of acc's */
{
struct acc *a = NULL, *next = NULL;

for (a = *aList; a != NULL; a = next)
    {
    next = a->next;
    accFree(&a);
    }
*aList = NULL;
}

void indelFree(struct indel **in)
/* Free a dynamically allocated indel */
{
struct indel *i;

if ((i = *in) == NULL) return;
/*freeMem(i->chrom);
  freeMem(i->mrna);*/
accFreeList(&(i->genMrnaAcc));
accFreeList(&(i->genEstAcc));
accFreeList(&(i->mrnaMrnaAcc));
accFreeList(&(i->mrnaEstAcc));
accFreeList(&(i->noMrnaAcc));
accFreeList(&(i->noEstAcc));
freez(in);
}

void indelListFree(struct indel **iList)
/* Free a dynamically allocated list of indel's */
{
struct indel *i, *next;

for (i = *iList; i != NULL; i = next)
    {
    next = i->next;
    indelFree(&i);
    }
*iList = NULL;
}

void pslInfoFree(struct pslInfo **pi)
/* Free a single dynamically allocated pslInfo element */
{
struct pslInfo *el;

if ((el = *pi) == NULL) return;
pslFree(&(el->psl));
indelListFree(&(el->indelList));
freez(pi);
}

void readCds(struct lineFile *cf)
/* Read in file of coding region starts and stops 
   Convert start to 0-based to make copmarison with psl easier */
{
int lineSize, wordCount;
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

boolean haveSnpTable(struct sqlConnection *conn)
/* Check if the SNP table exists for this DB */
{
static boolean checked = FALSE;
static boolean haveSnpTbl;
if (!checked)
    {
    haveSnpTbl = sqlTableExists(conn, "snpTsc");
    checked = TRUE;
    if (!haveSnpTbl)
        fprintf(stderr, "warning: no snpTsc table in this databsae\n");
    }
return haveSnpTbl;
}

int snpBase(struct sqlConnection *conn, char *chr, int position)
/* Determine whether a given position is known to be a SNP */ 
{
char query[256];
struct sqlResult *sr;
char **row;

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
       if (gstart >= psl->tStarts[i])
	  {
	  *start = psl->qStarts[i] + (gstart - psl->tStarts[i]);
	  gs = gstart;
	  } 
       else 
	  gs = psl->tStarts[i];
       /* Determine the end position offset */
       if (gend <= (psl->tStarts[i]+psl->blockSizes[i]))
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

void searchTransPsl(struct sqlConnection *conn, char *table, char *mrnaName, DNA *mdna, struct indel *ni, char *strand, unsigned type, struct psl* psl)
/* process one mRNA or EST for searchTrans */
{
int start = 0, end = 0;
struct dnaSeq *gseq = NULL, *mseq = NULL;
char *dna = NULL;
char thisStrand[2];
struct acc *acc;

/* Get the DNA sequence for the indel */
if (type == INDEL)
    gseq = getDna(psl, ni->chromStart-2, ni->chromEnd+2, &start, &end, thisStrand);
else
    gseq = getDna(psl, ni->chromStart-1, ni->chromEnd, &start, &end, thisStrand);
/* Get the corresponding mRNA or EST  sequence */
struct dnaSeq *seq = hRnaSeq(psl->qName);
if (thisStrand[0] != strand[0])
    {
    int temp = start;
    start = seq->size - end;
    end = seq->size - temp;
    reverseComplement(seq->dna, seq->size);
    reverseComplement(gseq->dna, gseq->size);
    }
if ((end-start) > 0)
    {
    dna = needMem((end-start)+1);
    strncpy(dna,seq->dna + start, end-start);
    dna[end-start] = '\0';
    }
else
    dna = cloneString("");
/*printf("Comparing %s:%d-%d(%d) bases %s with genomic %s\n",psl->qName, start, end, seq->size, seq->dna, gseq->dna);*/
AllocVar(acc);
strcpy(acc->name,seq->name);
if (sameString(gseq->dna, dna)) 
    if (sameString(table, "mrna"))
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
    if (sameString(mdna, dna)) 
        if (sameString(table, "mrna"))
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
        if (sameString(table, "mrna"))
            {
            ni->noMrna++;
            slAddHead(&ni->noMrnaAcc, acc);
            } 
        else 
            {
            ni->noEst++;
            slAddHead(&ni->noEstAcc, acc);
            }
freez(&dna);
dnaSeqFree(&seq);
dnaSeqFree(&gseq);
}

void searchTrans(struct sqlConnection *conn, char *table, char *mrnaName, struct dnaSeq *rna, struct indel *ni, char *strand, unsigned type)
/* Find all mRNA's or EST's that align in the region of an indel, mismatch, or codon */
{
struct sqlResult *sr;
char **row;
int offset;
struct psl *psl;
DNA mdna[10000];

if (type == CODONSUB)
    assert(((ni->mrnaEnd-ni->mrnaStart)+1) == 3);

/* Determine the sequence, If indel, add one base on each side */
if (type == INDEL)
    {
    if ((ni->mrnaEnd-ni->mrnaStart+4) > 0)
        {
        strncpy(mdna,rna->dna + ni->mrnaStart - 2,ni->mrnaEnd-ni->mrnaStart+4);
        mdna[ni->mrnaEnd-ni->mrnaStart+4] = '\0';
        /*printf("Indel at %d-%d (%d) in %s:%d-%d bases %s\n", ni->mrnaStart, ni->mrnaEnd, rna->size, ni->chrom, ni->chromStart, ni->chromEnd, mseq->dna);*/
        }
    else
        mdna[0] = '\0';
    }
else
    {
    int len = (ni->mrnaEnd-ni->mrnaStart)+1;
    strncpy(mdna,rna->dna + ni->mrnaStart,len);
    mdna[len] = '\0';
    /*printf("Mismatch in %s at %d in %s:%d bases %s\n", mrnaName, ni->mrnaStart, ni->chrom, ni->chromStart, mdna);*/
    }

/* Find all sequences that span this region */
if (type == INDEL)
    sr = hRangeQuery(conn, table, ni->chrom, ni->chromStart-2, ni->chromEnd+2, NULL, &offset);
else 
    sr = hRangeQuery(conn, table, ni->chrom, ni->chromStart-1, ni->chromEnd, NULL, &offset);
while ((row = sqlNextRow(sr)) != NULL) 
    {
    psl = pslLoad(row+offset);
    if (!sameString(psl->qName,mrnaName))
        searchTransPsl(conn, table, mrnaName, mdna, ni, strand, type, psl);
    pslFree(&psl);
    }
sqlFreeResult(&sr);
}

struct indel *createMismatch(struct sqlConnection *conn, char *mrna, int mbase, char* chr, int gbase, 
			  struct dnaSeq *rna, char *strand)
/* Create a record of a mismatch */
{
  struct indel *mi;
 
  AllocVar(mi);
  mi->size = 1;
  mi->chrom = chr;
  mi->chromStart = mi->chromEnd = gbase;
  mi->mrna = mrna;
  mi->mrnaStart = mi->mrnaEnd = mbase;
  
  /* Determine whether mRNAs and ESTs support genomic or mRNA sequence in mismatch */
  searchTrans(conn, "mrna", mrna, rna, mi, strand, MISMATCH);
  searchTrans(conn, "est", mrna, rna, mi, strand, MISMATCH);

  return(mi);
}

struct indel *createCodonSub(struct sqlConnection *conn, char *mrna, int mrnaStart,
                             char *mCodon, char* chr, int genPos[3], char* gCodon,
                             struct dnaSeq *rna, char *strand)
/* Create a record of a mismatch */
{
  struct indel *mi;
#if 0
  printf("codonSub: mrna=%s mrnaStart=%d  genPos=%d,%d,%d mCodon=%s,gCodon=%s, aa=%c %c\n",
         mrna, mrnaStart, genPos[0], genPos[1], genPos[2], mCodon, gCodon,
         lookupCodon(mCodon), lookupCodon(gCodon));
#endif
 
  AllocVar(mi);
  mi->size = 1;
  mi->chrom = chr;
  mi->chromStart = genPos[0];
  mi->chromEnd = genPos[2];
  mi->mrna = mrna;
  mi->mrnaStart = mrnaStart;
  mi->mrnaEnd = mrnaStart+2;
  memcpy(mi->codonGenPos, genPos, sizeof(mi->codonGenPos));
  strcpy(mi->genCodon, gCodon);
  strcpy(mi->mrnaCodon, mCodon);
  
  /* Determine whether mRNAs and ESTs support genomic or mRNA sequence in mismatch */
  searchTrans(conn, "mrna", mrna, rna, mi, strand, CODONSUB);
  searchTrans(conn, "est", mrna, rna, mi, strand, CODONSUB);

  return(mi);
}
void cdsCompare(struct sqlConnection *conn, struct pslInfo *pi, struct dnaSeq *rna, struct dnaSeq *dna)
/* Compare the alignment of the coding region of an mRNA to the genomic sequence */
{
int i,j;
DNA *r, *d; 
DNA rCodon[4], dCodon[4];
int codonSnps = 0, codonMismatches = 0;
int codonGenPos[3];
int codonMrnaStart = 0;
int nCodonBases = 0;   /* to deal with partial codons */
struct indel *mi, *miList=NULL;
struct indel *codonSub, *codonSubList=NULL;
ZeroVar(codonGenPos);

strcpy(rCodon, "---");
strcpy(dCodon, "---");
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
        {
        /* Check if it is in the coding region */
	if (((qstart+j) >= pi->cdsStart) && ((qstart+j) < pi->cdsEnd))
            {
	    int tPosition = tstart + j + pi->psl->tStarts[0];
            int iCodon = ((qstart+j-pi->cdsStart)%3);
            if (iCodon == 0) {
                codonSnps = 0;
                codonMismatches = 0;
                codonMrnaStart = qstart+j;
            }
	    if (pi->psl->strand[0] == '-')
                {
	        tPosition = pi->psl->tSize - tPosition - 1;
                codonGenPos[2-iCodon] = tPosition;
                }
            else
                {
                codonGenPos[iCodon] = tPosition;
                }
            rCodon[iCodon] = r[qstart+j];
            dCodon[iCodon] = d[tstart+j];
            nCodonBases++;
            /* Bases match */
            if ((char)r[qstart+j] == (char)d[tstart+j])
                pi->cdsMatch++;
            /* Check if mismatch is due to a SNP */
            else if (haveSnpTable(conn)
                     && snpBase(conn,pi->psl->tName,tPosition)) 
                {
                pi->cdsMatch++;
                pi->snp++;
                codonSnps++;
                }
            else
                {
                pi->cdsMismatch++;
		if (mismatchReport)
		    {
		    mi = createMismatch(conn, pi->psl->qName, qstart+j, pi->psl->tName, tPosition+1, rna, pi->psl->strand);
		    slAddHead(&miList,mi);
		    }
                codonMismatches++;
                /* Check if mismatch is in a codon wobble position.*/
                if (iCodon==2)
                    pi->thirdPos++;
                }
            /* If third base, check codon for mismatch */
            if ((iCodon==2) && (nCodonBases == 3) && !sameString(rCodon, dCodon))
                {
                if (lookupCodon(rCodon) == lookupCodon(dCodon))
                    {
                    pi->synSub++;
                    if ((codonSnps > 0) && (codonMismatches == 0))
                        pi->synSubSnp++;
                    }
                else
                    {
                    pi->nonSynSub++;
                    if ((codonSnps > 0) && (codonMismatches == 0))
                        pi->nonSynSubSnp++;
                    }
		if (codonSubReport)
                    {
                    codonSub = createCodonSub(conn, pi->psl->qName, qstart+j,
                                              rCodon, pi->psl->tName, codonGenPos,
                                              dCodon, rna, pi->psl->strand);
                    slAddHead(&codonSubList, codonSub);
                    }
                }
            if (iCodon == 2) 
                nCodonBases = 0;
            }
        }
    }
if (mismatchReport)
    { 
    slReverse(&miList);
    pi->mmList = miList;
    }
if (codonSubReport)
    {
    slReverse(&codonSubList);
    pi->codonSubList = codonSubList;
    }
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
  
  /* Determine whether mRNAs and ESTs support genomic or mRNA sequence in indel region */
  searchTrans(conn, "mrna", mrna, rna, ni, strand, INDEL);
  searchTrans(conn, "est", mrna, rna, ni, strand, INDEL);

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
	  prevqend = cdsS;
	  prevtend = tstart;
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
	    /*if ((pi->cdsPctId >= 0.98) && (pi->cdsCoverage >= 0.80))
	      indels[unaligned]++;*/
	    if (pi->indelCount > 256) 
		errAbort("Indel count too high");
	    if (pi->psl->strand[0] == '-') {
	      int temp = tstart;
	      tstart = pi->psl->tSize - prevtend;
	      prevtend = pi->psl->tSize - temp;
	    }
            /* Create an indel record for this */
	    if (indelReport) 
	        {
		ni = createIndel(conn, pi->psl->qName, prevqend, qstart, pi->psl->tName, prevtend, tstart, rna, pi->psl->strand); 
		slAddHead(&niList,ni);
		}
	  }
      }
    
    /* Find deletions in the mRNA sequence */
    unaligned = 0;
    qstart = pi->psl->qStarts[i];
    tstart = pi->psl->tStarts[i];
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
	  prevqend = qstart;
	  prevtend = tstart;
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
	    /*if ((pi->cdsPctId >= 0.98) && (pi->cdsCoverage >= 0.80))
	      indels[unaligned]++;*/
	    if (pi->psl->strand[0] == '-') {
	      int temp = tstart;
	      tstart = pi->psl->tSize - prevtend;
	      prevtend = pi->psl->tSize - temp;
	    }
	    /* Create an indel record for this */
	    if (indelReport)
	        {
		ni = createIndel(conn, pi->psl->qName, prevqend, qstart, pi->psl->tName, prevtend, tstart, rna, pi->psl->strand); 
		slAddHead(&niList,ni);
		}
	  }
      }
  }
  if (indelReport)
      { 
      slReverse(&niList);
      pi->indelList = niList;
      }
}

void processCds(struct sqlConnection *conn, struct pslInfo *pi, struct dnaSeq *rna, struct dnaSeq *dna)
/* Examine closely the alignment of the coding region */
{
if (verbose)
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

/* Revert back to original psl record for printing */
if (psl->strand[0] == '-') 
  {
   pslRcBoth(pi->psl);
  }

freeDnaSeq(&dnaSeq);
return(pi);
}

void writeOut(FILE *of, FILE *in, FILE *mm, FILE* cs, struct pslInfo *pi)
/* Output results of the mRNA alignment analysis */
{
struct indel *indel;
struct acc *acc;
int i;

fprintf(of, "%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t", pi->psl->qName, pi->psl->tName, pi->psl->tStart, 
	pi->psl->tEnd,pi->psl->qStart,pi->psl->qEnd,pi->psl->qSize,pi->loci);
fprintf(of, "%.4f\t%.4f\t%d\t%d\t%.4f\t%.4f\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t", 
	pi->coverage, pi->pctId, pi->cdsStart+1, 
	pi->cdsEnd, pi->cdsCoverage, pi->cdsPctId, pi->cdsMatch, 
	pi->cdsMismatch, pi->snp, pi->thirdPos, pi->synSub, pi->nonSynSub,
        pi->synSubSnp, pi->nonSynSubSnp, pi->introns, pi->stdSplice);
fprintf(of, "%d\t%d\t%d\t%d\t", pi->unalignedCds, pi->singleIndel, pi->tripleIndel, pi->totalIndel);
for (i = 0; i < pi->indelCount; i++)
    fprintf(of, "%d,", pi->indels[i]);
fprintf(of, "\n");

/* Write out detailed records of indels, if requested */
if (indelReport) 
    {
    if (verbose)
        printf("Writing out indels\n");
    for (indel = pi->indelList; indel != NULL; indel=indel->next)
        {
	/*printf("Indel of size %d in %s:%d-%d vs. %s:%d-%d\n",
	  indel->size, indel->mrna, indel->mrnaStart, indel->mrnaEnd,
	  indel->chrom, indel->chromStart, indel->chromEnd);*/
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

/* Write out detailed records of mismatches, if requested */
if (mismatchReport) 
    {
    if (verbose)
        printf("Writing out mismatches\n");
    for (indel = pi->mmList; indel != NULL; indel=indel->next)
        {
	/*printf("Indel of size %d in %s:%d-%d vs. %s:%d-%d\n",
	  indel->size, indel->mrna, indel->mrnaStart, indel->mrnaEnd,
	  indel->chrom, indel->chromStart, indel->chromEnd);*/
	fprintf(mm, "Mismatch at %s:%d vs. %s:%d\n",
	    indel->mrna, indel->mrnaStart, indel->chrom, indel->chromStart);
	fprintf(mm, "\t%d mRNAs support genomic: ", indel->genMrna);
	for (acc = indel->genMrnaAcc; acc != NULL; acc = acc->next)
	    fprintf(mm, "%s ", acc->name);
	fprintf(mm, "\n\t%d ESTs support genomic: ",indel->genEst);
	for (acc = indel->genEstAcc; acc != NULL; acc = acc->next)
	    fprintf(mm, "%s ", acc->name);
	fprintf(mm, "\n\t%d mRNAs support %s: ", indel->mrnaMrna, indel->mrna);
	for (acc = indel->mrnaMrnaAcc; acc != NULL; acc = acc->next)
	    fprintf(mm, "%s ", acc->name);
	fprintf(mm, "\n\t%d ESTs support %s: ",indel->mrnaEst, indel->mrna);
	for (acc = indel->mrnaEstAcc; acc != NULL; acc = acc->next)
	    fprintf(mm, "%s ", acc->name);
	fprintf(mm, "\n\t%d mRNAs support neither: ", indel->noMrna);
	for (acc = indel->noMrnaAcc; acc != NULL; acc = acc->next)
	    fprintf(mm, "%s ", acc->name);
	fprintf(mm, "\n\t%d ESTs support neither: ",indel->noEst);
	for (acc = indel->noEstAcc; acc != NULL; acc = acc->next)
	    fprintf(mm, "%s ", acc->name);
	fprintf(mm, "\n\n");
	}
    }
/* Write out detailed records of codon substitutions, if requested */
if (codonSubReport) 
    {
    if (verbose)
        printf("Writing out codon substitutions\n");
    for (indel = pi->codonSubList; indel != NULL; indel=indel->next)
        {
        char mrnaAA = lookupCodon(indel->mrnaCodon);
        char genAA = lookupCodon(indel->genCodon);
        bool isSyn = (mrnaAA == genAA);
	fprintf(cs, "%s codon substitution at %s:%d vs. %s:%d,%d,%d, %s vs. %s, ",
                (isSyn ? "synonymous" : "non-synonymous"),
                indel->mrna, indel->mrnaStart, indel->chrom, indel->codonGenPos[0],
                indel->codonGenPos[1], indel->codonGenPos[2],
                indel->genCodon, indel->mrnaCodon);
        if (mrnaAA == 0)
            fprintf(cs, "STOP vs. ");
        else
            fprintf(cs, "%c vs. ", mrnaAA);
        if (genAA == 0)
            fprintf(cs, "STOP\n");
        else
            fprintf(cs, "%c\n", genAA);
	fprintf(cs, "\tpsl %s %d %d\t%s %d %d\n",
                pi->psl->qName, pi->psl->qStart, pi->psl->qEnd,
                pi->psl->tName, pi->psl->tStart, pi->psl->tEnd);
	fprintf(cs, "\t%d mRNAs support genomic: ", indel->genMrna);
	for (acc = indel->genMrnaAcc; acc != NULL; acc = acc->next)
                fprintf(cs, "%s ", acc->name);
	fprintf(cs, "\n\t%d ESTs support genomic: ",indel->genEst);
	for (acc = indel->genEstAcc; acc != NULL; acc = acc->next)
	    fprintf(cs, "%s ", acc->name);
	fprintf(cs, "\n\t%d mRNAs support %s: ", indel->mrnaMrna, indel->mrna);
	for (acc = indel->mrnaMrnaAcc; acc != NULL; acc = acc->next)
	    fprintf(cs, "%s ", acc->name);
	fprintf(cs, "\n\t%d ESTs support %s: ",indel->mrnaEst, indel->mrna);
	for (acc = indel->mrnaEstAcc; acc != NULL; acc = acc->next)
	    fprintf(cs, "%s ", acc->name);
	fprintf(cs, "\n\t%d mRNAs support neither: ", indel->noMrna);
	for (acc = indel->noMrnaAcc; acc != NULL; acc = acc->next)
	    fprintf(cs, "%s ", acc->name);
	fprintf(cs, "\n\t%d ESTs support neither: ",indel->noEst);
	for (acc = indel->noEstAcc; acc != NULL; acc = acc->next)
	    fprintf(cs, "%s ", acc->name);
	fprintf(cs, "\n\n");
	}
    }
}
 
void doFile(struct lineFile *pf, FILE *of, FILE *in, FILE *mm, FILE* cs)
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
    slAddHead(&piList, pi);
    writeOut(of, in, mm, cs, pi);
    pslInfoFree(&pi);
    }
hFreeConn(&conn);
}

int main(int argc, char *argv[])
{
struct lineFile *pf, *cf, *lf;
FILE *of, *in=NULL, *mm=NULL, *cs=NULL;
char *faFile, *db, filename[64];

optionInit(&argc, argv, optionSpecs);
if (argc != 6)
    {
    fprintf(stderr, "USAGE: pslAnal [-db=db] -verbose -indels -mismatches -codonsub <psl file> <cds file> <loci file> <fa file> <out file prefix>\n");
    return 1;
    }
db = optionVal("db", "hg15");
verbose = optionExists("verbose");
indelReport = optionExists("indels");
mismatchReport = optionExists("mismatches");
codonSubReport = optionExists("codonsub");
pf = pslFileOpen(argv[1]);
cf = lineFileOpen(argv[2], FALSE);
lf = lineFileOpen(argv[3], FALSE);
faFile = argv[4];
sprintf(filename, "%s.anal", argv[5]);
of = mustOpen(filename, "w");
fprintf(of, "Acc\tChr\tStart\tEnd\tmStart\tmEnd\tSize\tLoci\tCov\tID\tCdsStart\tCdsEnd\tCdsCov\tCdsID\tCdsMatch\tCdsMismatch\tSnp\tThirdPos\tSyn\tNonSyn\tSynSnp\tNonSynSnp\tIntrons\tStdSplice\tUnCds\tSingle\tTriple\tTotal\tIndels\n");
fprintf(of, "10\t10\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10N\t10\n");
if (indelReport) 
    {
    sprintf(filename, "%s.indel", argv[5]);
    in = mustOpen(filename, "w");
    }
if (mismatchReport)
    {
    sprintf(filename, "%s.mismatch", argv[5]);
    mm = mustOpen(filename, "w");
    }

if (codonSubReport)
    {
    sprintf(filename, "%s.codonsub", argv[5]);
    cs = mustOpen(filename, "w");
    }

hSetDb(db);
if (verbose)
    printf("Reading CDS file\n");
readCds(cf);
if (verbose)
    printf("Reading FA file\n");
readRnaSeq(faFile);
if (verbose)
    printf("Reading loci file\n");
readLoci(lf);
if (verbose)
    printf("Processing psl file\n");
doFile(pf, of, in, mm, cs);

lineFileClose(&pf);
fclose(of);
fclose(in);

return 0;
}
