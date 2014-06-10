/* snpMaskChrom - Print chromosome sequence using IUPAC codes for single base substitutions. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "fa.h"
#include "hdb.h"
#include "nib.h"


char *database = NULL;
char *chromName = NULL;

// make this an option
boolean strict = TRUE;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpMaskChrom - print chromosome sequence using IUPAC codes for single base substitutions\n"
    "usage:\n"
    "    snpMaskChrom database snpTable chrom nib output\n");
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
/* Load a snpSimple from row fetched from snp table
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
/* Free a single dynamically allocated snpSimple such as created with snpSimpleLoad(). */
{
struct snpSimple *el;

if ((el = *pEl) == NULL) return;
freeMem(el->name);
freeMem(el->observed);
freez(pEl);
}

void snpSimpleFreeList(struct snpSimple **pList)
/* Free a list of dynamically allocated snpSimple's */
{
struct snpSimple *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    snpSimpleFree(&el);
    }
*pList = NULL;
}


struct snpSimple *readSnpsFromChrom(char *tableName, char *chrom)
/* Slurp in the snpSimple rows for one chrom */
/* strict option is used to check size */
{
struct snpSimple *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int start = 0;
int end = 0;

sqlSafef(query, sizeof(query), 
    "select name, chromStart, strand, observed, chrom, chromEnd, class, locType from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!sameString(row[4], chrom)) continue;
    if (!sameString(row[6], "single")) continue;
    if (!sameString(row[7], "exact")) continue;
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[5]);
    if (strict && end != start + 1) continue;
    el = snpSimpleLoad(row);
    slAddHead(&list,el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
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
/* For example, if there are 2 SNPs with observed strings A/C and A/G at the same location,
   return A/C/G. */
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

verbose(1, "observedBlend returning %s\n", t);
return t;
}


char iupac(char *name, char *observed, char orig)
/* Return IUPAC value based on observed. */
/* Check for clustering failure; detect attempt to do different substitution in same position. */
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



void snpMaskChrom(char *tableName, char *nibFile, char *outFile)
/* snpMaskChrom - Print a nib file as a fasta file, using IUPAC codes for single base substitutions. */
{
struct dnaSeq *seq;
char *ptr;
struct snpSimple *snps = NULL;
struct snpSimple *snp = NULL;
boolean inRep = FALSE;

seq = nibLoadAllMasked(NIB_MASK_MIXED, nibFile);
ptr = seq->dna;
snps = readSnpsFromChrom(tableName, chromName);

/* do all substitutions */

for (snp = snps; snp != NULL; snp = snp->next)
    {
    if (islower(ptr[snp->chromStart])) inRep = TRUE;
    else inRep = FALSE;
    ptr[snp->chromStart] = iupac(snp->name, snp->observed, ptr[snp->chromStart]);
    if (inRep)
        ptr[snp->chromStart] = tolower(ptr[snp->chromStart]);
    }

faWrite(outFile, chromName, seq->dna, seq->size);
snpSimpleFreeList(&snps);
dnaSeqFree(&seq);  

}


int main(int argc, char *argv[])
/* Check args and call snpMaskChrom. */
{
char *tableName;

if (argc != 6)
    usage();
database = argv[1];
if(!hDbExists(database))
    errAbort("%s does not exist\n", database);
hSetDb(database);
tableName = argv[2];
if(!hTableExistsDb(database, tableName))
    errAbort("no %s table in %s\n", tableName, database);
chromName = argv[3];
if(hgOfficialChromName(chromName) == NULL)
    errAbort("no such chromosome %s in %s\n", chromName, database);
// check that nib file exists
// or, use hNibForChrom from hdb.c
snpMaskChrom(argv[2], argv[4], argv[5]);
return 0;
}
