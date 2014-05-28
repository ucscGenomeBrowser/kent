/* hapmapValidate -- after merging genotype files across SNPs, check: */
/* is everything biallelic? */
/* if valid observed, does it match? */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


#include "common.h"

#include "hdb.h"
#include "sqlNum.h"


FILE *errorFileHandle = NULL;
FILE *complexFileHandle = NULL;
static char *db = NULL;
struct dyString *dynamicObserved = NULL;

struct snpInfo
    {
    char *chrom;
    int  chromStart;
    int  chromEnd;
    char *name;
    char *strand;
    char *observed;

    int  aCountCEU;       
    int  cCountCEU;       
    int  gCountCEU;       
    int  tCountCEU;       

    int  aCountCHB;       
    int  cCountCHB;       
    int  gCountCHB;
    int  tCountCHB;

    int  aCountJPT;       
    int  cCountJPT;       
    int  gCountJPT;
    int  tCountJPT;

    int  aCountYRI;       
    int  cCountYRI;       
    int  gCountYRI;       
    int  tCountYRI;       
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hapmapValidate - Read initial hapmap table.\n"
  "usage:\n"
  "  hapmapValidate database table\n");
}

void load(char **row, struct snpInfo *si)
{
si->chrom = cloneString(row[1]);
si->chromStart = sqlUnsigned(row[2]);
si->chromEnd = sqlUnsigned(row[3]);
si->name = cloneString(row[4]);
si->strand = cloneString(row[6]);
si->observed = cloneString(row[7]);

si->aCountCEU = sqlUnsigned(row[8]);       
si->aCountCHB = sqlUnsigned(row[9]);       
si->aCountJPT = sqlUnsigned(row[10]);       
si->aCountYRI = sqlUnsigned(row[11]);       

si->cCountCEU = sqlUnsigned(row[12]);       
si->cCountCHB = sqlUnsigned(row[13]);       
si->cCountJPT = sqlUnsigned(row[14]);       
si->cCountYRI = sqlUnsigned(row[15]);       

si->gCountCEU = sqlUnsigned(row[16]);       
si->gCountCHB = sqlUnsigned(row[17]);
si->gCountJPT = sqlUnsigned(row[18]);
si->gCountYRI = sqlUnsigned(row[19]);       

si->tCountCEU = sqlUnsigned(row[20]);       
si->tCountCHB = sqlUnsigned(row[21]);
si->tCountJPT = sqlUnsigned(row[22]);
si->tCountYRI = sqlUnsigned(row[23]);       
}


void printOne(struct snpInfo *si)
{
verbose(1, "chrom = %s, name = %s, observed = %s\n", 
            si->chrom, si->name, si->observed);
}


char *actualObserved(struct snpInfo *si)
{
dyStringClear(dynamicObserved);
if (si->aCountCEU > 0 || si->aCountCHB > 0 ||
    si->aCountJPT > 0 || si->aCountYRI)
    dyStringAppend(dynamicObserved, "A");

if (si->cCountCEU > 0 || si->cCountCHB > 0 ||
    si->cCountJPT > 0 || si->cCountYRI)
    {
    if (dynamicObserved->stringSize > 0)
        dyStringAppend(dynamicObserved, "/");
    dyStringAppend(dynamicObserved, "C");
    }

if (si->gCountCEU > 0 || si->gCountCHB > 0 ||
    si->gCountJPT > 0 || si->gCountYRI)
    {
    if (dynamicObserved->stringSize > 0)
        dyStringAppend(dynamicObserved, "/");
    dyStringAppend(dynamicObserved, "G");
    }

if (si->tCountCEU > 0 || si->tCountCHB > 0 ||
    si->tCountJPT > 0 || si->tCountYRI)
    {
    if (dynamicObserved->stringSize > 0)
        dyStringAppend(dynamicObserved, "/");
    dyStringAppend(dynamicObserved, "T");
    }
return dynamicObserved->string;

}

boolean isComplexType(struct snpInfo *si)
{
if (sameString(si->observed, "A/C")) return FALSE;
if (sameString(si->observed, "A/G")) return FALSE;
if (sameString(si->observed, "A/T")) return FALSE;
if (sameString(si->observed, "C/G")) return FALSE;
if (sameString(si->observed, "C/T")) return FALSE;
if (sameString(si->observed, "G/T")) return FALSE;
return TRUE;
}

