/* snpMask - Print SNPs (single base substituions) using IUPAC codes and flanking sequences. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "hdb.h"
#include "featureBits.h"


#define FLANKSIZE 20

char *database = NULL;
char *chromName = NULL;

char geneTable[] = "refGene";

boolean snpMaskVerbose = FALSE;
boolean printSnps = FALSE;
boolean printGenes = FALSE;
boolean printChrom = TRUE;
boolean strict = FALSE;

// call this something else
struct snp
/* Information from snp */
    {
    struct snp *next;  	        /* Next in singly linked list. */
    char *name;			/* rsId  */
    int chromStart;               /* start   */
    char strand;
    char *observed;		/* observed variants (usually slash-separated list) */
    };

struct snp *snpLoad(char **row)
/* Load a snp from row fetched with select * from snp
 * from database.  Dispose of this with snpFree(). */
/* Complement observed if negative strand. */
{
struct snp *ret;
int obsLen, i;
char *obsComp;

AllocVar(ret);
ret->name = cloneString(row[0]);
ret->chromStart = atoi(row[1]);
strcpy(&ret->strand, row[2]);
ret->observed   = cloneString(row[3]);

if (ret->strand == '+') return ret;

obsLen = strlen(ret->observed);
obsComp = needMem(obsLen + 1);
strcpy(obsComp, ret->observed);
for (i = 0; i < obsLen; i = i+2)
    {
    if (ret->observed[i] == 'A') obsComp[obsLen-i-1] = 'T';
    else if (ret->observed[i] == 'T') obsComp[obsLen-i-1] = 'A';
    else if (ret->observed[i] == 'C') obsComp[obsLen-i-1] = 'G';
    else if (ret->observed[i] == 'G') obsComp[obsLen-i-1] = 'C';
    }

if (snpMaskVerbose)
    {
    printf("negative strand detected for snp %s\n", ret->name);
    printf("original observed string = %s\n", ret->observed);
    printf("complemented observed string = %s\n", obsComp);
}

ret->observed=obsComp;
return ret;
}

void snpFree(struct snp **pEl)
/* Free a single dynamically allocated snp such as created * with snpLoad(). */
{
struct snp *el;

if ((el = *pEl) == NULL) return;
freeMem(el->name);
freeMem(el->observed);
freez(pEl);
}

void snpFreeList(struct snp **pList)
/* Free a list of dynamically allocated snp's */
{
struct snp *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    snpFree(&el);
    }
*pList = NULL;
}

void geneFreeList(struct genePred **gList)
/* Free a list of dynamically allocated genePred's */
{
struct genePred *el, *next;

for (el = *gList; el != NULL; el = next)
    {
    next = el->next;
    genePredFree(&el);
    }
*gList = NULL;
}

