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
    {"xeno", OPTION_BOOLEAN},
    {"indels", OPTION_BOOLEAN},
    {"mismatches", OPTION_BOOLEAN},
    {"codonsub", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean verbose = FALSE;
boolean indelReport = FALSE;
boolean mismatchReport = FALSE;
boolean codonSubReport = FALSE;
boolean xeno = FALSE;

struct acc
{
  struct acc *next;
  char *name;
  char *organism;
};

struct clone
{
  int id;
  int imageId;
};

/* indel object is now used for tracking several, these constants
 * defined the type */
#define INDEL    1
#define MISMATCH 2
#define CODONSUB 3
#define UNALIGNED 4

struct evid
{
  struct evid *next;
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
  int unMrna;
  struct acc *unMrnaAcc;
  int unEst;
  struct acc *unEstAcc;
};

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
  /* struct clone *mrnaCloneId; */
  struct evid *hs;
  struct evid *xe;

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
  struct indel *unaliList;
  int snp;
  int thirdPos;
  int synSub;
  int nonSynSub;
  int synSubSnp;
  int nonSynSubSnp;
  int loci;
  struct indel *codonSubList;
  struct clone *mrnaCloneId;
} *piList = NULL;

struct hash *cdsStarts = NULL;
struct hash *cdsEnds = NULL;
struct hash *loci = NULL;
struct hash *rnaSeqs = NULL;

int indels[128];

void cloneFree(struct clone **clone)
/* Free a dynamically allocated acc */
{
struct clone *c;

if ((c = *clone) == NULL) return;
freez(clone);
}

void accFree(struct acc **acc)
/* Free a dynamically allocated acc */
{
struct acc *a;

if ((a = *acc) == NULL) return;
freez(&(a->organism));
freez(&(a->name));
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

void evidFree(struct evid **ev)
/* Free a dynamically allocated evid */
{
struct evid *e;

if ((e = *ev) == NULL) return;
accFreeList(&(e->genMrnaAcc));
accFreeList(&(e->genEstAcc));
accFreeList(&(e->mrnaMrnaAcc));
accFreeList(&(e->mrnaEstAcc));
accFreeList(&(e->noMrnaAcc));
accFreeList(&(e->noEstAcc));
accFreeList(&(e->unMrnaAcc));
accFreeList(&(e->unEstAcc));
freez(ev);
}

void evidListFree(struct evid **iList)
/* Free a dynamically allocated list of evid's */
{
struct evid *i, *next;

for (i = *iList; i != NULL; i = next)
    {
    next = i->next;
    evidFree(&i);
    }
*iList = NULL;
}

void indelFree(struct indel **in)
/* Free a dynamically allocated indel */
{
struct indel *i;

if ((i = *in) == NULL) return;
evidListFree(&(i->hs));
evidListFree(&(i->xe));
/* cloneFree(&(i->mrnaCloneId)); */
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
indelListFree(&(el->mmList));
indelListFree(&(el->codonSubList));
cloneFree(&(el->mrnaCloneId));
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
int ret = 0;
boolean haveSnpTsc = sqlTableExists(conn, "snpTsc");
boolean haveSnpNih = sqlTableExists(conn, "snpNih");
boolean haveAffyGeno = sqlTableExists(conn, "affyGeno");

if (haveSnpTsc)
    {
    sprintf(query, "select * from snpTsc where chrom = '%s' and chromStart = %d", chr, position); 
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL) 
      ret = 1;
    sqlFreeResult(&sr);
    }

if ((haveSnpNih) && (!ret))
    {
    sprintf(query, "select * from snpNih where chrom = '%s' and chromStart = %d", chr, position); 
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL) 
        ret = 1;
    sqlFreeResult(&sr);
    }

if ((haveAffyGeno) && (!ret))
    {
    sprintf(query, "select * from affyGeno where chrom = '%s' and chromStart = %d", chr, position); 
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL) 
        ret = 1;
    sqlFreeResult(&sr);
    }