boolean isMonomorphic(struct snpInfo *si)
{
boolean gotA = FALSE;
boolean gotC = FALSE;
boolean gotG = FALSE;
boolean gotT = FALSE;
int alleleCount = 0;

if (si->aCountCEU > 0) gotA = TRUE;
if (si->aCountCHB > 0) gotA = TRUE;
if (si->aCountJPT > 0) gotA = TRUE;
if (si->aCountYRI > 0) gotA = TRUE;
if (gotA) alleleCount++;

if (si->cCountCEU > 0) gotC = TRUE;
if (si->cCountCHB > 0) gotC = TRUE;
if (si->cCountJPT > 0) gotC = TRUE;
if (si->cCountYRI > 0) gotC = TRUE;
if (gotC) alleleCount++;

if (si->gCountCEU > 0) gotG = TRUE;
if (si->gCountCHB > 0) gotG = TRUE;
if (si->gCountJPT > 0) gotG = TRUE;
if (si->gCountYRI > 0) gotG = TRUE;
if (gotG) alleleCount++;

if (si->tCountCEU > 0) gotT = TRUE;
if (si->tCountCHB > 0) gotT = TRUE;
if (si->tCountJPT > 0) gotT = TRUE;
if (si->tCountYRI > 0) gotT = TRUE;
if (gotT) alleleCount++;

if (alleleCount == 1) return TRUE;
return FALSE;

}

boolean allPopulations(struct snpInfo *si)
{
boolean gotCEU = FALSE;
boolean gotCHB = FALSE;
boolean gotJPT = FALSE;
boolean gotYRI = FALSE;
int popCount = 0;

if (si->aCountCEU > 0) gotCEU = TRUE;
if (si->cCountCEU > 0) gotCEU = TRUE;
if (si->gCountCEU > 0) gotCEU = TRUE;
if (si->tCountCEU > 0) gotCEU = TRUE;
if (gotCEU) popCount++;

if (si->aCountCHB > 0) gotCHB = TRUE;
if (si->cCountCHB > 0) gotCHB = TRUE;
if (si->gCountCHB > 0) gotCHB = TRUE;
if (si->tCountCHB > 0) gotCHB = TRUE;
if (gotCHB) popCount++;

if (si->aCountJPT > 0) gotJPT = TRUE;
if (si->cCountJPT > 0) gotJPT = TRUE;
if (si->gCountJPT > 0) gotJPT = TRUE;
if (si->tCountJPT > 0) gotJPT = TRUE;
if (gotJPT) popCount++;

if (si->aCountYRI > 0) gotYRI = TRUE;
if (si->cCountYRI > 0) gotYRI = TRUE;
if (si->gCountYRI > 0) gotYRI = TRUE;
if (si->tCountYRI > 0) gotYRI = TRUE;
if (gotYRI) popCount++;

if (popCount == 4) return TRUE;
return FALSE;
}

