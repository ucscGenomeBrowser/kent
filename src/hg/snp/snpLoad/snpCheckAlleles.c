/* snpCheckAlleles
 * Write first standalone, once working
 * integrate into snpRefUCSC.
 *
 * Log 4 exceptions:
 *
 * 1) WrongRefAllelePositiveStrand
 *    orientation = 0
 *    and allele != refUCSC
 *
 *    Note an assymetry here: we are not checking
 *    allele against refUCSCReverseComp

 *    Note: these are also logged as RangeLocTypeWrongSizeLargeAllele
 *
 * 2) WrongRefAlleleNegativeStrand
 *    orientation = 1
 *    and allele != refUCSCReverseComp
 *    and allele != refUCSC
 *
 *    Hoping for none of these
 *   
 * 3) RefAlleleNotRevCompExactLocType
 *    orientation = 1
 *    and allele != refUCSCReverseComp
 *    and allele = refUCSC
 *    and loc_type = 2
 *
 * 4) RefAlleleNotRevCompRangeLocType
 *    orientation = 1
 *    and allele != refUCSCReverseComp
 *    and allele = refUCSC
 *    and loc_type = 1, 4, 5, 6
 *
 * Use UCSC chromInfo.  */


#include "common.h"

#include "chromInfo.h"
#include "dystring.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpCheckAlleles.c,v 1.4 2006/03/06 20:44:57 heather Exp $";

static char *snpDb = NULL;
static struct hash *chromHash = NULL;
FILE *exceptionFileHandle = NULL;


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpCheckAlleles - log allele exceptions\n"
    "usage:\n"
    "    snpCheckAlleles snpDb \n");
}


struct hash *loadChroms()
/* hash from UCSC chromInfo */
/* not using size */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *randomSubstring = NULL;
struct chromInfo *el;
char tableName[64];

ret = newHash(0);
safef(query, sizeof(query), "select chrom, size from chromInfo");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", row[0]);
    if (!hTableExists(tableName)) continue;
    el = chromInfoLoad(row);
    hashAdd(ret, el->chrom, (void *)(& el->size));
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

void writeToExceptionFile(char *chrom, char *start, char *end, char *name, char *exception)
{
fprintf(exceptionFileHandle, "%s\t%s\t%s\trs%s\t%s\n", chrom, start, end, name, exception);
}



void doCheckAlleles(char *chromName)
/* check each row for exceptions */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
int loc_type = 0;

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
if (!hTableExists(tableName)) return;

verbose(1, "chrom = %s\n", chromName);
safef(query, sizeof(query), 
    "select snp_id, chromStart, chromEnd, loc_type, orientation, allele, refUCSC, refUCSCReverseComp from %s where loc_type != 3", 
     tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    loc_type = sqlUnsigned(row[3]);
    /* check positive strand first */
    if (sameString(row[4], "0"))
        {
        if (sameString(row[5], row[6])) continue;
        writeToExceptionFile(chromName, row[1], row[2], row[0], "WrongRefAllelePositiveStrand");
	continue;
	}

    /* negative strand */
    /* ideally matches reverse comp */
    if (sameString(row[5], row[7])) continue;

    /* matching non-reverse comp is useful info */
    if (sameString(row[5], row[6]))
        {
	if (loc_type == 2)
            writeToExceptionFile(chromName, row[1], row[2], row[0], "RefAlleleNotRevCompExactLocType");
	else
            writeToExceptionFile(chromName, row[1], row[2], row[0], "RefAlleleNotRevCompRangeLocType");
        continue;
	}

    /* nothing matching */
    writeToExceptionFile(chromName, row[1], row[2], row[0], "WrongRefAlleleNegativeStrand");
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* read chrN_snpTmp, log exceptions */
{
struct hashCookie cookie;
struct hashEl *hel;
char *chromName;

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);

chromHash = loadChroms();
if (chromHash == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 1;
    }

exceptionFileHandle = mustOpen("snpCheckAlleles.exceptions", "w");

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    doCheckAlleles(chromName);

carefulClose(&exceptionFileHandle);
return 0;
}