return(ret);
}

void findOrganism(struct sqlConnection *conn, struct acc *acc)
/* Determine organism for each non-human mrna/est in the list */
{
char query[256];
struct sqlResult *sr;
char **row;
char *a;
char *accs[4];
int wordCount, id = -1;


a = cloneString(acc->name);
wordCount = chopByChar(a, '.', accs, ArraySize(accs)); 
if (wordCount != 2) 
    errAbort("Accession not standard, %s\n", acc->name);
sprintf(query, "select organism from mrna where acc = '%s'", accs[0]); 
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    id = sqlUnsigned(row[0]);
sqlFreeResult(&sr);
if (id != -1)
    {
    sprintf(query, "select name from organism where id = %d", id);   
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
      acc->organism = cloneString(row[0]);
    else
      printf("Could not find organism for id %d\n", id);
    sqlFreeResult(&sr);
    } 
else 
    printf("Could not find mrna record for %s\n", acc->name);
}

struct clone *getMrnaCloneId(struct sqlConnection *conn, char *acc)
/* Find the clone id for an mrna accession */
{
char query[256];
struct sqlResult *sr, *sr1;
char **row;
char *a;
char *accs[4];
int wordCount, i;
struct clone *ret = NULL;

AllocVar(ret);
a = cloneString(acc);
wordCount = chopByChar(a, '.', accs, ArraySize(accs)); 
if (wordCount > 2) 
    errAbort("Accession not standard, %s\n", acc);
sprintf(query, "select mrnaClone from mrna where acc = '%s'", accs[0]); 
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    ret->id = sqlUnsigned(row[0]);
    ret->imageId = 0;
    }
sqlFreeResult(&sr);
sprintf(query, "select imageId from imageClone where acc = '%s'", accs[0]); 
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    ret->imageId = sqlUnsigned(row[0]);
sqlFreeResult(&sr);
return(ret);
}

