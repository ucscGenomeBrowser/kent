/* hapmap - for each chrom, read a file with data from 5 populations and merge. */
/* Note: As of hg17, we don't have hapmap data for all chroms, just chr1-22 and chrX. */

/* clean up hashes between chroms or just leak memory like crazy? */

/* Include an overall hash to check for multiple alignments?  (on the same or different chrom) */
/* Not checking if the same SNP is reported more than once for the same population. */
/* In this case, just use the final values */

/* This is in place: confirm that r+o = n */

/* To do (here or elsewhere): confirm that JPT + CHB = combined */

/* If errors occur past the first population, move to errorHash */
/* Check errorHash first then */

#include "common.h"

#include "hash.h"
#include "hdb.h"
#include "linefile.h"

static char const rcsid[] = "$Id: hapmap.c,v 1.2 2007/01/11 18:28:50 heather Exp $";

FILE *errorFileHandle = NULL;
static char *db = NULL;
static struct hash *snpHash = NULL;
static struct hash *errorHash = NULL;

// strand, refAllele, otherAllele could be single char
struct snpInfo
    {
    char *chrom;
    int  position;
    char *strand;
    char *center;
    char *refAllele;
    char *otherAllele;

    int  refCountCEU;       
    int  refCountCHB;       
    int  refCountJPT;       
    int  refCountYRI;       
    int  otherCountCEU;       
    int  otherCountCHB;       
    int  otherCountJPT;       
    int  otherCountYRI;       
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hapmap - Read chrom files and merge over populations.\n"
  "usage:\n"
  "  hapmap database \n");
}

boolean validAllele(char *allele)
{
if (sameString(allele, "A")) return TRUE;
if (sameString(allele, "C")) return TRUE;
if (sameString(allele, "G")) return TRUE;
if (sameString(allele, "T")) return TRUE;
if (sameString(allele, "?")) return TRUE;
return FALSE;
}

boolean validPop(char *pop)
{
if (sameString(pop, "CEU")) return TRUE;
if (sameString(pop, "CHB")) return TRUE;
if (sameString(pop, "JPT")) return TRUE;
if (sameString(pop, "YRI")) return TRUE;
return FALSE;
}

void initCounts(struct snpInfo *si)
{
si->refCountCEU = 0;
si->otherCountCEU = 0;
si->refCountCHB = 0;
si->otherCountCHB = 0;
si->refCountJPT = 0;
si->otherCountJPT = 0;
}

void setCounts(char *pop, struct snpInfo *si, int refCount, int otherCount)
{
if (sameString(pop, "CEU"))
    {
    si->refCountCEU = refCount;
    si->otherCountCEU = otherCount;
    return;
    }

if (sameString(pop, "CHB"))
    {
    si->refCountCHB = refCount;
    si->otherCountCHB = otherCount;
    return;
    }

if (sameString(pop, "JPT"))
    {
    si->refCountJPT = refCount;
    si->otherCountJPT = otherCount;
    return;
    }

if (sameString(pop, "YRI"))
    {
    si->refCountYRI = refCount;
    si->otherCountYRI = otherCount;
    }
}

