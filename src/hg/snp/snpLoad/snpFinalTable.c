/* snpFinalTable - create chrN_snp125 tables from chrN_snpTmp tables. */
/* Also create exceptions table from exceptions files. */
/* Get chroms from UCSC chromInfo. */

/* Reorder columns. */
/* Add bin. */
/* Add 'rs' in front of snp_id. */
/* Decrease chromStart and chromEnd from int(11) to int(10). */
/* Add score for BED format (always 0). */
/* Rename orientation --> strand. */
/* Observed can be varchar(32). */
/* Expand validation_status. */
/* Func okay as is??!! */
/* Drop refUCSCReverseComp. */

#include "common.h"

#include "chromInfo.h"
#include "dystring.h"
#include "hash.h"
#include "hdb.h"
#include "jksql.h"
#include "snp125.h"

static char const rcsid[] = "$Id: snpFinalTable.c,v 1.1 2006/02/23 01:10:00 heather Exp $";

static char *snpDb = NULL;
static struct hash *chromHash = NULL;
FILE *outputFileHandle = NULL;

char *strandStrings[] = {
"+",
"-",
};


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpFinalTable - create chrN_snp125 tables from chrN_snpTmp tables.\n"
    "Also create exceptions table from exceptions files.\n"
    "usage:\n"
    "    snpFinalTable snpDb\n");
}

struct hash *loadChroms()
/* hash from UCSC chromInfo */
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
    randomSubstring = strstr(row[0], "random");
    if (randomSubstring != NULL) continue;
    safef(tableName, ArraySize(tableName), "%s_snpTmp", row[0]);
    if (!hTableExists(tableName)) continue;
    el = chromInfoLoad(row);
    hashAdd(ret, el->chrom, (void *)(& el->size));
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

char *validString(int validCode)
{
switch (validCode)
    {
    case 0:
        return "unknown";
    case 1:
        return "by-cluster";
    case 2:
        return "by-frequency";
    case 3:
        return "by-cluster,by-frequency";
    case 4:
        return "by-submitter";
    case 5:
        return "by-submitter,by-cluster";
    case 6:
        return "by-submitter,by-frequency";
    case 7:
        return "by-submitter";
    case 8:
        return "by-2hit-2allele";
    case 9:
        return "by-2hit-2allele,by-cluster";
    case 10:
        return "by-2hit-2allele,by-frequency";
    case 11:
        return "by-2hit-2allele,by-frequency,by-cluster";
    case 12:
        return "by-2hit-2allele,by-submitter";
    case 13:
        return "by-2hit-2allele,by-submitter,by-cluster";
    case 14:
        return "by-2hit-2allele,by-submitter,by-frequency";
    case 15:
        return "by-2hit-2allele,by-submitter,by-frequency,by-cluster";
    case 16:
        return "by-hapmap";
    case 17:
        return "by-hapmap,by-cluster";
    case 18:
        return "by-hapmap,by-frequency";
    case 19:
        return "by-hapmap,by-frequency,by-cluster";
    case 20:
        return "by-hapmap,by-submitter";
    case 21:
        return "by-hapmap,by-submitter,by-cluster";
    case 22:
        return "by-hapmap,by-submitter,by-frequency";
    case 23:
        return "by-hapmap,by-submitter,by-frequency,by-cluster";
    case 24:
        return "by-hapmap,by-2hit-2allele";
    case 25:
        return "by-hapmap,by-2hit-2allele,by-cluster";
    case 26:
        return "by-hapmap,by-2hit-2allele,by-frequency";
    case 27:
        return "by-hapmap,by-2hit-2allele,by-frequency,by-cluster";
    case 28:
        return "by-hapmap,by-2hit-2allele,by-submitter";
    case 29:
        return "by-hapmap,by-2hit-2allele,by-submitter,by-cluster";
    case 30:
        return "by-hapmap,by-2hit-2allele,by-submitter,by-frequency";
    case 31:
        return "by-hapmap,by-2hit-2allele,by-submitter,by-frequency,by-cluster";
    default:
        return "unknown";
    }
}