boolean sameClone(struct clone *c1, struct clone *c2)
/* Determine if two clones are the same */
{
if ((c1 == NULL) || (c2 == NULL))
    return(0);
 if (((c1->id) && (c2->id) && (c1->id == c2->id)) || ((c1->imageId) && (c2->imageId) && (c1->imageId == c2->imageId)))
    return(1);
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

void searchTransPsl(char *table, char *mrnaName, DNA *mdna, struct indel *ni, char *strand, unsigned type, struct psl* psl)
/* process one mRNA or EST for searchTrans */
{
int start = 0, end = 0;
struct dnaSeq *gseq = NULL, *mseq = NULL;
char *dna = NULL;
char thisStrand[2];
struct acc *acc;
struct sqlConnection *conn2 = hAllocConn();

/* Get the DNA sequence for the region in question */
if (type == INDEL)
    gseq = getDna(psl, ni->chromStart-2, ni->chromEnd+1, &start, &end, thisStrand);
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
/* Classify this mrna/est */
AllocVar(acc);
acc->name = cloneString(seq->name);
acc->organism = NULL;
/* If it doesn't align to this region */
if ((start == 0) && (end == 0))
    {
    if (sameString(table, "mrna"))
        {
        ni->hs->unMrna++;
        slAddHead(&ni->hs->unMrnaAcc, acc);
        } 
    else if (sameString(table, "xenoMrna"))
        {
        ni->xe->unMrna++;
	findOrganism(conn2, acc);
	slAddHead(&ni->xe->unMrnaAcc, acc);
	}
    else if (sameString(table, "est"))
        {
        ni->hs->unEst++;
        slAddHead(&ni->hs->unEstAcc, acc);
	}
     else if (sameString(table, "xenoEst"))
        {
        ni->xe->unEst++;
	findOrganism(conn2, acc);
	slAddHead(&ni->xe->unEstAcc, acc);
        }
    }
/* If it agrees with the genomic sequence */
else if ((sameString(gseq->dna, dna)) || ((type == INDEL) && (strlen(dna) == gseq->size))) 
    {
    if (sameString(table, "mrna"))
        {
        ni->hs->genMrna++;
        slAddHead(&ni->hs->genMrnaAcc, acc);
        } 
    else if (sameString(table, "xenoMrna"))
        {
        ni->xe->genMrna++;
	findOrganism(conn2, acc);
	slAddHead(&ni->xe->genMrnaAcc, acc);
	}
    else if (sameString(table, "est"))
        {
        ni->hs->genEst++;
        slAddHead(&ni->hs->genEstAcc, acc);
	}
     else if (sameString(table, "xenoEst"))
        {
        ni->xe->genEst++;
	findOrganism(conn2, acc);
	slAddHead(&ni->xe->genEstAcc, acc);
        }
    }
/* If it agrees with the mrna sequence */
else if ((sameString(mdna, dna)) || ((type == INDEL) && (strlen(dna) == strlen(mdna)))) 
    {
    if (sameString(table, "mrna"))
        {
        ni->hs->mrnaMrna++;
        slAddHead(&ni->hs->mrnaMrnaAcc, acc);
        } 
    else if (sameString(table, "xenoMrna"))
        {
        ni->xe->mrnaMrna++;
	findOrganism(conn2, acc);
	slAddHead(&ni->xe->mrnaMrnaAcc, acc);
	}
    else if (sameString(table, "est"))
        {
        ni->hs->mrnaEst++;
        slAddHead(&ni->hs->mrnaEstAcc, acc);
	}
     else if (sameString(table, "xenoEst"))
        {
        ni->xe->mrnaEst++;
	findOrganism(conn2, acc);
	slAddHead(&ni->xe->mrnaEstAcc, acc);
        }
    }
/* if it agrees with neither */
else 
    {
    if (sameString(table, "mrna"))
        {
	ni->hs->noMrna++;
        slAddHead(&ni->hs->noMrnaAcc, acc);
        } 
    else if (sameString(table, "xenoMrna"))
        {
        ni->xe->noMrna++;
	findOrganism(conn2, acc);
	slAddHead(&ni->xe->noMrnaAcc, acc);
	}
    else if (sameString(table, "est"))
        {
        ni->hs->noEst++;
        slAddHead(&ni->hs->noEstAcc, acc);
	}
     else if (sameString(table, "xenoEst"))
        {
        ni->xe->noEst++;
	findOrganism(conn2, acc);
	slAddHead(&ni->xe->noEstAcc, acc);
        }
    }  
freez(&dna);
dnaSeqFree(&seq);
dnaSeqFree(&gseq);
hFreeConn(&conn2);
}

void searchTrans(struct sqlConnection *conn, char *table, char *mrnaName, struct dnaSeq *rna, struct indel *ni, char *strand, unsigned type, struct clone *mrnaCloneId)
/* Find all mRNA's or EST's that align in the region of an indel, mismatch, or codon */
{
struct sqlResult *sr;
char **row;
int offset;
struct clone *cloneId;
struct psl *psl;
DNA mdna[10000];
struct sqlConnection *conn2 = hAllocConn();

if (type == CODONSUB)
    assert(((ni->mrnaEnd-ni->mrnaStart)+1) == 3);

/* Determine the sequence, If indel, add one base on each side */
if (type == INDEL)
    {
    if ((ni->mrnaEnd-ni->mrnaStart+4) > 0)
        {
	assert((ni->mrnaEnd-ni->mrnaStart+4) < sizeof(mdna));
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
    assert((len) < sizeof(mdna));
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
    cloneId = getMrnaCloneId(conn2, psl->qName);
    if ((!sameString(psl->qName,mrnaName)) && (!sameClone(cloneId,mrnaCloneId)))
        searchTransPsl(table, mrnaName, mdna, ni, strand, type, psl);
    cloneFree(&cloneId);
    pslFree(&psl);
    }
sqlFreeResult(&sr);
hFreeConn(&conn2);
}

struct evid *createEvid()
/* Create an instance of an evid struct */
{
struct evid *ev;

 AllocVar(ev);
 ev->next = NULL;
 ev->genMrna = 0;
 ev->genMrnaAcc = NULL;
 ev->genEst = 0;
 ev->genEstAcc = NULL;
 ev->mrnaMrna = 0;
 ev->mrnaMrnaAcc = NULL;
 ev->mrnaEst = 0;
 ev->mrnaEstAcc = NULL;
 ev->noMrna = 0;
 ev->noMrnaAcc = NULL;
 ev->noEst = 0;
 ev->noEstAcc = NULL;
 ev->unMrna = 0;
 ev->unMrnaAcc = NULL;
 ev->unEst = 0;
 ev->unEstAcc = NULL;
 return(ev);
}

struct indel *createMismatch(struct sqlConnection *conn, char *mrna, int mbase, char* chr, int gbase, 
			  struct dnaSeq *rna, char *strand, struct clone *cloneId)
/* Create a record of a mismatch */
{
  struct indel *mi;
 
  AllocVar(mi);
  mi->next = NULL;
  mi->size = 1;
  mi->chrom = chr;
  mi->chromStart = mi->chromEnd = gbase;
  mi->mrna = mrna;
  mi->mrnaStart = mi->mrnaEnd = mbase;
  mi->hs = createEvid();
  mi->xe = createEvid();
  
  /* Determine whether mRNAs and ESTs support genomic or mRNA sequence in mismatch */
  searchTrans(conn, "mrna", mrna, rna, mi, strand, MISMATCH, cloneId);
  searchTrans(conn, "est", mrna, rna, mi, strand, MISMATCH, cloneId);
  if (xeno)
      {
      searchTrans(conn, "xenoMrna", mrna, rna, mi, strand, MISMATCH, cloneId);
      searchTrans(conn, "xenoEst", mrna, rna, mi, strand, MISMATCH, cloneId);
      }

  return(mi);
}

struct indel *createCodonSub(struct sqlConnection *conn, char *mrna, int mrnaStart,
                             char *mCodon, char* chr, int genPos[3], char* gCodon,
                             struct dnaSeq *rna, char *strand, struct clone *cloneId)
/* Create a record of a mismatch */
{
  struct indel *mi;
#if 0
  printf("codonSub: mrna=%s mrnaStart=%d  genPos=%d,%d,%d mCodon=%s,gCodon=%s, aa=%c %c\n",
         mrna, mrnaStart, genPos[0], genPos[1], genPos[2], mCodon, gCodon,
         lookupCodon(mCodon), lookupCodon(gCodon));
#endif
 
  AllocVar(mi);
  mi->next = NULL;
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
  mi->hs = createEvid();
  mi->xe = createEvid();
  
  
  /* Determine whether mRNAs and ESTs support genomic or mRNA sequence in mismatch */
  searchTrans(conn, "mrna", mrna, rna, mi, strand, CODONSUB, cloneId);
  searchTrans(conn, "est", mrna, rna, mi, strand, CODONSUB, cloneId);
  if (xeno)
      {
      searchTrans(conn, "xenoMrna", mrna, rna, mi, strand, CODONSUB, cloneId);
      searchTrans(conn, "xenoEst", mrna, rna, mi, strand, CODONSUB, cloneId);
      }
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
		    mi = createMismatch(conn, pi->psl->qName, qstart+j, pi->psl->tName, tPosition+1, rna, pi->psl->strand, pi->mrnaCloneId);
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
                                              dCodon, rna, pi->psl->strand, pi->mrnaCloneId);
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
			  struct dnaSeq *rna, char *strand, struct clone *cloneId)
/* Create a record of an indel */
{
  struct indel *ni;
 
  AllocVar(ni);
  ni->next = NULL;
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
  ni->hs = createEvid();
  ni->xe = createEvid();
  
  
  /* Determine whether mRNAs and ESTs support genomic or mRNA sequence in indel region */
  searchTrans(conn, "mrna", mrna, rna, ni, strand, INDEL, cloneId);
  searchTrans(conn, "est", mrna, rna, ni, strand, INDEL, cloneId);
  if (xeno)
      {
      searchTrans(conn, "xenoMrna", mrna, rna, ni, strand, INDEL, cloneId);
      searchTrans(conn, "xenoEst", mrna, rna, ni, strand, INDEL, cloneId);
      }

  return(ni);
}

void cdsIndels(struct sqlConnection *conn, struct pslInfo *pi, struct dnaSeq *rna)
/* Find indels in coding region of mRNA alignment */
{
  int i, j;
  int unaligned=0, prevqend=0, prevtend=0, qstart=0, tstart=0;
  struct indel *ni, *niList=NULL, *uiList=NULL;
  int cdsS = pi->cdsStart, cdsE = pi->cdsEnd;
  boolean unali = FALSE;
  
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
	if (unaligned == (tstart - prevtend))
	    unali = TRUE;
	else
	    unali = FALSE;
	/* Check if unaligned part is a gap in the mRNA alignment, not an insertion */
	if (unaligned > 30)
	  pi->cdsGap += unaligned;
	/* Check if there is an indel */
	else if (unaligned > 0)
	  {
	    if (!unali)
	        {
	        if (unaligned == 1) 
	            pi->singleIndel++;
		if ((unaligned%3) == 0) 
	            pi->tripleIndel += unaligned/3;
	        pi->totalIndel += unaligned;
	        pi->indels[pi->indelCount] = unaligned;
	        pi->indelCount++;
		}
	    pi->unalignedCds += unaligned;
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
		ni = createIndel(conn, pi->psl->qName, prevqend, qstart, pi->psl->tName, prevtend, tstart, rna, pi->psl->strand, pi->mrnaCloneId); 
		if (unali)
		    slAddHead(&uiList, ni);
		else
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
	if (unaligned == (qstart - prevqend))
	    unali = TRUE;
	else
	    unali = FALSE;
	/* Check if unaligned part is an intron */
	if (unaligned > 30)
	  pi->cdsGap += unaligned;
	/* Check if there is an indel */
	else if ((unaligned != 0) && (!unali)) 
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
		ni = createIndel(conn, pi->psl->qName, prevqend, qstart, pi->psl->tName, prevtend, tstart, rna, pi->psl->strand, pi->mrnaCloneId); 
		slAddHead(&niList,ni);
		}
	  }
      }
  }
  if (indelReport)
      { 
      slReverse(&niList);
      pi->indelList = niList;
      slReverse(&uiList);
      pi->unaliList = uiList;
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
pi->mrnaCloneId = getMrnaCloneId(conn, psl->qName);

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

void writeList(FILE *of, struct indel *iList, int type, struct psl *psl)
/* Write out an list of indel/mismatches/codon subs*/
{
struct indel *indel;
struct acc *acc;

for (indel = iList; indel != NULL; indel=indel->next)
    {
    /*printf("Indel of size %d in %s:%d-%d vs. %s:%d-%d\n",
    indel->size, indel->mrna, indel->mrnaStart, indel->mrnaEnd,
    indel->chrom, indel->chromStart, indel->chromEnd);*/
    if (type == INDEL)
        fprintf(of, "Indel of size %d in %s:%d-%d vs. %s:%d-%d\n",
		indel->size, indel->mrna, indel->mrnaStart, indel->mrnaEnd,
		indel->chrom, indel->chromStart, indel->chromEnd);
    else if (type == UNALIGNED)
        fprintf(of, "Unaligned bases of size %d in %s:%d-%d vs. %s:%d-%d\n",
		indel->size, indel->mrna, indel->mrnaStart, indel->mrnaEnd,
		indel->chrom, indel->chromStart, indel->chromEnd);
    else if (type == MISMATCH)
	fprintf(of, "Mismatch at %s:%d vs. %s:%d\n",
	    indel->mrna, indel->mrnaStart, indel->chrom, indel->chromStart);
    else if (type == CODONSUB)
       {
       char mrnaAA = lookupCodon(indel->mrnaCodon);
       char genAA = lookupCodon(indel->genCodon);
       bool isSyn = (mrnaAA == genAA);
       fprintf(of, "%s codon substitution at %s:%d vs. %s:%d,%d,%d, %s vs. %s, ",
	       (isSyn ? "synonymous" : "non-synonymous"),
	       indel->mrna, indel->mrnaStart, indel->chrom, indel->codonGenPos[0],
	       indel->codonGenPos[1], indel->codonGenPos[2],
	       indel->mrnaCodon, indel->genCodon);
       if (mrnaAA == 0)
	   fprintf(of, "STOP vs. ");
       else
           fprintf(of, "%c vs. ", mrnaAA);
       if (genAA == 0)
           fprintf(of, "STOP\n");
       else
           fprintf(of, "%c\n", genAA);
       fprintf(of, "\tpsl %s %d %d\t%s %d %d\n",
	       psl->qName, psl->qStart, psl->qEnd,
	       psl->tName, psl->tStart, psl->tEnd);
       } 
    fprintf(of, "\t%d human mRNAs support genomic: ", indel->hs->genMrna);
    slReverse(&(indel->hs->genMrnaAcc));
    for (acc = indel->hs->genMrnaAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", acc->name);
    fprintf(of, "\n\t%d human ESTs support genomic: ",indel->hs->genEst);
    slReverse(&(indel->hs->genEstAcc));
    for (acc = indel->hs->genEstAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", acc->name);
    fprintf(of, "\n\t%d human mRNAs support %s: ", indel->hs->mrnaMrna, indel->mrna);
    slReverse(&(indel->hs->mrnaMrnaAcc));
    for (acc = indel->hs->mrnaMrnaAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", acc->name);
    fprintf(of, "\n\t%d human ESTs support %s: ",indel->hs->mrnaEst, indel->mrna);
    slReverse(&(indel->hs->mrnaEstAcc));
    for (acc = indel->hs->mrnaEstAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", acc->name);
    fprintf(of, "\n\t%d human mRNAs support neither: ", indel->hs->noMrna);
    slReverse(&(indel->hs->noMrnaAcc));
    for (acc = indel->hs->noMrnaAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", acc->name);
    fprintf(of, "\n\t%d human ESTs support neither: ",indel->hs->noEst);
    slReverse(&(indel->hs->noEstAcc));
    for (acc = indel->hs->noEstAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", acc->name);
    fprintf(of, "\n\t%d human mRNAs do not align: ", indel->hs->unMrna);
    slReverse(&(indel->hs->unMrnaAcc));
    for (acc = indel->hs->unMrnaAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", acc->name);
    fprintf(of, "\n\t%d human ESTs do not align: ",indel->hs->unEst);
    slReverse(&(indel->hs->unEstAcc));
    for (acc = indel->hs->unEstAcc; acc != NULL; acc = acc->next)
	fprintf(of, "%s ", acc->name);

    if (xeno)
        {
        fprintf(of, "\n\n\t%d xeno mRNAs support genomic: ", indel->xe->genMrna);
	slReverse(&(indel->xe->genMrnaAcc));
	for (acc = indel->xe->genMrnaAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", acc->name, acc->organism);
	fprintf(of, "\n\t%d xeno ESTs support genomic: ",indel->xe->genEst);
	slReverse(&(indel->xe->genEstAcc));
	for (acc = indel->xe->genEstAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", acc->name, acc->organism);
	fprintf(of, "\n\t%d xeno mRNAs support %s: ", indel->xe->mrnaMrna, indel->mrna);
	slReverse(&(indel->xe->mrnaMrnaAcc));
	for (acc = indel->xe->mrnaMrnaAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", acc->name, acc->organism);
	fprintf(of, "\n\t%d xeno ESTs support %s: ",indel->xe->mrnaEst, indel->mrna);
	slReverse(&(indel->xe->mrnaEstAcc));
	for (acc = indel->xe->mrnaEstAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", acc->name, acc->organism);
	fprintf(of, "\n\t%d xeno mRNAs support neither: ", indel->xe->noMrna);
	slReverse(&(indel->xe->noMrnaAcc));
	for (acc = indel->xe->noMrnaAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", acc->name, acc->organism);
	fprintf(of, "\n\t%d xeno ESTs support neither: ",indel->xe->noEst);
	slReverse(&(indel->xe->noEstAcc));
	for (acc = indel->xe->noEstAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", acc->name, acc->organism);
	fprintf(of, "\n\t%d xeno mRNAs do not align: ", indel->xe->unMrna);
	slReverse(&(indel->xe->unMrnaAcc));
	for (acc = indel->xe->unMrnaAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", acc->name, acc->organism);
	fprintf(of, "\n\t%d xeno ESTs do not align: ",indel->xe->unEst);
	slReverse(&(indel->xe->unEstAcc));
	for (acc = indel->xe->unEstAcc; acc != NULL; acc = acc->next)
	    fprintf(of, "%s(%s) ", acc->name, acc->organism);
	}
    fprintf(of, "\n\n");
    }
}

void writeOut(FILE *of, FILE *in, FILE *mm, FILE* cs, FILE* un, struct pslInfo *pi)
/* Output results of the mRNA alignment analysis */
{
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
        printf("Writing out indels and unaligned regions\n");
    writeList(in, pi->indelList, INDEL, NULL);
    writeList(un, pi->unaliList, UNALIGNED, NULL);
    }

/* Write out detailed records of mismatches, if requested */
if (mismatchReport) 
    {
    if (verbose)
        printf("Writing out mismatches\n");
    writeList(mm, pi->mmList, MISMATCH, NULL);
    }
/* Write out detailed records of codon substitutions, if requested */
if (codonSubReport) 
    {
    if (verbose)
        printf("Writing out codon substitutions\n");
    writeList(cs, pi->codonSubList, CODONSUB, pi->psl);
    }
}
 
void doFile(struct lineFile *pf, FILE *of, FILE *in, FILE *mm, FILE* cs, FILE* un)
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
    writeOut(of, in, mm, cs, un, pi);
    pslInfoFree(&pi);
    }
hFreeConn(&conn);
}

int main(int argc, char *argv[])
{
struct lineFile *pf, *cf, *lf;
FILE *of, *in=NULL, *mm=NULL, *cs=NULL, *un=NULL;
char *faFile, *db, filename[64];

optionInit(&argc, argv, optionSpecs);
if (argc != 6)
    {
    fprintf(stderr, "USAGE: pslAnal [-db=db] -verbose -xeno -indels -mismatches -codonsub <psl file> <cds file> <loci file> <fa file> <out file prefix>\n");
    return 1;
    }
db = optionVal("db", "hg15");
verbose = optionExists("verbose");
indelReport = optionExists("indels");
mismatchReport = optionExists("mismatches");
codonSubReport = optionExists("codonsub");
xeno = optionExists("xeno");
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
    sprintf(filename, "%s.unali", argv[5]);
    un = mustOpen(filename, "w");
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
doFile(pf, of, in, mm, cs, un);

if (indelReport)
    {
    fclose(in);
    fclose(un);
    }
if (mismatchReport)
    fclose(mm);
if (codonSubReport)
    fclose(cs);
fclose(of);
lineFileClose(&pf);

return 0;
}
