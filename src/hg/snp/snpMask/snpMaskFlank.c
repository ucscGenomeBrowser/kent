/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


/* snpMaskFlank - Print sequences centered on SNPs, exons only, with SNPs (single base substituions) 
   using IUPAC codes. */
#include "common.h"
#include "binRange.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "hdb.h"
#include "featureBits.h"


char *database = NULL;
char *chromName = NULL;
struct hash *snpHash = NULL;
struct dnaSeq *seqAll;

char geneTable[] = "refGene";
char snpTable[] = "snp125";

#define FLANKSIZE 50
#define MAX_EXONS 150

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpMaskFlank - print sequences centered on SNPs, exons only, \n"
    "using IUPAC codes for single base substitutions\n"
    "usage:\n"
    "    snpMaskFlank database chrom nib outfile\n");
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
verbose(5, "lookupIupac unknown char %s\n", dna);
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
    char *func;
    };

struct snpSimple *snpSimpleLoad(char *name, int start, char *strand, char *observed, char *func)
/* Load a snpSimple from row fetched from snp
 * in database.  Dispose of this with snpSimpleFree(). 
   Complement observed if negative strand, preserving alphabetical order. */
{
struct snpSimple *ret;
int obsLen, i;
char *obsComp;

AllocVar(ret);
ret->name = cloneString(name);
ret->chromStart = start;
strcpy(&ret->strand, strand);
ret->observed = cloneString(observed);
ret->func = cloneString(func);

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

struct binKeeper *readSnps(char *chrom)
{
int chromSize = hChromSize(chrom);
struct binKeeper *ret = binKeeperNew(0, chromSize);
struct snpSimple *el = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;
int start = 0;
int end = 0;
char *class = NULL;
char *locType = NULL;

sqlSafef(query, sizeof(query), "select name, chromStart, chromEnd, class, "
    "locType, strand, observed, func from %s where chrom='%s' ", snpTable, chrom);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    class = cloneString(row[3]);
    if (!sameString(class, "single")) continue;
    locType = cloneString(row[4]);
    if (!sameString(locType, "exact")) continue;
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[2]);
    if (end != start + 1) continue;
    count++;
    el = snpSimpleLoad(row[0], start, row[5], row[6], row[7]);
    binKeeperAdd(ret, start, end, el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(5, "query = %s\n", query);
verbose(1, "Count of snps found = %d\n", count);
return ret;
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

verbose(1, "readGenes...\n");
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
verbose(1, "Count of genes found = %d\n\n", count);
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


/* return start coord in absolute coords, putting flankSize exonic bases prior to snp */
int findStartPos(int flankSize, int snpPos, struct genePred *gene, int exonPos)
{
    boolean firstExon = TRUE;
    int bases = 0;
    int basesNeeded = flankSize;
    int avail = 0, endPos = 0, exonStart = 0;

    assert (exonPos >= 0);
    assert (exonPos < gene->exonCount);
    assert (snpPos >= 0);
    assert (flankSize > 0);

    // probably can just fold this into the rest of the logic in here
    if (exonPos == 0)
        {
        endPos = snpPos;
        basesNeeded = flankSize;
	exonStart = gene->exonStarts[0];
	avail = endPos - exonStart;
	if (avail >= basesNeeded) return (endPos - basesNeeded);
	return (exonStart);
        }

    while (exonPos >= 0)
    {
        if (firstExon) endPos = snpPos;
        else endPos = gene->exonEnds[exonPos];
        firstExon = FALSE;
        basesNeeded = flankSize - bases;
	exonStart = gene->exonStarts[exonPos];
        avail = endPos - exonStart;
        if (avail >= basesNeeded) 
	    {
	    return (endPos - basesNeeded);
	    }
        bases = bases + avail;
        exonPos--;
    }
    /* beginning of gene */
    return gene->exonStarts[0];
}

/* return end coord in absolute coords, putting flankSize exonic bases after snp */
int findEndPos(int flankSize, int snpPos, struct genePred *gene, int exonPos)
{
    boolean firstExon = TRUE;
    int bases = 0;
    int basesNeeded = flankSize;
    int avail = 0, startPos = 0, exonEnd = 0;

    assert (exonPos >= 0);
    assert (exonPos < gene->exonCount);
    assert (snpPos >= 0);
    assert (flankSize > 0);

    if (exonPos == gene->exonCount - 1)
        {
	startPos = snpPos;
	basesNeeded = flankSize;
	exonEnd = gene->exonEnds[exonPos];
	avail = exonEnd - startPos;
	if (avail >= basesNeeded) 
	    return (startPos + basesNeeded);
	return (exonEnd);
        }

    while (exonPos <= gene->exonCount - 1)
        {
        if (firstExon) startPos = snpPos;
        else startPos = gene->exonStarts[exonPos];
        firstExon = FALSE;
        basesNeeded = flankSize - bases;
	exonEnd = gene->exonEnds[exonPos];
        avail = exonEnd - startPos;
        if (avail >= basesNeeded) 
	    return (startPos + basesNeeded);
        bases = bases + avail;
        exonPos++;
    }
    /* end of gene */
    return gene->exonEnds[gene->exonCount - 1];
}


int findExonPos(struct genePred *gene, int position)
/* find out which exon contains position */
/* write here first, then move to hg/lib/genePred.c */
{
int iExon = 0;
unsigned exonStart = 0;
unsigned exonEnd = 0;

for (iExon = 0; iExon < gene->exonCount; iExon++)
    {
    exonStart = gene->exonStarts[iExon];
    exonEnd = gene->exonEnds[iExon];
    assert (position >= exonStart);
    if (position <= exonEnd) return (iExon);
    }
    return -1;
}



/* calculate sequence size for exons given start and end in absolute coords */
int getExonSize(int startPos, int endPos, struct genePred *gene)
{
int startExon = 0, endExon = 0;
int seqSize = 0;
int exonPos = 0;

assert (endPos >= startPos);

startExon = findExonPos(gene, startPos);
assert (startExon >= 0);
assert (startExon < gene->exonCount);

endExon = findExonPos(gene, endPos);
assert (endExon >= 0);
assert (endExon < gene->exonCount);

if (startExon == endExon)
    return (endPos - startPos);

seqSize = gene->exonEnds[startExon] - startPos;
exonPos = startExon + 1;
while (exonPos < endExon)
{
    seqSize = seqSize + gene->exonEnds[exonPos] - gene->exonStarts[exonPos];
    exonPos++;
}

seqSize = seqSize + (endPos - gene->exonStarts[endExon]);
return (seqSize);
}


/* pass in array that has sequences for each exon */
/* start and end are absolute coords */
/* size excludes intronic regions */
struct dnaSeq *getFlankSeq(FILE *fileHandle, int start, int end, int size, 
                           struct genePred *gene, struct dnaSeq *exonSeqArray[])
{
struct dnaSeq *newSeq;
int startExon = findExonPos(gene, start);
int endExon = findExonPos(gene, end);
int exonPos = 0;
int exonSize = 0;
int offset = 0;
int seqPos = 0;

AllocVar(newSeq);
newSeq->dna = needMem(size + 1);

fprintf(fileHandle, "exons: ");
if (startExon == endExon)
    {
    verbose(1, "    single exonPos = %d\n", startExon);
    fprintf(fileHandle, "%d-%d\n", start, end);
    exonSize = end - start;
    assert (exonSize <= exonSeqArray[startExon]->size);
    offset = start - gene->exonStarts[startExon];
    memcpy(newSeq->dna, (exonSeqArray[startExon]->dna)+offset, exonSize);
    newSeq->dna[exonSize] = 0;
    newSeq->size = exonSize;
    return (newSeq);
    }

verbose(1, "    range of exonPos = %d to %d\n", startExon, endExon);
exonSize = gene->exonEnds[startExon] - start;
fprintf(fileHandle, "%d-%d,", gene->exonStarts[startExon], gene->exonEnds[startExon]); 
offset = start - gene->exonStarts[startExon];
memcpy(newSeq->dna, (exonSeqArray[startExon]->dna)+offset, exonSize);
seqPos = exonSize;

exonPos = startExon + 1;
while (exonPos < endExon)
    {
    fprintf(fileHandle, "%d-%d,", gene->exonStarts[exonPos], gene->exonEnds[exonPos]);
    exonSize = gene->exonEnds[exonPos] - gene->exonStarts[exonPos];
    assert (exonSize <= exonSeqArray[exonPos]->size);
    memcpy(newSeq->dna+seqPos, exonSeqArray[exonPos]->dna, exonSize);
    seqPos = seqPos + exonSize;
    exonPos++;
    }

exonSize = end - gene->exonStarts[endExon];
fprintf(fileHandle, "%d-%d\n", gene->exonStarts[endExon], gene->exonEnds[endExon]);
assert (exonSize <= exonSeqArray[endExon]->size);
memcpy(newSeq->dna+seqPos, exonSeqArray[endExon]->dna, exonSize);
newSeq->dna[size] = 0;
newSeq->size = size;
return (newSeq);
}

struct dnaSeq *getSeqFrag(int start, int size)
{
struct dnaSeq *seq;
DNA *dna = needMem(size+1);

memcpy(dna, seqAll->dna + start, size);

AllocVar(seq);
seq->dna = dna;
seq->size = size;

return seq;

}


void snpMaskFlank(char *nibFile, char *outFile)
/* snpMaskFlank - Print sequence, centered on SNP, exons only, 
   using IUPAC codes for single base substitutions. */
{
FILE *fileHandle = mustOpen(outFile, "w");
struct genePred *genes = NULL, *gene = NULL;
int exonPos = 0, exonStart = 0, exonEnd = 0, exonSize = 0;
struct snpSimple *thisSnp = NULL;
struct binKeeper *snps = NULL;
struct binElement *el, *elList = NULL;
struct dnaSeq *seqOrig, *seqMasked, *seqFlank;
char *ptr;
int snpPos = 0;
int flankStart = 0, flankEnd = 0, flankSize = 0;
struct dnaSeq *exonSequence[MAX_EXONS];
struct binElement *snpLists[MAX_EXONS];

// for short circuit
int geneCount = 0;

snps = readSnps(chromName);
genes = readGenes(chromName);

for (gene = genes; gene != NULL; gene = gene->next)
    {
    geneCount++;
    // short circuit goes here
    verbose(1, "gene %d = %s\n", geneCount, gene->name);

    /* create masked sequence and store to array */
    for (exonPos = 0; exonPos < gene->exonCount; exonPos++)
        {
        exonStart = gene->exonStarts[exonPos];
        exonEnd   = gene->exonEnds[exonPos];
        exonSize = exonEnd - exonStart;
        assert (exonSize > 0);

	elList = binKeeperFind(snps, exonStart, exonEnd);
	snpLists[exonPos] = elList;

	seqMasked = getSeqFrag(exonStart, exonSize);

        /* first do all substitutions */
	/* a flank may include other SNPs */
        ptr = seqMasked->dna;
	for (el = elList; el != NULL; el = el->next)
            {
	    thisSnp = (struct snpSimple *)el->val;
	    snpPos = thisSnp->chromStart - exonStart;
	    assert(snpPos >= 0);
	    verbose(5, "before substitution %c\n", ptr[snpPos]);
            ptr[snpPos] = iupac(thisSnp->name, thisSnp->observed, ptr[snpPos]);
	    verbose(5, "after substitution %c\n", ptr[snpPos]);
            }

        exonSequence[exonPos] = seqMasked;
        }

    /* print each snp */
    for (exonPos = 0; exonPos < gene->exonCount; exonPos++)
        {
        exonStart = gene->exonStarts[exonPos];
        exonEnd   = gene->exonEnds[exonPos];
	verbose(5, "exonPos = %d\n", exonPos);
        /* could save these from last time */
	elList = snpLists[exonPos];
	if (elList == NULL) continue;

	for (el = elList; el!= NULL; el = el->next)
	    {
            char *snpName = needMem(512);

            thisSnp = (struct snpSimple *)el->val;
	    snpPos = thisSnp->chromStart;

	    /* include reference allele in fasta header line */
            strcat(snpName, "");
	    strcat(snpName, thisSnp->name);
	    strcat(snpName, " (reference = ");
            /* could do a memcpy from sequence for full exon, potentially faster */
	    seqOrig = getSeqFrag(snpPos, 1);
	    ptr = seqOrig->dna;
	    if (sameString(gene->strand, "-"))
	        reverseComplement(seqOrig->dna, 1);
	    strcat(snpName, ptr);
	    strcat(snpName, ")");
	    if (sameString(gene->strand, "-"))
	      strcat(snpName, " (negative strand) ");
	    strcat(snpName, thisSnp->func);
	    verbose(1, "    snp = %s, snpPos = %d\n", snpName, snpPos);

            /* get flank start and end in absolute coords */
            flankStart = findStartPos(FLANKSIZE, snpPos, gene, exonPos);
            flankEnd = findEndPos(FLANKSIZE, snpPos, gene, exonPos);
            flankSize = getExonSize(flankStart, flankEnd, gene);
            fprintf(fileHandle, "flankStart = %d, flankEnd = %d\n", flankStart, flankEnd);
            seqFlank = getFlankSeq(fileHandle, flankStart, flankEnd, flankSize, 
	                           gene, exonSequence);
	    assert(flankSize == seqFlank->size);
	    if (sameString(gene->strand, "-"))
	        reverseComplement(seqFlank->dna, seqFlank->size);
	    faWriteNext(fileHandle, snpName, seqFlank->dna, flankSize);
	    freeDnaSeq(&seqFlank);

	    }
        }

        /* free memory in exonSequence */
	for (exonPos = 0; exonPos < gene->exonCount; exonPos++)
	    freeDnaSeq(&exonSequence[exonPos]);
    }

geneFreeList(&genes);
carefulClose(&fileHandle);
}


int main(int argc, char *argv[])
/* Check args and call snpMaskFlank. */
{
if (argc != 5)
    usage();
database = argv[1];
if(!hDbExists(database))
    errAbort("%s does not exist\n", database);
hSetDb(database);
if(!hTableExistsDb(database, snpTable))
    errAbort("no snp table in %s\n", database);
chromName = argv[2];
if(hgOfficialChromName(chromName) == NULL)
    errAbort("no such chromosome %s in %s\n", chromName, database);
// check that nib file exists
// or, use hNibForChrom from hdb.c
dnaUtilOpen();
verbose(1, "getting sequence\n");
seqAll = nibLoadPartMasked(NIB_MASK_MIXED, argv[3], 0, hChromSize(chromName));
snpMaskFlank(argv[3], argv[4]);
return 0;
}