void readSnps(char *chromName)
/* read chrN_snpTmp */
/* write to chrN_snp125.tab */

/*  0: snp_id               int(11)       */
/*  1: chromStart           int(11)       */
/*  2: chromEnd             int(11)       */
/*  3: loc_type             tinyint(4)    */
/*  4: class                varchar(255)  */
/*  5: orientation          tinyint(4)    */
/*  6: molType              varchar(255)  */
/*  7: fxn_class            varchar(255)  */
/*  8: validation_status    tinyint(4)    */
/*  9: avHet                float         */
/*  10: avHetSE              float         */
/*  11: allele               blob          */
/*  12: refUCSC              blob          */
/*  13: refUCSCReverseComp   blob          */
/*  14: observed             blob          */

{
char tableName[64];
char fileName[64];
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int start = 0;
int end = 0;
int score = 0;
int bin = 0;
char *s;
FILE *f;

// this is a kludge
char *observedString = NULL;
char *molTypeString = NULL;
char *classString = NULL;

char *alleleString = NULL;
char *refUCSCString = NULL;

char *functionString = NULL;

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
if (!hTableExists(tableName)) return;

safef(fileName, ArraySize(fileName), "%s_snp125.tab", chromName);
f = mustOpen(fileName, "w");

safef(query, sizeof(query), "select * from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[2]);
    bin = hFindBin(start, end);
    /* 16 things to print.  Start with 4x4. */
    fprintf(f, "%d\t%s\t%d\t%d\trs%s\t", bin, chromName, start, end, row[0]);
    alleleString = cloneString(row[11]);
    refUCSCString = cloneString(row[12]);
    stripChar(alleleString, ' ');
    stripChar(refUCSCString, ' ');
    fprintf(f, "%d\t%s\t%s\t%s\t", 
            score, strandStrings[sqlUnsigned(row[5])], alleleString, refUCSCString);

    s = validString(sqlUnsigned(row[8]));
    observedString = cloneString(row[14]);
    molTypeString = cloneString(row[6]);
    classString = cloneString(row[4]);
    stripChar(observedString, ' ');
    stripChar(molTypeString, ' ');
    stripChar(classString, ' ');
    fprintf(f, "%s\t%s\t%s\t%s\t", observedString, molTypeString, classString, s);

    functionString = cloneString(row[7]);
    stripChar(functionString, ' ');
    fprintf(f, "%s\t%s\t%s\t%s\n", row[9], row[10], functionString, row[3]);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&f);
}



void loadDatabase(char *chromName)
{
struct sqlConnection *conn = hAllocConn();
char fileName[64];
char tableName[64];
FILE *f;

safef(tableName, ArraySize(tableName), "%s_snp125", chromName);
safef(fileName, ArraySize(fileName), "%s_snp125.tab", chromName);

snp125TableCreate(conn, tableName);
f = mustOpen(fileName, "r");
hgLoadTabFile(conn, ".", tableName, &f);
hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* Query all chrN_snpTmp tables; write to snp125.tab. */
{
struct hashCookie cookie;
struct hashEl *hel;
char *chromName;
char tableName[64];

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);

chromHash = loadChroms();

// outputFileHandle = mustOpen("snp125.tab", "w");

// cookie = hashFirst(chromHash);
// while ((chromName = hashNextName(&cookie)) != NULL)
    // {
    // safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
    // if (!hTableExists(tableName)) continue;
	        // 
    // verbose(1, "chrom = %s\n", chromName);
    // readSnps(chromName);
    // }

// carefulClose(&outputFileHandle);

readSnps("chr10");
loadDatabase("chr10");

// cookie = hashFirst(chromHash);
// while ((chromName = hashNextName(&cookie)) != NULL)
    // {
    // safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
    // if (!hTableExists(tableName)) continue;
	        // 
    // loadDatabase(chromName);
    // }


return 0;
}
