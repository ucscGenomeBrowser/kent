/* snpMaskGenes - Print gene sequences, exons only, with SNPs (single base substituions) using IUPAC codes. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "hdb.h"
#include "featureBits.h"


char *database = NULL;
char *chromName = NULL;

char geneTable[] = "refGene";

boolean strict = TRUE;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpMaskGenes - print gene sequences, exons only, \n"
    "using IUPAC codes for single base substitutions\n"
    "usage:\n"
    "    snpMaskGenes database chrom nib output\n");
}

struct iupacTable
/* IUPAC codes. */
/* Move to dnaUtil? */
    {
    DNA *observed;
    AA iupacCode;
    };

struct iupacTable iupacTable[] =
{
    {"A/C",     'M',},
    {"A/G",     'R',},
    {"A/T",     'W',},
    {"C/G",     'S',},
    {"C/T",     'Y',},
    {"G/T",     'K',},
    {"A/C/G",   'V',},
    {"A/C/T",   'H',},
    {"A/G/T",   'D',},
    {"C/G/T",   'B',},
    {"A/C/G/T", 'N',},
};

void printIupacTable()
{
int i = 0;
for (i = 0; i<11; i++)
    printf("code for %s is %c\n", iupacTable[i].observed, iupacTable[i].iupacCode);
}

AA lookupIupac(DNA *dna)
/* Given an observed string, return the IUPAC code. */
{
int i = 0;
for (i = 0; i<11; i++)
    if (sameString(dna, iupacTable[i].observed)) return iupacTable[i].iupacCode;
return '?';
}

DNA *lookupIupacReverse(AA aa)
/* Given an AA, return the observed string. */
{
int i = 0;
for (i = 0; i<11; i++)
    if(iupacTable[i].iupacCode == aa) return iupacTable[i].observed;
return "?";
}


struct snpSimple
/* Get only what is needed from snp. */
/* Assuming observed string is always in alphabetical order. */
/* Assuming observed string is upper case. */
    {
    struct snpSimple *next;
    char *name;			/* rsId  */
    int chromStart;       
    char strand;
    char *observed;	
    };

struct snpSimple *snpSimpleLoad(char **row)
/* Load a snpSimple from row fetched from snp
 * in database.  Dispose of this with snpSimpleFree(). 
   Complement observed if negative strand, preserving alphabetical order. */
{
struct snpSimple *ret;
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
    char c = ret->observed[i];
    obsComp[obsLen-i-1] = ntCompTable[(int)c];
    }

verbose(2, "negative strand detected for snp %s\n", ret->name);
verbose(2, "original observed string = %s\n", ret->observed);
verbose(2, "complemented observed string = %s\n", obsComp);

freeMem(ret->observed);
ret->observed=obsComp;
return ret;
}

void snpSimpleFree(struct snpSimple **pEl)
/* Free a single dynamically allocated snp such as created * with snpLoad(). */
{
struct snpSimple *el;

if ((el = *pEl) == NULL) return;
freeMem(el->name);
freeMem(el->observed);
freez(pEl);
}

void snpSimpleFreeList(struct snpSimple **pList)
/* Free a list of dynamically allocated snp's */
{
struct snpSimple *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    snpSimpleFree(&el);
    }
*pList = NULL;
}

// change to using genePredFreeList in genePred.h
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

struct snpSimple *readSnpsFromGene(struct genePred *gene, char *chrom)
/* read simple snps from txStart to txEnd */
/* later change this to cdsStart, cdsEnd */
{
struct snpSimple *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;

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
verbose(4, "query = %s\n", query);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = snpSimpleLoad(row);
    slAddHead(&list,el);
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
verbose(4, "Count of snps found = %d\n", count);
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
int count = 0;

sqlSafef(query, sizeof(query), "select * from %s where chrom='%s' ", geneTable, chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = genePredLoad(row);
    slAddHead(&list,el);
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
verbose(1, "Count of genes found = %d\n", count);
return list;
}

boolean isComplex(char mychar)
/* helper function: distinguish bases from IUPAC */
{
    if (mychar == 'N' || mychar == 'n') return TRUE;
    if (ntChars[(int)mychar] == 0) return TRUE;
    return FALSE;
}

char *observedBlend(char *obs1, char *obs2)
/* Used to handle dbSNP clustering errors with differing observed strings. */
/* For example, if there are 2 SNPs with observed strings A/C and A/G
   at the same location, return A/C/G. */
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

verbose(2, "observedBlend returning %s\n", t);
return t;
}


