/* hapmap2 - 2nd step in processing: */
/* create hapmapSnpsCombined table, given 4 hapmapSnpsPop tables */
/* relies on hashes so will consume memory! */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* This program relies on passing around hashes, there might be a better way */

/* Check for multiple alignments when building hash for each pop. Not expecting any. */

#include "common.h"

#include "hapmapSnps.h"
#include "hapmapSnpsCombined.h"
#include "hash.h"
#include "hdb.h"
#include "linefile.h"
#include "jksql.h"


FILE *errorFileHandle = NULL;

static struct hash *nameHash = NULL;
static struct hash *hashCEU = NULL;
static struct hash *hashCHB = NULL;
static struct hash *hashJPT = NULL;
static struct hash *hashYRI = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hapmap2 - Read 4 simple population tables and merge.\n"
  "usage:\n"
  "  hapmap database \n");
}


void addIfNew(char *name)
{
struct hashEl *hel = hashLookup(nameHash, name);
if (hel == NULL)
    hashAdd(nameHash, cloneString(name), NULL);
}


struct hash *getHash(char *tableName)
/* read table, create hash */
{
struct hash *ret;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char query[64];
char **row = NULL;
struct hapmapSnps *loadItem = NULL;
struct hashEl *hel = NULL;

ret = newHash(18);

sqlSafef(query, sizeof(query), "select * from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    loadItem = hapmapSnpsLoad(row + 1);
    /* we want to capture all names in nameHash */
    addIfNew(loadItem->name);
    /* attempt to add to population hash */
    /* if failure, log multiple alignment */
    /* not bothering with error hash, so if something aligns 3 times it is logged twice, etc. */
    hel = hashLookup(ret, loadItem->name);
    if (hel)
        {
	fprintf(errorFileHandle, "multiple positions for %s\n", loadItem->name);
	continue;
	}
    hashAdd(ret, cloneString(loadItem->name), loadItem);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

void logMissing(char *name, struct hashEl *helCEU, struct hashEl *helCHB, 
                struct hashEl *helJPT, struct hashEl *helYRI)
{
if (!helCEU)
   fprintf(errorFileHandle, "%s missing from CEU\n", name);
if (!helCHB)
   fprintf(errorFileHandle, "%s missing from CHB\n", name);
if (!helJPT)
   fprintf(errorFileHandle, "%s missing from JPT\n", name);
if (!helYRI)
   fprintf(errorFileHandle, "%s missing from YRI\n", name);
}

boolean differentChrom(struct hashEl *hel1, struct hashEl *hel2)
/* if either is missing, we can't say these are different */
{
struct hapmapSnps *ha1, *ha2;
if (!hel1) return FALSE;
if (!hel2) return FALSE;
ha1 = (struct hapmapSnps *)hel1->val;
ha2 = (struct hapmapSnps *)hel2->val;
if (sameString(ha1->chrom, ha2->chrom)) return FALSE;
return TRUE;
}

boolean differentStrand(struct hashEl *hel1, struct hashEl *hel2)
/* if either is missing, we can't say these are different */
{
struct hapmapSnps *ha1, *ha2;
if (!hel1) return FALSE;
if (!hel2) return FALSE;
ha1 = (struct hapmapSnps *)hel1->val;
ha2 = (struct hapmapSnps *)hel2->val;
if (sameString(ha1->strand, ha2->strand)) return FALSE;
return TRUE;
}

boolean differentObserved(struct hashEl *hel1, struct hashEl *hel2)
/* if either is missing, we can't say these are different */
{
struct hapmapSnps *ha1, *ha2;
if (!hel1) return FALSE;
if (!hel2) return FALSE;
ha1 = (struct hapmapSnps *)hel1->val;
ha2 = (struct hapmapSnps *)hel2->val;
if (sameString(ha1->observed, ha2->observed)) return FALSE;
return TRUE;
}

boolean differentChromStart(struct hashEl *hel1, struct hashEl *hel2)
{
struct hapmapSnps *ha1, *ha2;
if (!hel1) return FALSE;
if (!hel2) return FALSE;
ha1 = (struct hapmapSnps *)hel1->val;
ha2 = (struct hapmapSnps *)hel2->val;
if (ha1->chromStart == ha2->chromStart) return FALSE;
return TRUE;
}

boolean matchingChroms(struct hashEl *helCEU, struct hashEl *helCHB, 
                       struct hashEl *helJPT, struct hashEl *helYRI)
/* returns TRUE if all 4 chroms are the same. */
{
if (differentChrom(helCEU, helCHB)) return FALSE;
if (differentChrom(helCEU, helJPT)) return FALSE;
if (differentChrom(helCEU, helYRI)) return FALSE;
if (differentChrom(helCHB, helJPT)) return FALSE;
if (differentChrom(helCHB, helYRI)) return FALSE;
if (differentChrom(helJPT, helYRI)) return FALSE;

return TRUE;
}

boolean matchingPositions(struct hashEl *helCEU, struct hashEl *helCHB, 
                          struct hashEl *helJPT, struct hashEl *helYRI)
/* returns TRUE if all 4 chroms are the same. */
{
if (differentChromStart(helCEU, helCHB)) return FALSE;
if (differentChromStart(helCEU, helJPT)) return FALSE;
if (differentChromStart(helCEU, helYRI)) return FALSE;
if (differentChromStart(helCHB, helJPT)) return FALSE;
if (differentChromStart(helCHB, helYRI)) return FALSE;
if (differentChromStart(helJPT, helYRI)) return FALSE;

return TRUE;
}

boolean matchingStrand(struct hashEl *helCEU, struct hashEl *helCHB, 
                       struct hashEl *helJPT, struct hashEl *helYRI)
/* returns TRUE if all 4 strands are the same. */
{
if (differentStrand(helCEU, helCHB)) return FALSE;
if (differentStrand(helCEU, helJPT)) return FALSE;
if (differentStrand(helCEU, helYRI)) return FALSE;
if (differentStrand(helCHB, helJPT)) return FALSE;
if (differentStrand(helCHB, helYRI)) return FALSE;
if (differentStrand(helJPT, helYRI)) return FALSE;

return TRUE;
}

boolean matchingObserved(struct hashEl *helCEU, struct hashEl *helCHB, 
                         struct hashEl *helJPT, struct hashEl *helYRI)
/* returns TRUE if all 4 observed are the same. */
{
if (differentObserved(helCEU, helCHB)) return FALSE;
if (differentObserved(helCEU, helJPT)) return FALSE;
if (differentObserved(helCEU, helYRI)) return FALSE;
if (differentObserved(helCHB, helJPT)) return FALSE;
if (differentObserved(helCHB, helYRI)) return FALSE;
if (differentObserved(helJPT, helYRI)) return FALSE;

return TRUE;
}

int getAlleleCount(char *allele, struct hashEl *helCEU, struct hashEl *helCHB, 
                   struct hashEl *helJPT, struct hashEl *helYRI)
/* return count of allele over all populations */
/* don't adjust for strand */
/* don't include heterozygous */
{
int count = 0;
struct hapmapSnps *ha = NULL;
if (helCEU)
    {
    ha = (struct hapmapSnps *)helCEU->val;
    if (sameString(ha->allele1, allele)) count = count + ha->homoCount1;
    if (sameString(ha->allele2, allele)) count = count + ha->homoCount2;
    }
if (helCHB)
    {
    ha = (struct hapmapSnps *)helCHB->val;
    if (sameString(ha->allele1, allele)) count = count + ha->homoCount1;
    if (sameString(ha->allele2, allele)) count = count + ha->homoCount2;
    }
if (helJPT)
    {
    ha = (struct hapmapSnps *)helJPT->val;
    if (sameString(ha->allele1, allele)) count = count + ha->homoCount1;
    if (sameString(ha->allele2, allele)) count = count + ha->homoCount2;
    }
if (helYRI)
    {
    ha = (struct hapmapSnps *)helYRI->val;
    if (sameString(ha->allele1, allele)) count = count + ha->homoCount1;
    if (sameString(ha->allele2, allele)) count = count + ha->homoCount2;
    }
return count;
}

boolean matchingAlleles(struct hashEl *helCEU, struct hashEl *helCHB, 
                        struct hashEl *helJPT, struct hashEl *helYRI)
/* returns FALSE if more than 2 alleles found overall */
{
int aCount = getAlleleCount("A", helCEU, helCHB, helJPT, helYRI);
int cCount = getAlleleCount("C", helCEU, helCHB, helJPT, helYRI);
int gCount = getAlleleCount("G", helCEU, helCHB, helJPT, helYRI);
int tCount = getAlleleCount("T", helCEU, helCHB, helJPT, helYRI);
int alleleCount = 0;

if (aCount > 0) alleleCount++;
if (cCount > 0) alleleCount++;
if (gCount > 0) alleleCount++;
if (tCount > 0) alleleCount++;

if (alleleCount > 2) return FALSE;

return TRUE;

}


boolean confirmAllele(struct hashEl *hel, char *allele)
{
struct hapmapSnps *ha;
if (!hel) return FALSE;
ha = (struct hapmapSnps *)hel->val;
if (sameString(ha->allele1, allele))
    {
    if (ha->heteroCount > 0) return TRUE;
    if (ha->homoCount1 > 0) return TRUE;
    }
if (sameString(ha->allele2, allele))
    {
    if (ha->heteroCount > 0) return TRUE;
    if (ha->homoCount2 > 0) return TRUE;
    }
return FALSE;
}

char *getAllele1(struct hashEl *helCEU, struct hashEl *helCHB, 
                 struct hashEl *helJPT, struct hashEl *helYRI)
/* return the first allele found */
{
int count = 0;

if (confirmAllele(helCEU, "A")) count++;
if (confirmAllele(helCHB, "A")) count++;
if (confirmAllele(helJPT, "A")) count++;
if (confirmAllele(helYRI, "A")) count++;
if (count > 0) return "A";

if (confirmAllele(helCEU, "C")) count++;
if (confirmAllele(helCHB, "C")) count++;
if (confirmAllele(helJPT, "C")) count++;
if (confirmAllele(helYRI, "C")) count++;
if (count > 0) return "C";

if (confirmAllele(helCEU, "G")) count++;
if (confirmAllele(helCHB, "G")) count++;
if (confirmAllele(helJPT, "G")) count++;
if (confirmAllele(helYRI, "G")) count++;
if (count > 0) return "G";

if (confirmAllele(helCEU, "T")) count++;
if (confirmAllele(helCHB, "T")) count++;
if (confirmAllele(helJPT, "T")) count++;
if (confirmAllele(helYRI, "T")) count++;
if (count > 0) return "T";

/* degenerate case, no data */
/* should never get here */
return "none";
}



char *getAllele2(char *allele1, struct hashEl *helCEU, struct hashEl *helCHB, 
                 struct hashEl *helJPT, struct hashEl *helYRI)
/* return the second allele found */
/* must be different from allele1 */
/* will return "?" if all populations are monomorphic */
{
int count = 0;

if (confirmAllele(helCEU, "A")) count++;
if (confirmAllele(helCHB, "A")) count++;
if (confirmAllele(helJPT, "A")) count++;
if (confirmAllele(helYRI, "A")) count++;
if (count > 0 && differentString(allele1, "A")) return "A";

count = 0;
if (confirmAllele(helCEU, "C")) count++;
if (confirmAllele(helCHB, "C")) count++;
if (confirmAllele(helJPT, "C")) count++;
if (confirmAllele(helYRI, "C")) count++;
if (count > 0 && differentString(allele1, "C")) return "C";

count = 0;
if (confirmAllele(helCEU, "G")) count++;
if (confirmAllele(helCHB, "G")) count++;
if (confirmAllele(helJPT, "G")) count++;
if (confirmAllele(helYRI, "G")) count++;
if (count > 0 && differentString(allele1, "G")) return "G";

count = 0;
if (confirmAllele(helCEU, "T")) count++;
if (confirmAllele(helCHB, "T")) count++;
if (confirmAllele(helJPT, "T")) count++;
if (confirmAllele(helYRI, "T")) count++;
if (count > 0 && differentString(allele1, "T")) return "T";

/* no allele found, all populations are monomorphic */
return "none";
}

int getHomoCount(struct hashEl *hel, char *allele)
{
struct hapmapSnps *ha = NULL;
if (!hel) return 0;
ha = (struct hapmapSnps *)hel->val;
if (sameString(ha->allele1, allele)) return ha->homoCount1;
if (sameString(ha->allele2, allele)) return ha->homoCount2;
return 0;
}

int getHeteroCount(struct hashEl *hel)
{
struct hapmapSnps *ha = NULL;
if (!hel) return 0;
ha = (struct hapmapSnps *)hel->val;
return ha->heteroCount;
}

char *reverseObserved(char *observed)
/* reverse complement simple observed string */
/* if complex, leave as is */
{
if (sameString(observed, "A/T")) return "A/T";
if (sameString(observed, "C/G")) return "C/G";

if (sameString(observed, "A/C")) return "G/T";
if (sameString(observed, "A/G")) return "C/T";
if (sameString(observed, "C/T")) return "A/G";
if (sameString(observed, "G/T")) return "A/C";

return observed;
}

struct hashEl *forcePositive(struct hashEl *hel)
/* convert to positive strand */
{
struct hapmapSnps *ha;
if (!hel) return NULL;
ha = (struct hapmapSnps *)hel->val;
if (sameString(ha->strand, "+")) return hel;
if (sameString(ha->allele2, "?"))
    {
    strcpy(ha->strand, "+");
    reverseComplement(ha->allele1, 1);
    ha->observed = reverseObserved(ha->observed);
    /* put it back in the hash element */
    hel->val = ha;
    return hel;
    }
char *allele1 = cloneString(ha->allele1);
int homoCount1 = ha->homoCount1;
char *allele2 = cloneString(ha->allele2);
int homoCount2 = ha->homoCount2;

reverseComplement(allele1, 1);
reverseComplement(allele2, 1);

strcpy(ha->allele1, allele2);
ha->homoCount1 = homoCount2;

strcpy(ha->allele2, allele1);
ha->homoCount2 = homoCount1;

strcpy(ha->strand, "+");
ha->observed = reverseObserved(ha->observed);
/* put it back in the hash element */
hel->val = ha;
return hel;

}



struct hapmapSnpsCombined *mergeOne(struct hashEl *helCEU, struct hashEl *helCHB, 
                                    struct hashEl *helJPT, struct hashEl *helYRI)
{
struct hapmapSnpsCombined *ret = NULL;
struct hapmapSnps *sample = NULL;
char *allele1, *allele2;

allele1 = getAllele1(helCEU, helCHB, helJPT, helYRI);
allele2 = getAllele2(allele1, helCEU, helCHB, helJPT, helYRI);

if (helCEU)
    sample = (struct hapmapSnps *)helCEU->val;
else if (helCHB)
    sample = (struct hapmapSnps *)helCHB->val;
else if (helJPT)
    sample = (struct hapmapSnps *)helJPT->val;
else if (helYRI)
    sample = (struct hapmapSnps *)helYRI->val;
else
    return NULL;

AllocVar(ret);
ret->chrom = cloneString(sample->chrom);
ret->chromStart = sample->chromStart;
ret->chromEnd = sample->chromEnd;
ret->name = cloneString(sample->name);
ret->score = 0;
strcpy(ret->strand, sample->strand);
ret->observed = cloneString(sample->observed);
/* allele1 is always a single character */
strcpy(ret->allele1, allele1);
ret->homoCount1CEU = getHomoCount(helCEU, allele1);
ret->homoCount1CHB = getHomoCount(helCHB, allele1);
ret->homoCount1JPT = getHomoCount(helJPT, allele1);
ret->homoCount1YRI = getHomoCount(helYRI, allele1);
ret->allele2 = cloneString(allele2);
ret->homoCount2CEU = getHomoCount(helCEU, allele2);
ret->homoCount2CHB = getHomoCount(helCHB, allele2);
ret->homoCount2JPT = getHomoCount(helJPT, allele2);
ret->homoCount2YRI = getHomoCount(helYRI, allele2);
ret->heteroCountCEU = getHeteroCount(helCEU);
ret->heteroCountCHB = getHeteroCount(helCHB);
ret->heteroCountJPT = getHeteroCount(helJPT);
ret->heteroCountYRI = getHeteroCount(helYRI);
return ret;
}

void writeOutput(struct hapmapSnpsCombined *hap, FILE *outputFileHandle)
/* output one hapmapSnpsCombined */
{
fprintf(outputFileHandle, "%s %d %d %s ", hap->chrom, hap->chromStart, hap->chromEnd, hap->name);
fprintf(outputFileHandle, "%d %s %s ", hap->score, hap->strand, hap->observed);
fprintf(outputFileHandle, "%s %d %d %d %d ", hap->allele1, hap->homoCount1CEU, hap->homoCount1CHB, hap->homoCount1JPT, hap->homoCount1YRI);
fprintf(outputFileHandle, "%s %d %d %d %d ", hap->allele2, hap->homoCount2CEU, hap->homoCount2CHB, hap->homoCount2JPT, hap->homoCount2YRI);
fprintf(outputFileHandle, "%d %d %d %d\n", hap->heteroCountCEU, hap->heteroCountCHB, hap->heteroCountJPT, hap->heteroCountYRI);
}	    

void showAllele(struct hashEl *hel)
/* helper function for debugging */
{
struct hapmapSnps *ha = NULL;
if (!hel) return;
ha = (struct hapmapSnps *)hel->val;
verbose(1, "name = %s\n", ha->name);
verbose(1, "score = %d\n", ha->score);
verbose(1, "strand = %s\n", ha->strand);
verbose(1, "allele1 = %s\n", ha->allele1);
verbose(1, "homoCount1 = %d\n", ha->homoCount1);
verbose(1, "allele2 = %s\n", ha->allele2);
verbose(1, "homoCount2 = %d\n", ha->homoCount2);
verbose(1, "heteroCount = %d\n", ha->heteroCount);
verbose(1, "------------------------\n");
}

struct hapmapSnpsCombined *fixStrandAndMerge(struct hashEl *helCEU, struct hashEl *helCHB, 
                                             struct hashEl *helJPT, struct hashEl *helYRI)
/* force everything to positive strand */
{
boolean allMatch = TRUE;
helCEU = forcePositive(helCEU);
helCHB = forcePositive(helCHB);
helJPT = forcePositive(helJPT);
helYRI = forcePositive(helYRI);
/* need a sanity check for alleles */
allMatch = matchingAlleles(helCEU, helCHB, helJPT, helYRI);
if (!allMatch) return NULL;
return mergeOne(helCEU, helCHB, helJPT, helYRI);
}

void mergeAll()
/* read through nameHash */
/* look up in 4 population hashes */
/* call logMissing to report on missing data */
/* check chrom, position, strand, observed */
{
char outputFileName[64];
FILE *outputFileHandle = NULL;
struct hashCookie cookie;
struct hashEl *nameHashElement = NULL;
struct hashEl *helCEU = NULL;
struct hashEl *helCHB = NULL;
struct hashEl *helJPT = NULL;
struct hashEl *helYRI = NULL;
boolean allMatch = TRUE;
struct hapmapSnpsCombined *hap = NULL;

safef(outputFileName, sizeof(outputFileName), "hapmapSnpsCombined.tab");
outputFileHandle = mustOpen(outputFileName, "w");

cookie = hashFirst(nameHash);

while ((nameHashElement = hashNext(&cookie)) != NULL)
    {
    helCEU = hashLookup(hashCEU, nameHashElement->name);
    helCHB = hashLookup(hashCHB, nameHashElement->name);
    helJPT = hashLookup(hashJPT, nameHashElement->name);
    helYRI = hashLookup(hashYRI, nameHashElement->name);
    /* should convert to instance of hapmapSnps here */

    logMissing(nameHashElement->name, helCEU, helCHB, helJPT, helYRI);

    allMatch = matchingChroms(helCEU, helCHB, helJPT, helYRI);
    if (!allMatch)
        {
	fprintf(errorFileHandle, "different chroms for %s\n", nameHashElement->name);
	continue;
	}

    allMatch = matchingPositions(helCEU, helCHB, helJPT, helYRI);
    if (!allMatch)
        {
	fprintf(errorFileHandle, "different positions for %s\n", nameHashElement->name);
	continue;
	}

    /* if strand is different, log and fix if possible */
    allMatch = matchingStrand(helCEU, helCHB, helJPT, helYRI);
    if (!allMatch)
        {
	fprintf(errorFileHandle, "different strands for %s\n", nameHashElement->name);
	hap = fixStrandAndMerge(helCEU, helCHB, helJPT, helYRI);
	if (hap)
	    writeOutput(hap, outputFileHandle);
	continue;
	}

    /* different observed is not a fatal error? */
    allMatch = matchingObserved(helCEU, helCHB, helJPT, helYRI);
    if (!allMatch)
        {
	fprintf(errorFileHandle, "different observed for %s\n", nameHashElement->name);
	continue;
	}

    /* should just have 2 alleles in all */
    allMatch = matchingAlleles(helCEU, helCHB, helJPT, helYRI);
    if (!allMatch)
        {
	fprintf(errorFileHandle, "different alleles for %s\n", nameHashElement->name);
	continue;
	}
        
    hap = mergeOne(helCEU, helCHB, helJPT, helYRI);
    if (hap) 
        writeOutput(hap, outputFileHandle);

    }
carefulClose(&outputFileHandle);
}

int main(int argc, char *argv[])
{
if (argc != 2)
    usage();

hSetDb(argv[1]);

errorFileHandle = mustOpen("hapmap2.errors", "w");

nameHash = newHash(18);
verbose(1, "building CEU hash...\n");
hashCEU = getHash("hapmapSnpsCEU");  
verbose(1, "building CHB hash...\n");
hashCHB = getHash("hapmapSnpsCHB");  
verbose(1, "building JPT hash...\n");
hashJPT = getHash("hapmapSnpsJPT");  
verbose(1, "building YRI hash...\n");
hashYRI = getHash("hapmapSnpsYRI");  

verbose(1, "merging...\n");
mergeAll();

// freeHash(&nameHash);
// freeHash(&hashCEU);

carefulClose(&errorFileHandle);

return 0;
}
