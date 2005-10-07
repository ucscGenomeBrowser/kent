/* snpMaskFlank - Print sequences centered on SNPs, exons only, with SNPs (single base substituions) 
   using IUPAC codes. */
#include "common.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "hdb.h"
#include "featureBits.h"

static char const rcsid[] = "$Id: snpMaskFlank.c,v 1.2 2005/09/23 23:28:29 heather Exp $";

char *database = NULL;
char *chromName = NULL;

char geneTable[] = "refGene";

#define FLANKSIZE 50

boolean strict = TRUE;

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
    obsComp[obsLen-i-1] = ntCompTable[c];
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

struct snpSimple *readSnps(char *chrom, int start, int end)
{
struct snpSimple *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;

if (strict)
    {
    safef(query, sizeof(query), "select name, chromStart, strand, observed from snp "
    "where chrom='%s' and chromEnd = chromStart + 1 and class = 'snp' and locType = 'exact' "
    "and chromStart >= %d and chromEnd <= %d", chrom, start, end);
    }
else
    {
    /* this includes snps that are larger than one base */
    safef(query, sizeof(query), "select name, chromStart, strand, observed from snp "
    "where chrom='%s' and class = 'snp' "
    "and chromStart >= %d and chromEnd <= %d", chrom, start, end);
    }
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
verbose(5, "query = %s\n", query);
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

safef(query, sizeof(query), "select * from %s where chrom='%s' ", geneTable, chrom);
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
    if (ntChars[mychar] == 0) return TRUE;
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




void snpMaskFlank(char *nibFile, char *outFile)
/* snpMaskFlank - Print sequence, centered on SNP, exons only, 
   using IUPAC codes for single base substitutions. */
{
FILE *fileHandle = mustOpen(outFile, "w");
struct genePred *genes = NULL, *gene = NULL;
int exonPos = 0, exonStart = 0, exonEnd = 0, exonSize = 0;
struct snpSimple *snps = NULL, *snp = NULL;
struct dnaSeq *seqOrig, *seqMasked, *seqFlank;
char *ptr;
int snpPos = 0;
int flankStart = 0, flankEnd = 0, flankSize = 0;

int geneCount = 0;

genes = readGenes(chromName);

for (gene = genes; gene != NULL; gene = gene->next)
    {
    verbose(1, "gene %d = %s\n----------\n", geneCount, gene->name);
    geneCount++;
    // short circuit
    if (geneCount == 100) return;

    for (exonPos = 0; exonPos < gene->exonCount; exonPos++)
        {
        exonStart = gene->exonStarts[exonPos];
        exonEnd   = gene->exonEnds[exonPos];
	verbose(1, "  exon %d coords %d-%d\n----------\n", exonPos+1, exonStart, exonEnd);
        exonSize = exonEnd - exonStart;
        assert (exonSize > 0);

        snps = readSnps(gene->chrom, exonStart, exonEnd);
	if (snps == NULL) continue;

        AllocVar(seqMasked);
        seqMasked->dna = needMem(exonSize+1);
        seqMasked = nibLoadPartMasked(NIB_MASK_MIXED, nibFile, exonStart, exonSize);

        /* first do all substitutions */
	/* a flank may include other SNPs */
        ptr = seqMasked->dna;
        for (snp = snps; snp != NULL; snp = snp->next)
            {
	    snpPos = snp->chromStart - exonStart;
	    assert(snpPos >= 0);
	    verbose(5, "before substitution %c\n", ptr[snpPos]);
            ptr[snpPos] = iupac(snp->name, snp->observed, ptr[snpPos]);
	    verbose(5, "after substitution %c\n", ptr[snpPos]);
            }

	/* print each snp */
	for (snp = snps; snp != NULL; snp = snp->next)
	    {
            char *snpName = needMem(64);
	    char referenceAllele;

	    snpPos = snp->chromStart;

	    /* include reference allele in fasta header line */
            strcat(snpName, "");
	    strcat(snpName, snp->name);
	    strcat(snpName, " (reference = ");
            seqOrig = nibLoadPartMasked(NIB_MASK_MIXED, nibFile, snpPos, 1);
	    ptr = seqOrig->dna;
	    strcat(snpName, ptr);
	    strcat(snpName, ")");
	    verbose(1, "    snp = %s, snpPos = %d\n", snpName, snpPos);

	    AllocVar(seqFlank);
	    flankStart = (snpPos - FLANKSIZE > exonStart) ? snpPos - FLANKSIZE - exonStart: 0;
	    flankEnd = (snpPos + FLANKSIZE < exonEnd) ? snpPos + FLANKSIZE - exonStart: exonEnd - exonStart - 1;
	    flankSize = flankEnd - flankStart;
            verbose(5, "flank: start = %d, end = %d, size = %d\n", flankStart, flankEnd, flankSize);
	    seqFlank->dna = needMem(flankSize + 1);
	    memcpy(seqFlank->dna, seqMasked->dna+flankStart, flankSize);
	    seqFlank->dna[flankSize] = 0;
	    seqFlank->size = flankSize;
	    verbose(5, "seq = %s\n", seqFlank->dna);
	    faWriteNext(fileHandle, snpName, seqFlank->dna, flankSize);
	    freeDnaSeq(&seqFlank);
	    }
        verbose(1, "----------------\n");
        snpSimpleFreeList(&snps);
        }
    }

geneFreeList(&genes);
if (fclose(fileHandle) != 0)
    errnoAbort("fclose failed");
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
if(!hTableExistsDb(database, "snp"))
    errAbort("no snp table in %s\n", database);
chromName = argv[2];
if(hgOfficialChromName(chromName) == NULL)
    errAbort("no such chromosome %s in %s\n", chromName, database);
// check that nib file exists
// or, use hNibForChrom from hdb.c
snpMaskFlank(argv[3], argv[4]);
return 0;
}
