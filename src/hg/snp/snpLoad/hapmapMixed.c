/* hapmapMixed -- compare mixed */
/* A little utility to read the hapmapSnpsCombined table */
/* It looks only at rows where all 4 populations have data and the major allele is mixed */
/* It reports counts where 3 populations have a certain allele and the 4th is different */

#include "common.h"

#include "hapmapSnpsCombined.h"
#include "hdb.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: hapmapMixed.c,v 1.2 2007/03/14 20:33:56 heather Exp $";

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


int getPopCount(struct hapmapSnpsCombined *thisItem)
/* move this to kent/src/hg/lib */
{
int ret = 0;
if (thisItem->homoCount1CEU > 0 || thisItem->homoCount2CEU > 0 || thisItem->heteroCountCEU > 0) ret++;
if (thisItem->homoCount1CHB > 0 || thisItem->homoCount2CHB > 0 || thisItem->heteroCountJPT > 0) ret++;
if (thisItem->homoCount1JPT > 0 || thisItem->homoCount2JPT > 0 || thisItem->heteroCountCHB > 0) ret++;
if (thisItem->homoCount1YRI > 0 || thisItem->homoCount2YRI > 0 || thisItem->heteroCountYRI > 0) ret++;
return ret;
}


boolean isMixed(struct hapmapSnpsCombined *thisItem)
/* move this to kent/src/hg/lib */
/* return TRUE if different populations have a different major allele */
/* don't need to consider heteroCount */
{
int allele1Count = 0;
int allele2Count = 0;

if (thisItem->homoCount1CEU >= thisItem->homoCount2CEU && thisItem->homoCount1CEU > 0) 
    allele1Count++;
else if (thisItem->homoCount2CEU > 0)
    allele2Count++;

if (thisItem->homoCount1CHB >= thisItem->homoCount2CHB && thisItem->homoCount1CHB > 0) 
    allele1Count++;
else if (thisItem->homoCount2CHB > 0)
    allele2Count++;

if (thisItem->homoCount1JPT >= thisItem->homoCount2JPT && thisItem->homoCount1JPT > 0) 
    allele1Count++;
else if (thisItem->homoCount2JPT > 0)
    allele2Count++;

if (thisItem->homoCount1YRI >= thisItem->homoCount2YRI && thisItem->homoCount1YRI > 0) 
    allele1Count++;
else if (thisItem->homoCount2YRI > 0)
    allele2Count++;

if (allele1Count > 0 && allele2Count > 0) return TRUE;

return FALSE;
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

safef(query, sizeof(query), "select * from hapmapSnpsCombined");

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
