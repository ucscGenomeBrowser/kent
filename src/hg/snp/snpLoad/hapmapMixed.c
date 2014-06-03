/* hapmapMixed -- compare mixed */
/* A little utility to read the hapmapSnpsCombined table */
/* It looks only at rows where all 4 populations have data and the major allele is mixed */
/* It reports counts where 3 populations have a certain allele and the 4th is different */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "hapmapSnpsCombined.h"
#include "hdb.h"
#include "sqlNum.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hapmapMixed - Read hapmapSnpsCombined, report counts of mixed.\n"
  "usage:\n"
  "  hapmapMixed database\n");
}


char *getMajorAllele(char *allele1, int count1, char *allele2, int count2)
{
if (count1 == 0 && count2 == 0)
    return "?";
if (count1 >= count2)
    return allele1;
return allele2;
}

void hapmapMixed()
/* read through input */
{
char query[64];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hapmapSnpsCombined *thisItem = NULL;
int popCount = 0;
char *majorAlleleCEU;
char *majorAlleleCHB;
char *majorAlleleJPT;
char *majorAlleleYRI;
int countCEU = 0;
int countCHB = 0;
int countJPT = 0;
int countYRI = 0;

sqlSafef(query, sizeof(query), "select * from hapmapSnpsCombined");

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    thisItem = hapmapSnpsCombinedLoad(row + 1);
    popCount = getPopCount(thisItem);
    if (popCount != 4) continue;
    if (!isMixed(thisItem)) continue;

    majorAlleleCEU = getMajorAllele(thisItem->allele1, thisItem->homoCount1CEU, 
                                    thisItem->allele2, thisItem->homoCount2CEU);
    majorAlleleCHB = getMajorAllele(thisItem->allele1, thisItem->homoCount1CHB, 
                                    thisItem->allele2, thisItem->homoCount2CHB);
    majorAlleleJPT = getMajorAllele(thisItem->allele1, thisItem->homoCount1JPT, 
                                    thisItem->allele2, thisItem->homoCount2JPT);
    majorAlleleYRI = getMajorAllele(thisItem->allele1, thisItem->homoCount1YRI, 
                                    thisItem->allele2, thisItem->homoCount2YRI);
    if (sameString(majorAlleleCEU, majorAlleleCHB) && sameString(majorAlleleCHB, majorAlleleJPT))
        countYRI++;
    else if (sameString(majorAlleleCEU, majorAlleleCHB) && sameString(majorAlleleCHB, majorAlleleYRI))
        countJPT++;
    else if (sameString(majorAlleleCEU, majorAlleleJPT) && sameString(majorAlleleJPT, majorAlleleYRI))
        countCHB++;
    else if (sameString(majorAlleleCHB, majorAlleleJPT) && sameString(majorAlleleJPT, majorAlleleYRI))
        countCEU++;

    }

printf("countCEU = %d\n", countCEU);
printf("countCHB = %d\n", countCHB);
printf("countJPT = %d\n", countJPT);
printf("countYRI = %d\n", countYRI);
}


int main(int argc, char *argv[])
{

if (argc != 2)
    usage();

hSetDb(argv[1]);

if (!hTableExists("hapmapSnpsCombined")) 
    {
    verbose(1, "can't find hapmapSnpsCombined table \n");
    return 1;
    }

hapmapMixed();

return 0;
}
