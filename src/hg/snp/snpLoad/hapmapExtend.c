/* hapmapExtend - report isMixed and isAllPops */
/* read the hapmapAllelesCombined table */

#include "common.h"

#include "hapmapAllelesCombined.h"
#include "hdb.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: hapmapExtend.c,v 1.1 2007/02/28 00:51:37 heather Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hapmapExtend - Read hapmapAllelesCombined, report isMixed and isAllPops.\n"
  "usage:\n"
  "  hapmapExtend database\n");
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


boolean checkMixture(struct hapmapAllelesCombined *hmac)
/* return TRUE if different populations have a different major allele */
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


void hapmapExtend()
/* read through input */
{
char query[64];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hapmapAllelesCombined *hmac = NULL;
int popCount = 0;
boolean isMixed = FALSE;
FILE *outputFileHandle = mustOpen("hapmapExtend.out", "w");

safef(query, sizeof(query), "select * from hapmapAllelesCombined");

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hmac = hapmapAllelesCombinedLoad(row + 1);
    isMixed = checkMixture(hmac);
    popCount = getPopCount(hmac);
    if (isMixed && popCount == 4)
        {
	fprintf(outputFileHandle, "%s\tTRUE\tTRUE\n", hmac->name);
	continue;
	}
    if (!isMixed && popCount == 4)
        {
	fprintf(outputFileHandle, "%s\tFALSE\tTRUE\n", hmac->name);
	continue;
	}
    if (isMixed && popCount < 4)
        {
	fprintf(outputFileHandle, "%s\tTRUE\tFALSE\n", hmac->name);
	continue;
	}
    if (!isMixed && popCount < 4)
        {
	fprintf(outputFileHandle, "%s\tFALSE\tFALSE\n", hmac->name);
	continue;
	}
    
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&outputFileHandle);
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

hapmapExtend();

return 0;
}