void readFile(char *chrom)
/* possible optimization: don't do hash lookup for first population */
{
char inputFileName[64];
struct lineFile *lf = NULL;
char *line;
char *row[13];
struct hashEl *hel = NULL;
char *rsId;
struct snpInfo *si = NULL;
int refCount;
int otherCount;
int total;

// double-check
safef(inputFileName, sizeof(inputFileName), "%s.hapmap", chrom);
if (!fileExists(inputFileName)) return;

lf = lineFileOpen(inputFileName, TRUE);

while (lineFileNext(lf, &line, NULL)) 
    {
    chopString(line, " ", row, ArraySize(row));
    /* allele should be 'A', 'C', 'G', 'T' or '?' */
    if (!validAllele(row[5]))
        {
        fprintf(errorFileHandle, "invalid refAllele %s for %s in chrom %s\n", row[5], row[1], chrom);
        continue;
	}
    if (!validAllele(row[7]))
        {
        fprintf(errorFileHandle, "invalid otherAllele %s for %s in chrom %s\n", row[7], row[1], chrom);
        continue;
	}

    /* check counts */
    /* don't put in error hash, counts could be fine for other populations */
    refCount = atoi(row[6]);
    otherCount = atoi(row[8]);
    total = atoi(row[9]);
    if (total != refCount + otherCount)
        {
        fprintf(errorFileHandle, "wrong counts for %s in pop %s, chrom %s\n", 
                row[1], row[0], chrom);
        fprintf(errorFileHandle, "refCount = %d, otherCount = %d, total = %d\n", 
                refCount, otherCount, total);
        continue;
        }

    /* if otherAllele is '?', then otherCount should be 0 */
    /* don't put in error hash, otherCount could be fine for other populations */
    if (sameString(row[7], "?") && otherCount != 0)
        {
        fprintf(errorFileHandle, "mismatched other data for %s in pop %s, chrom %s\n", 
                row[1], row[0], chrom);
        fprintf(errorFileHandle, "otherAllele = %s, otherCount = %d\n", 
                row[7], otherCount);
        continue;
	}

    /* check population */
    /* should be fine, I control this upstream */
    /* no need for error hash */
    if (!validPop(row[0]))
        {
        fprintf(errorFileHandle, "unknown pop %s for %s in chrom %s\n", row[0], row[1], chrom);
        continue;
	}

    rsId = cloneString(row[1]);

    /* If we've already encountered a problem with this rsId in another population, just skip */
    hel = hashLookup(errorHash, rsId);
    if (hel)
        continue;

    hel = hashLookup(snpHash, rsId);
    if (hel == NULL)
        {
	AllocVar(si);
	initCounts(si);
        si->chrom = cloneString(chrom);
        si->position = atoi(row[2]);
        si->strand = cloneString(row[3]);
        si->center = cloneString(row[4]);
        si->refAllele = cloneString(row[5]);
	/* force use of '?' for otherAllele */
	if (otherCount == 0)
	    si->otherAllele = cloneString("?");
	else
            si->otherAllele = cloneString(row[7]);
	setCounts(row[0], si, refCount, otherCount);
	hashAdd(snpHash, rsId, si);
	}
    else
        {
	si = (struct snpInfo *)hel->val;
        if (si->position != atoi(row[2]))
	    {
            fprintf(errorFileHandle, "multiple positions for %s in chrom %s\n", rsId, chrom);
	    hashRemove(snpHash, rsId);
	    hashAdd(errorHash, rsId, NULL);
            continue;
	    }
	if (differentString(si->strand, row[3]))
	    {
            fprintf(errorFileHandle, "different strands for %s in chrom %s\n", rsId, chrom);
	    hashRemove(snpHash, rsId);
	    hashAdd(errorHash, rsId, NULL);
            continue;
	    }
	if (differentString(si->center, row[4]))
	    {
            fprintf(errorFileHandle, "different centers for %s in chrom %s\n", rsId, chrom);
	    hashRemove(snpHash, rsId);
	    hashAdd(errorHash, rsId, NULL);
            continue;
	    }
	if (differentString(si->refAllele, row[5]))
	    {
            fprintf(errorFileHandle, "different refAlleles for %s in chrom %s\n", rsId, chrom);
	    hashRemove(snpHash, rsId);
	    hashAdd(errorHash, rsId, NULL);
            continue;
	    }
	if (otherCount != 0)
	    {
	    /* we have already encountered a valid otherAllele in a previous population */
            if (differentString(si->otherAllele, "?"))
	        {
	        /* Check that row[7] doesn't conflict with the previous other allele. */
	        /* If otherCount is nonZero, we know row[7] isn't '?' (we screened for that before hashLookup) */
	        if (differentString(si->otherAllele, row[7]))
	            {
	            fprintf(errorFileHandle, "different otherAlleles for %s in chrom %s\n", rsId, chrom);
	            hashRemove(snpHash, rsId);
	            hashAdd(errorHash, rsId, NULL);
	            }
		}
	    /* otherAllele is '?' (previous populations were monomorphic) */
	    /* So, this is the first time we encounter other allele and we save it. */
	    else
	        si->otherAllele = cloneString(row[7]);
	    }
	setCounts(row[0], si, refCount, otherCount);
	}
    }
lineFileClose(&lf);
}

void printOutput(char *chrom)
{
char outputFileName[64];
FILE *outputFileHandle = NULL;
struct hashCookie cookie;
struct hashEl *hel= NULL;
struct snpInfo *si;
char *rsId;

safef(outputFileName, sizeof(outputFileName), "%s.hapmap.new", chrom);
outputFileHandle = mustOpen(outputFileName, "w");

cookie = hashFirst(snpHash);

while ((hel = hashNext(&cookie)) != NULL)
    {
    si = (struct snpInfo *)hel->val;
    rsId = (char *)hel->name;
    fprintf(outputFileHandle, "%s\t", si->chrom);
    fprintf(outputFileHandle, "%d\t", si->position);
    fprintf(outputFileHandle, "%d\t", si->position + 1);
    fprintf(outputFileHandle, "%s\t", rsId);
    fprintf(outputFileHandle, "0\t");
    fprintf(outputFileHandle, "%s\t", si->strand);
    fprintf(outputFileHandle, "%s\t", si->refAllele);
    fprintf(outputFileHandle, "%s\t", si->otherAllele);
    fprintf(outputFileHandle, "%d\t", si->refCountCEU);
    fprintf(outputFileHandle, "%d\t", si->refCountCHB);
    fprintf(outputFileHandle, "%d\t", si->refCountJPT);
    fprintf(outputFileHandle, "%d\t", si->refCountYRI);
    fprintf(outputFileHandle, "%d\t", si->otherCountCEU);
    fprintf(outputFileHandle, "%d\t", si->otherCountCHB);
    fprintf(outputFileHandle, "%d\t", si->otherCountJPT);
    fprintf(outputFileHandle, "%d\n", si->otherCountYRI);
    }
carefulClose(&outputFileHandle);

}

int main(int argc, char *argv[])
{
struct slName *chromList, *chromPtr;
char inputFileName[64];
if (argc != 2)
    usage();

db = argv[1];
hSetDb(db);
chromList = hAllChromNamesDb(db);

errorFileHandle = mustOpen("hapmap.errors", "w");

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    safef(inputFileName, sizeof(inputFileName), "%s.hapmap", chromPtr->name);
    if (!fileExists(inputFileName)) continue;
    printf("%s\n", chromPtr->name);
    snpHash = newHash(16);  // max 300K SNPs in one chrom
    errorHash = newHash(0);  
    readFile(chromPtr->name);
    printOutput(chromPtr->name);
    // clean out snp and error hashes
    freeHash(&snpHash);
    freeHash(&errorHash);
    }
carefulClose(&errorFileHandle);

return 0;
}