char iupac(char *name, char *observed, char orig)
{
    char *observed2;

    if (isComplex(orig)) 
        {
        observed2 = lookupIupacReverse(toupper(orig));
	if (!sameString(observed, observed2)) 
	    {
	    verbose(1, "differing observed strings %s, %s, %s\n", name, observed, observed2);
	    observed = observedBlend(observed, observed2);
	    verbose(1, "---------------\n");
	    }
	}

    return (lookupIupac(observed));
}



void printExons(struct genePred *gene, struct dnaSeq *seq, FILE *f)
/* print the sequence from the exons */
{
int exonPos = 0;
int exonStart = 0;
int exonEnd = 0;
int size = 0;
int total = 0;
struct dnaSeq *exonOnlySeq;
int offset = 0;

verbose(3, "exonCount = %d\n", gene->exonCount);

// get length of exons
for (exonPos = 0; exonPos < gene->exonCount; exonPos++)
    {
    exonStart = gene->exonStarts[exonPos] - gene->txStart;
    exonEnd   = gene->exonEnds[exonPos] - gene->txStart;
    size = exonEnd - exonStart;
    assert (size > 0);
    total += size;
    }

// modeled after hgSeq.c
AllocVar(exonOnlySeq);
exonOnlySeq->dna = needLargeMem(total+1);
exonOnlySeq->size = total;

offset = 0;
for (exonPos = 0; exonPos < gene->exonCount; exonPos++)
    {
    exonStart = gene->exonStarts[exonPos] - gene->txStart;
    exonEnd   = gene->exonEnds[exonPos] - gene->txStart;
    size = exonEnd - exonStart;
    verbose(4, "size = %d\n", size);
    memcpy(exonOnlySeq->dna+offset, seq->dna+exonStart, size);
    offset += size;
    }

assert(offset == exonOnlySeq->size);
exonOnlySeq->dna[offset] = 0;
faWriteNext(f, gene->name, exonOnlySeq->dna, exonOnlySeq->size);
freeDnaSeq(&exonOnlySeq);

}

void snpMaskGenes(char *nibFile, char *outFile)
/* snpMaskGenes - Print gene sequence, exons only, 
   using IUPAC codes for single base substitutions. */
{
struct genePred *genes = NULL;
struct genePred *gene = NULL;
struct dnaSeq *seq;
char *ptr;
struct snpSimple *snps = NULL;
struct snpSimple *snp = NULL;
int snpPos = 0;
int size = 0;
FILE *fileHandle = mustOpen(outFile, "w");

genes = readGenes(chromName);

for (gene = genes; gene != NULL; gene = gene->next)
    {
    verbose(4, "gene = %s\n", gene->name);

    snps = readSnpsFromGene(gene, chromName);

    size = gene->txEnd - gene->txStart;
    assert(size > 0);
    AllocVar(seq);
    seq->dna = needLargeMem(size+1);
    seq = nibLoadPartMasked(NIB_MASK_MIXED, nibFile, gene->txStart, size);

    ptr = seq->dna;

    /* do substitutions */
    /* including introns; doesn't take much time, keeps code clean */
    for (snp = snps; snp != NULL; snp = snp->next)
        {
	snpPos = snp->chromStart - gene->txStart;
	assert(snpPos >= 0);
	verbose(5, "before substitution %c\n", ptr[snpPos]);
        ptr[snpPos] = iupac(snp->name, snp->observed, ptr[snpPos]);
	verbose(5, "after substitution %c\n", ptr[snpPos]);
        }

    printExons(gene, seq, fileHandle);
    snpSimpleFreeList(&snps);
    dnaSeqFree(&seq);  
    }

geneFreeList(&genes);
if (fclose(fileHandle) != 0)
    errnoAbort("fclose failed");
}


int main(int argc, char *argv[])
/* Check args and call snpMaskGenes. */
{
if (argc != 5)
    usage();
database = argv[1];
if(!hDbExists(database))
    errAbort("%s does not exist\n", database);
hSetDb(database);
if(!hTableExistsDb(database, "snp"))
    errAbort("no snp table in %s\n", database);
chromName = argv[2];
if(hgOfficialChromName(chromName) == NULL)
    errAbort("no such chromosome %s in %s\n", chromName, database);
// check that nib file exists
// or, use hNibForChrom from hdb.c
snpMaskGenes(argv[3], argv[4]);
return 0;
}
