/* snpFinalTable - create chrN_snp tables from chrN_snpTmp tables. */
/* Also create exceptions table from exceptions files. */
/* Get chroms from UCSC chromInfo. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* Reorder columns. */
/* Add bin. */
/* Add 'rs' in front of snp_id. */
/* Orientation --> strandStrings. */
/* Add score for BED format (always 0). */
/* Expand validation_status. */
/* Drop refUCSCReverseComp. */

/* Not currently using outputFileHandle. */

#include "common.h"

#include "dystring.h"
#include "hdb.h"
#include "jksql.h"
#include "snp125.h"
#include "snp125Exceptions.h"


static char *snpDb = NULL;
static int version = 0;
FILE *outputFileHandle = NULL;

char *strandStrings[] = {
"+",
"-",
};

/* these are described in b125_mapping.doc */
/* Also snpFixed.LocTypeCode */
char *locTypeStrings[] = {
    "unknown",
    "range",
    "exact",
    "between",
    "rangeInsertion",
    "rangeSubstitution",
    "rangeDeletion",
};

char *exceptionTables[] = {
    "snpCheckAlleles",
    "snpCheckClassAndObserved",
    "snpExpandAllele",
    "snpLocType",
};


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpFinalTable - create chrN_snp tables from chrN_snpTmp tables.\n"
    "Also create exceptions table from exceptions files.\n"
    "usage:\n"
    "    snpFinalTable snpDb version\n");
}

char *validString(int validCode)
/* These are from snpFixed.SnpValidationCode */
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


void processSnps(char *chromName, int version)
/* read chrN_snpTmp */
/* write to chrN_snp.tab */

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
/*  10: avHetSE             float         */
/*  11: allele              blob          */
/*  12: refUCSC             blob          */
/*  13: refUCSCReverseComp  blob          */
/*  14: observed            blob          */
/*  15: weight              int           */

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

char *observedString = NULL;
char *molTypeString = NULL;
char *classString = NULL;

char *alleleString = NULL;
char *refUCSCString = NULL;

char *functionString = NULL;

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
if (!hTableExists(tableName)) return;

safef(fileName, ArraySize(fileName), "%s_snp%d.tab", chromName, version);
f = mustOpen(fileName, "w");

sqlSafef(query, sizeof(query), "select * from %s", tableName);

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
    fprintf(f, "%d\t%s\t%s\t%s\t", 
            score, strandStrings[sqlUnsigned(row[5])], alleleString, refUCSCString);

    s = validString(sqlUnsigned(row[8]));
    observedString = cloneString(row[14]);
    molTypeString = cloneString(row[6]);
    classString = cloneString(row[4]);
    fprintf(f, "%s\t%s\t%s\t%s\t", observedString, molTypeString, classString, s);

    functionString = cloneString(row[7]);
    fprintf(f, "%s\t%s\t%s\t%s\t", row[9], row[10], functionString, locTypeStrings[sqlUnsigned(row[3])]);
    fprintf(f, "%s\n", row[15]);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&f);
}



void loadDatabase(char *chromName, int version)
{
struct sqlConnection *conn = hAllocConn();
char fileName[64];
char tableName[64];
FILE *f;

safef(tableName, ArraySize(tableName), "%s_snp%d", chromName, version);
safef(fileName, ArraySize(fileName), "%s_snp%d.tab", chromName, version);

snp125TableCreate(conn, tableName);
f = mustOpen(fileName, "r");
hgLoadTabFile(conn, ".", tableName, &f);
hFreeConn(&conn);
}

void doExceptions(int version)
/* write snpExceptions table */
{
struct sqlConnection *conn = hAllocConn();
int i = 0;
FILE *exceptionFileHandle = NULL;
char tableName[64];
char fileName[64];

safef(fileName, ArraySize(fileName), "snp%dExceptions", version);
snp125ExceptionsTableCreate(conn);

for (i = 0; i < ArraySize(exceptionTables); i++)
    {
    safef(tableName, ArraySize(tableName), "%s.tab", exceptionTables[i]);
    exceptionFileHandle = mustOpen(tableName, "r");
    hgLoadNamedTabFile(conn, ".", fileName, exceptionTables[i], &exceptionFileHandle);
    }
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Query all chrN_snpTmp tables; write to snp.tab. */
{

struct slName *chromList, *chromPtr;
char tableName[64];
char fileName[64];

if (argc != 3)
    usage();

snpDb = argv[1];
hSetDb(snpDb);
chromList = hAllChromNamesDb(snpDb);

version = atoi(argv[2]);
safef(fileName, ArraySize(fileName), "snp%d.tab", version);
outputFileHandle = mustOpen(fileName, "w");

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromPtr->name);
    if (!hTableExists(tableName)) continue;
    verbose(1, "chrom = %s\n", chromPtr->name);
    processSnps(chromPtr->name, version);
    }

carefulClose(&outputFileHandle);

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromPtr->name);
    if (!hTableExists(tableName)) continue;
    loadDatabase(chromPtr->name, version);
    }

doExceptions(version);

return 0;
}