struct snp *readSnpsFromGene(struct genePred *gene, char * chrom)
/* read simple snps from txStart to txEnd */
/* later change this to cdsStart, cdsEnd */
{
struct snp *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

if (strict)
    {
    sqlSafef(query, sizeof(query), "select name, chromStart, strand, observed from snp "
    "where chrom='%s' and chromEnd = chromStart + 1 and class = 'snp' and locType = 'exact' "
    "and chromStart >= %d and chromEnd <= %d", chrom, gene->txStart, gene->txEnd);
    }
else
    {
    /* this includes snps that are larger than one base */
    sqlSafef(query, sizeof(query), "select name, chromStart, strand, observed from snp "
    "where chrom='%s' and class = 'snp' "
    "and chromStart >= %d and chromEnd <= %d", chrom, gene->txStart, gene->txEnd);
    }
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = snpLoad(row);
    slAddHead(&list,el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
return list;
}


struct snp *readSnpsFromChrom(char *chrom)
/* Slurp in the snp rows for one chrom */
{
struct snp *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

if (strict)
    {
    sqlSafef(query, sizeof(query), "select name, chromStart, strand, observed from snp "
    "where chrom='%s' and chromEnd = chromStart + 1 and class = 'snp' and locType = 'exact'", chrom);
    }
else
    {
    /* this includes snps that are larger than one base */
    sqlSafef(query, sizeof(query), "select name, chromStart, strand, observed from snp "
    "where chrom='%s' and class = 'snp'", chrom);
    }
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = snpLoad(row);
    slAddHead(&list,el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
return list;
}

struct genePred *readGenes(char *chrom)
/* Slurp in the genes for one chrom */
{
struct genePred *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
sqlSafef(query, sizeof(query), "select * from %s where chrom='%s' ", geneTable, chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = genePredLoad(row);
    slAddHead(&list,el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
return list;
}

boolean isComplex(char mychar)
{
    if (mychar == 'A') return FALSE;
    if (mychar == 'a') return FALSE;
    if (mychar == 'C') return FALSE;
    if (mychar == 'c') return FALSE;
    if (mychar == 'G') return FALSE;
    if (mychar == 'g') return FALSE;
    if (mychar == 'T') return FALSE;
    if (mychar == 't') return FALSE;
    return TRUE;
}

char *iupacReverse(char mychar)
{
    if (mychar == 'M') return "A/C";
    if (mychar == 'R') return "A/G";
    if (mychar == 'W') return "A/T";
    if (mychar == 'S') return "C/G";
    if (mychar == 'Y') return "C/T";
    if (mychar == 'K') return "G/T";
    if (mychar == 'V') return "A/C/G";
    if (mychar == 'H') return "A/C/T";
    if (mychar == 'D') return "A/G/T";
    if (mychar == 'B') return "C/G/T";
    if (mychar == 'N') return "A/C/G/T";

    return "?";
}

char *observedBlend(char *obs1, char *obs2)
{

char *s1, *s2;
char *t = needMem(16);
int count = 0;

strcat(t, "");

s1 = strchr(obs1, 'A');
s2 = strchr(obs2, 'A');
if (s1 != NULL || s2 != NULL) 
    {
    strcat(t, "A");
    count++;
    }
s1 = strchr(obs1, 'C');
s2 = strchr(obs2, 'C');
if (s1 != NULL || s2 != NULL) 
    {
    if (count > 0) strcat(t, "/");
    strcat(t, "C");
    count++;
    }
s1 = strchr(obs1, 'G');
s2 = strchr(obs2, 'G');
if (s1 != NULL || s2 != NULL) 
    {
    if (count > 0) strcat(t, "/");
    strcat(t, "G");
    count++;
    }
s1 = strchr(obs1, 'T');
s2 = strchr(obs2, 'T');
if (s1 != NULL || s2 != NULL) 
    {
    if (count > 0) strcat(t, "/");
    strcat(t, "T");
    }

printf("observedBlend returning %s\n", t);
return t;
}


char iupac(char *name, char *observed, char orig)
{
    char *observed2;

    if (isComplex(orig)) 
        {
        observed2 = iupacReverse(orig);
	if (!sameString(observed, observed2)) 
	    {
	    printf("differing observed strings %s, %s, %s\n", name, observed, observed2);
	    observed = observedBlend(observed, observed2);
	    printf("---------------\n");
	    }
	}

    if (sameString(observed, "A/C")) return 'M';
    if (sameString(observed, "A/G"))  return 'R';
    if (sameString(observed, "A/T"))  return 'W';
    if (sameString(observed, "C/G"))  return 'S';
    if (sameString(observed, "C/T"))  return 'Y';
    if (sameString(observed, "G/T"))  return 'K';
    if (sameString(observed, "A/C/G"))  return 'V';
    if (sameString(observed, "A/C/T"))  return 'H';
    if (sameString(observed, "A/G/T"))  return 'D';
    if (sameString(observed, "C/G/T"))  return 'B';
    if (sameString(observed, "A/C/G/T"))  return 'N';

    return orig;
}

void testFB()
{
struct featureBits *fbList = NULL;
struct featureBits *fbPos = NULL;

// fbList = fbGetRange("snp", "rs12172168", 14430800, 14431000);
// fbList = fbGetRange("snp", "rs12172168", 0, 10);
fbList = fbGetRange("snp", chromName, 14430800, 14431000);

for (fbPos = fbList; fbPos != NULL; fbPos = fbPos->next)
    {
    printf("snp start = %d\n", fbPos->start);
    printf("snp end = %d\n", fbPos->end);
    }
}

void printSnpSeq(struct snp *snp, struct dnaSeq *seq)
{
int i = 0;
int startPos = (snp->chromStart) - FLANKSIZE;
int endPos = startPos + (FLANKSIZE*2);
char *ptr = seq->dna;
Bits bits;
struct featureBits *fbList = NULL;
struct featureBits *fbPos = NULL;

fbList = fbGetRange("refGene", chromName, startPos, endPos);

for (fbPos = fbList; fbPos != NULL; fbPos = fbPos->next)
    {
    printf("gene start = %d\n", fbPos->start);
    printf("gene end = %d\n", fbPos->end);
    }

printf("%s: ", snp->name);
for (i=startPos; i < endPos; i++)
  printf("%c", ptr[i]);
printf("\n");

}

// stolen from hgc.c
// put in common.c

void printLines(FILE *f, char *s, int lineSize)
/* Print s, lineSize characters (or less) per line. */
{
int len = strlen(s);
int start;
int oneSize;

for (start = 0; start < len; start += lineSize)
    {
    oneSize = len - start;
    if (oneSize > lineSize)
        oneSize = lineSize;
    mustWrite(f, s+start, oneSize);
    fputc('\n', f);
    }
if (start != len)
    fputc('\n', f);
}


// not remembering which exon has startPos
// move to genePred.c?

int findStartPos(int flankSize, int snpPos, struct genePred *gene, int exonPos)
{
    boolean firstExon = TRUE;
    int bases = 0;
    int basesNeeded = flankSize;
    int avail;
    int endPos;
    int exonStart;

    // arg checking
    if (exonPos <= 0) 
        {
	fprintf(stderr, "findStartPos: exonPos (%d) too small\n", exonPos);
	return -1;
	}
    if (exonPos > gene->exonCount)
        {
	fprintf(stderr, "findStartPos: exonPos (%d) exceeds exonCount(%d)\n", exonPos, gene->exonCount);
	return -1;
	}
    if (snpPos <= 0) return 0;
    if (flankSize <= 0) return snpPos;

    while (exonPos >= 1)
    {
        if (firstExon) endPos = snpPos;
        else endPos = gene->exonEnds[exonPos-1];
        firstExon = FALSE;
        basesNeeded = flankSize - bases;
	exonStart = gene->exonStarts[exonPos-1];
        avail = endPos - exonStart;
        if (avail >= basesNeeded) return (endPos - basesNeeded);
        bases = bases + avail;
        exonPos--;
    }
    // beginning of gene
    return gene->exonStarts[0];

}

// not remembering which exon has endPos
// move to genePred.c?

int findEndPos(int flankSize, int snpPos, struct genePred *gene, int exonPos)
{
    boolean firstExon = TRUE;
    int bases = 0;
    int basesNeeded = flankSize;
    int avail;
    int startPos;
    int exonEnd;

    // arg checking
    if (exonPos <= 0 || exonPos > gene->exonCount) return -1;
    if (snpPos <= 0) return 0;
    if (flankSize <= 0) return snpPos;

    while (exonPos <= gene->exonCount)
    {
        if (firstExon) startPos = snpPos;
        else startPos = gene->exonStarts[exonPos-1];
        firstExon = FALSE;
        basesNeeded = flankSize - bases;
	exonEnd = gene->exonEnds[exonPos-1];
        avail = exonEnd - startPos;
        if (avail >= basesNeeded) return (startPos + basesNeeded);
        bases = bases + avail;
        exonPos++;
    }
    // beginning of gene
    return gene->exonEnds[gene->exonCount - 1];

}

/* write here first, then move to hg/lib/genePred.c */
int findExonPos(struct genePred *gene, int position)
/* find out which exon contains position */
{
int iExon = 0;
unsigned exonStart = 0;
unsigned exonEnd = 0;

for (iExon = 0; iExon < gene->exonCount; iExon++)
    {
    exonStart = gene->exonStarts[iExon];
    exonEnd = gene->exonEnds[iExon];
    if (position < exonStart) 
        {
        // fprintf(stderr, "couldn't find position (%d) in gene (%s)\n", position, gene->name);
	return -1;
	}
    // start counting exons from 1
    if (position <= exonEnd) return (iExon + 1);
    }
    // fprintf(stderr, "couldn't find position (%d) in gene (%s)\n", position, gene->name);
    return -1;
}

/* write here first, then move to hg/lib/genePred.c */
void printExons(char *snpName, struct genePred *gene, char *chrom, int start, int end, struct dnaSeq *seq)
{
int iExon = 0;
// which exon 
int startExon = 0;
int endExon = 0;
struct dyString *dy = NULL;
char *seqBuffer;
int size;
char *ptr = seq->dna;
// actual nucleotide positions
int exonStart = 0;
int exonEnd = 0;

// arg checking
if (start > end) 
    {
    fprintf(stderr, "error with %s (start exceeds end)\n", gene->name);
    return;
    }

startExon = findExonPos(gene, start);
endExon = findExonPos(gene, end);

// more checking
if (startExon == -1 || endExon == -1) 
    {
    fprintf(stderr, "error with %s (startExon = %d; endExon = %d)\n", gene->name, startExon, endExon);
    return;
    }

// simple case
if (startExon == endExon)
    {
    // printf("simple case; all in one exon\n");
    size = end - start + 1;
    seqBuffer = needMem(size);
    strncpy(seqBuffer, &ptr[start], size);
    printf("> %s %s:%d-%d (%s)\n", gene->name, chrom, start, end, snpName);
    printLines(stdout, seqBuffer, 50);
    freeMem(seqBuffer);
    // *seqBuffer = 0;
    return;
    }

// printf("not simple case; flank in multiple exons\n");
// printf("startExon = %d; endExon = %d\n", startExon, endExon);
// append to dyString
dy = newDyString(512);

// remainder of first exon
exonEnd = gene->exonEnds[startExon-1];
size = exonEnd - start + 1;
seqBuffer = needMem(size);
strncpy(seqBuffer, &ptr[start], size);
dyStringPrintf(dy, "%s", seqBuffer);
freeMem(seqBuffer);

// middle exons
for (iExon = startExon + 1; iExon < endExon; iExon++)
    {
        exonStart = gene->exonStarts[iExon-1];
        exonEnd = gene->exonEnds[iExon-1];
	size = exonEnd - exonStart + 1;
	seqBuffer = needMem(size);
	strncpy(seqBuffer, &ptr[exonStart], size);
        dyStringPrintf(dy, "%s", seqBuffer);
        freeMem(seqBuffer);
    }

// start of last exon
exonStart = gene->exonStarts[endExon-1];
size = end - exonStart + 1;
seqBuffer = needMem(size);
strncpy(seqBuffer, &ptr[exonStart], size);
dyStringPrintf(dy, "%s", seqBuffer);
freeMem(seqBuffer);

printf("> %s %s:%d-%d (%s)\n", gene->name, chrom, start, end, snpName);
printLines(stdout, dy->string, 50);

dyStringFree(&dy);

}

void doPrintGenes(char *chromName, struct dnaSeq *seq)
{
struct genePred *genes = NULL;
struct genePred *gene = NULL;
struct snp *snps = NULL;
struct snp *snp = NULL;
int startPos = 0;
int endPos = 0;
int exonPos = 0;

genes = readGenes(chromName);

for (gene = genes; gene != NULL; gene = gene->next)
    {
    // printf("gene = %s %s:%d-%d\n", gene->name, chromName, gene->txStart, gene->txEnd);
    snps = readSnpsFromGene(gene, chromName);
    for (snp = snps; snp != NULL; snp = snp->next)
        {
        // for each SNP, figure out flanking start and stop
        // first figure out which exon contains the snp
        exonPos = findExonPos(gene, snp->chromStart);
        // exonPos will return -1 if the position isn't in any exon
        if (exonPos == -1) continue;
        // exonPos should also never be 0
        if (exonPos == 0) continue;
        // printf("  snp %s at %d\n", snp->name, snp->chromStart);
        // printf("  exonPos = %d\n", exonPos);
        startPos = findStartPos(FLANKSIZE, snp->chromStart, gene, exonPos);
        endPos = findEndPos(FLANKSIZE, snp->chromStart, gene, exonPos);
        // printf("flank start = %d, flank end = %d\n", startPos, endPos);
        printExons(snp->name, gene, chromName, startPos, endPos, seq);
        }
    snpFreeList(&snps);
    }

geneFreeList(&genes);

}

void snpMask(char *nibFile, char *outFile)
/* snpMask - Print a nib file, using IUPAC codes for single base substitutions. */
{
struct dnaSeq *seq;
char *ptr;
struct snp *snps = NULL;
struct snp *snp = NULL;

seq = nibLoadAllMasked(NIB_MASK_MIXED, nibFile);
ptr = seq->dna;
snps = readSnpsFromChrom(chromName);
printf("got all snps in %s\n", chromName);

/* do all substitutions */
for (snp = snps; snp != NULL; snp = snp->next)
    {
    ptr[snp->chromStart] = iupac(snp->name, snp->observed, ptr[snp->chromStart]);
    }

if (printSnps)
    {
    for (snp = snps; snp != NULL; snp = snp->next)
        {
        printSnpSeq(snp, seq);
        }
    }

if (printChrom)
    faWrite(outFile, chromName, seq->dna, seq->size);

snpFreeList(&snps);

if (printGenes) doPrintGenes(chromName, seq);

dnaSeqFree(&seq);  

}


int main(int argc, char *argv[])
/* Process command line. */
/* argv[1] is database */
/* argv[2] is chrom */
/* argv[3] is nib file */
/* argv[4] is output file */
{
database = argv[1];
if (!hDbIsActive(database))
    {
    printf("Currently no support for %s\n", database);
    return -1;
    }
hSetDb(database);
chromName = argv[2];
// testFB();
snpMask(argv[3], argv[4]);
return 0;
}