boolean isBiallelic(struct snpInfo *si)
{
if (sameString(si->observed,"A/C")) 
    {
    if (si->gCountCEU > 0) return FALSE;
    if (si->gCountCHB > 0) return FALSE;
    if (si->gCountJPT > 0) return FALSE;
    if (si->gCountYRI > 0) return FALSE;
    if (si->tCountCEU > 0) return FALSE;
    if (si->tCountCHB > 0) return FALSE;
    if (si->tCountJPT > 0) return FALSE;
    if (si->tCountYRI > 0) return FALSE;
    return TRUE;
    }
if (sameString(si->observed,"A/G")) 
    {
    if (si->cCountCEU > 0) return FALSE;
    if (si->cCountCHB > 0) return FALSE;
    if (si->cCountJPT > 0) return FALSE;
    if (si->cCountYRI > 0) return FALSE;
    if (si->tCountCEU > 0) return FALSE;
    if (si->tCountCHB > 0) return FALSE;
    if (si->tCountJPT > 0) return FALSE;
    if (si->tCountYRI > 0) return FALSE;
    return TRUE;
    }
if (sameString(si->observed,"A/T")) 
    {
    if (si->cCountCEU > 0) return FALSE;
    if (si->cCountCHB > 0) return FALSE;
    if (si->cCountJPT > 0) return FALSE;
    if (si->cCountYRI > 0) return FALSE;
    if (si->gCountCEU > 0) return FALSE;
    if (si->gCountCHB > 0) return FALSE;
    if (si->gCountJPT > 0) return FALSE;
    if (si->gCountYRI > 0) return FALSE;
    return TRUE;
    }
if (sameString(si->observed,"C/G")) 
    {
    if (si->aCountCEU > 0) return FALSE;
    if (si->aCountCHB > 0) return FALSE;
    if (si->aCountJPT > 0) return FALSE;
    if (si->aCountYRI > 0) return FALSE;
    if (si->tCountCEU > 0) return FALSE;
    if (si->tCountCHB > 0) return FALSE;
    if (si->tCountJPT > 0) return FALSE;
    if (si->tCountYRI > 0) return FALSE;
    return TRUE;
    }
if (sameString(si->observed,"C/T")) 
    {
    if (si->aCountCEU > 0) return FALSE;
    if (si->aCountCHB > 0) return FALSE;
    if (si->aCountJPT > 0) return FALSE;
    if (si->aCountYRI > 0) return FALSE;
    if (si->gCountCEU > 0) return FALSE;
    if (si->gCountCHB > 0) return FALSE;
    if (si->gCountJPT > 0) return FALSE;
    if (si->gCountYRI > 0) return FALSE;
    return TRUE;
    }
if (sameString(si->observed,"G/T")) 
    {
    if (si->aCountCEU > 0) return FALSE;
    if (si->aCountCHB > 0) return FALSE;
    if (si->aCountJPT > 0) return FALSE;
    if (si->aCountYRI > 0) return FALSE;
    if (si->cCountCEU > 0) return FALSE;
    if (si->cCountCHB > 0) return FALSE;
    if (si->cCountJPT > 0) return FALSE;
    if (si->cCountYRI > 0) return FALSE;
    return TRUE;
    }
return FALSE;
}

char *characterizeObserved(char *observed)
{
if (sameString(observed, "A")) return "monomorphic";
if (sameString(observed, "C")) return "monomorphic";
if (sameString(observed, "G")) return "monomorphic";
if (sameString(observed, "T")) return "monomorphic";
if (sameString(observed, "A/C")) return "bi-allelic";
if (sameString(observed, "A/G")) return "bi-allelic";
if (sameString(observed, "A/T")) return "bi-allelic";
if (sameString(observed, "C/G")) return "bi-allelic";
if (sameString(observed, "C/T")) return "bi-allelic";
if (sameString(observed, "G/T")) return "bi-allelic";
return "complex";
}


void hapmapValidate(char *tableName)
{
struct snpInfo *si = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *observedActual;

AllocVar(si);
sqlSafef(query, sizeof(query), "select * from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    load(row, si);
    // skip these for now
    if (isComplexType(si)) 
        {
	observedActual = actualObserved(si);
	fprintf(complexFileHandle, "%s %s %s\n", 
	    row[4], observedActual, characterizeObserved(observedActual));
        continue;
        }
    // if (!isBiallelic(si))
        // fprintf(errorFileHandle, "%s is not biallelic\n", si->name);
    // if (isMonomorphic(si))
        // fprintf(errorFileHandle, "%s is monomorphic\n", si->name);
    if (!allPopulations(si))
        fprintf(errorFileHandle, "%s not available for all populations\n", si->name);
    }
}


int main(int argc, char *argv[])
{
if (argc != 3)
    usage();

db = argv[1];
hSetDb(db);

if (!hTableExists(argv[2])) 
    {
    verbose(1, "can't find table %s\n", argv[2]);
    return 1;
    }

errorFileHandle = mustOpen("hapmapValidate.error", "w");
complexFileHandle = mustOpen("hapmapValidate.complex", "w");
dynamicObserved = newDyString(32);
hapmapValidate(argv[2]);
carefulClose(&errorFileHandle);
carefulClose(&complexFileHandle);

return 0;
}
