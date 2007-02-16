/* hapmapMixed -- compare mixed */
/* A little utility to read the hapmapAllelesCombined table */
/* It looks only at rows where all 4 populations have data and the major allele is mixed */
/* It reports counts where 3 populations have a certain allele and the 4th is different */

#include "common.h"

#include "hapmapAllelesCombined.h"
#include "hdb.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: hapmapMixed.c,v 1.1 2007/02/16 19:04:55 heather Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hapmapMixed - Read hapmapAllelesCombined, report counts of mixed.\n"
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


int getPopCount(struct hapmapAllelesCombined *hmac)
{
int ret = 0;
if (hmac->allele1CountCEU > 0 || hmac->allele2CountCEU > 0 || hmac->heteroCountCEU > 0) ret++;
if (hmac->allele1CountCHB > 0 || hmac->allele2CountCHB > 0 || hmac->heteroCountJPT > 0) ret++;
if (hmac->allele1CountJPT > 0 || hmac->allele2CountJPT > 0 || hmac->heteroCountCHB > 0) ret++;
if (hmac->allele1CountYRI > 0 || hmac->allele2CountYRI > 0 || hmac->heteroCountYRI > 0) ret++;
return ret;
}


boolean isMixed(struct hapmapAllelesCombined *hmac)
/* return TRUE if different populations have a different major allele */
/* don't need to consider heteroCount */
{
int allele1Count = 0;
int allele2Count = 0;

if (hmac->allele1CountCEU >= hmac->allele2CountCEU && hmac->allele1CountCEU > 0) 
    allele1Count++;
else if (hmac->allele2CountCEU > 0)
    allele2Count++;

if (hmac->allele1CountCHB >= hmac->allele2CountCHB && hmac->allele1CountCHB > 0) 
    allele1Count++;
else if (hmac->allele2CountCHB > 0)
    allele2Count++;

if (hmac->allele1CountJPT >= hmac->allele2CountJPT && hmac->allele1CountJPT > 0) 
    allele1Count++;
else if (hmac->allele2CountJPT > 0)
    allele2Count++;

if (hmac->allele1CountYRI >= hmac->allele2CountYRI && hmac->allele1CountYRI > 0) 
    allele1Count++;
else if (hmac->allele2CountYRI > 0)
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
struct hapmapAllelesCombined *hmac = NULL;
int popCount = 0;
char *majorAlleleCEU;
char *majorAlleleCHB;
char *majorAlleleJPT;
char *majorAlleleYRI;
int countCEU = 0;
int countCHB = 0;
int countJPT = 0;
int countYRI = 0;

safef(query, sizeof(query), "select * from hapmapAllelesCombined");

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hmac = hapmapAllelesCombinedLoad(row + 1);
    popCount = getPopCount(hmac);
    if (popCount != 4) continue;
    if (!isMixed(hmac)) continue;

    majorAlleleCEU = getMajorAllele(hmac->allele1, hmac->allele1CountCEU, 
                                    hmac->allele2, hmac->allele2CountCEU);
    majorAlleleCHB = getMajorAllele(hmac->allele1, hmac->allele1CountCHB, 
                                    hmac->allele2, hmac->allele2CountCHB);
    majorAlleleJPT = getMajorAllele(hmac->allele1, hmac->allele1CountJPT, 
                                    hmac->allele2, hmac->allele2CountJPT);
    majorAlleleYRI = getMajorAllele(hmac->allele1, hmac->allele1CountYRI, 
                                    hmac->allele2, hmac->allele2CountYRI);
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

if (!hTableExists("hapmapAllelesCombined")) 
    {
    verbose(1, "can't find hapmapAllelesCombined table \n");
    return 1;
    }

hapmapMixed();

return 0;
}
