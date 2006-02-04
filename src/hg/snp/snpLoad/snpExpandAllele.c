/* snpExpandAllele - fourth step in dbSNP processing.
 * Read the chrN_snpTmp tables created by snpLocType and expand allele.
 * Rewrite to new chrN_snpTmp tables.  Get chromInfo from ContigInfo. 

 * Do I need to write a whole new tab file?  Or could I just do updates?
 * See what the counts are.

 * May need to adjust chromEnd for locType 4,5 and 6!! */

#include "common.h"

#include "dystring.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpExpandAllele.c,v 1.3 2006/02/04 05:29:20 heather Exp $";

char *snpDb = NULL;
char *contigGroup = NULL;
static struct hash *chromHash = NULL;


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpExpandAllele - expand allele in chrN_snpTmp\n"
    "usage:\n"
    "    snpExpandAllele snpDb contigGroup\n");
}


struct hash *loadChroms(char *contigGroup)
/* hash all chromNames that match contigGroup */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *chromName;

ret = newHash(0);
safef(query, sizeof(query), "select distinct(contig_chr) from ContigInfo where group_term = '%s'", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    chromName = cloneString(row[0]);
    hashAdd(ret, chromName, NULL);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

boolean needToSplit(char *allele)
/* return true if allele contains open paren */
{
char *openParen = NULL;
openParen = strchr(allele, '(');
if (openParen == NULL) return FALSE;
return TRUE;
}

char *getLeftFlank(char *allele)
/* We know allele has an open paren (could assert that). */
/* The flank will contain all ACGT. */
/* We may or may not have any flank. */
{
int splitPos = 0;
char *leftFlank = NULL;

splitPos = strcspn(allele, "(");
leftFlank = cloneString(allele);
leftFlank[splitPos] = '\0';
return leftFlank;
}

int getRepeatCount(char *allele)
/* We believe allele starts with one or two digit number. */
{
char *localAllele = cloneString(allele);
int size = strspn(localAllele, "0123456789");
if (size != 1 && size != 2) return 0;
localAllele[size] = '\0';
return (atoi(localAllele));
}

char *generateRepeatString(char nucleotide, int count)
/* create a string with count nucleotides */
{
int pos;
char *repeatString = needMem(count+1);

for (pos = 0; pos < count; pos++)
   repeatString[pos] = nucleotide;
repeatString[count] = '\0';

return repeatString;
}

char *expandAllele(char *startAllele)
{
struct dyString *newAllele = newDyString(1024);
/* copy startAllele into oldAllele, which we consume as we go */
char *oldAllele = cloneString(startAllele);
char *leftFlank = NULL;
char nucleotide;
int repeatCount = 0;
char *repeatString = NULL;

while (oldAllele != NULL)
    {
    if (!needToSplit(oldAllele))
        {
	dyStringAppend(newAllele, oldAllele);
	if (!sameString(oldAllele, startAllele))
	    {
	    verbose(5, "expandAllele started with %s\n", startAllele);
	    verbose(5, "expandAllele returning %s\n", newAllele->string);
	    verbose(5, "---------------------\n");
	    }
	return (newAllele->string);
	}
    leftFlank = getLeftFlank(oldAllele);
    verbose(5, "leftFlank = %s\n", leftFlank);
    // perhaps dyStringAppend can handle NULL input?
    if (leftFlank != NULL)
        {
	dyStringAppend(newAllele, leftFlank);
	}

    oldAllele = strpbrk(oldAllele, "(");
    verbose(5, "oldAllele = %s\n", oldAllele);
    if (strlen(oldAllele) < 3)
        {
	verbose(1, "syntax error in %s\n", startAllele);
	verbose(1, "-----------------------------\n");
	return newAllele->string;
	}

    nucleotide = oldAllele[1];
    oldAllele = oldAllele + 3;
    if (strlen(oldAllele) < 1)
        {
	verbose(1, "syntax error in %s\n", startAllele);
	verbose(1, "-----------------------------\n");
	return newAllele->string;
	}
        
    repeatCount = getRepeatCount(oldAllele);
    if (repeatCount == 0)
        {
	verbose(1, "syntax error in %s\n", startAllele);
	verbose(1, "-----------------------------\n");
	return newAllele->string;
	}
    verbose(5, "repeatCount = %d\n", repeatCount);
    if (repeatCount <= 9)
        oldAllele = oldAllele + 1;
    else
        oldAllele = oldAllele + 2;
    repeatString = generateRepeatString(nucleotide, repeatCount);
    verbose(5, "repeatString = %s\n", repeatString);
    dyStringAppend(newAllele, repeatString);
    }
return newAllele->string;
}

void doExpandAllele(char *chromName)
/* call expandAllele for all rows */
/* don't do for loc_type 2 (exact) and loc_type 3 (between) */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
char fileName[64];
FILE *f;
char *allele = NULL;
// for short-circuit
int count = 0;

strcpy(tableName, "chr");
strcat(tableName, chromName);
strcat(tableName, "_snpTmp");
strcpy(fileName, tableName);
strcat(fileName, ".tab");

f = mustOpen(fileName, "w");

// start with loc_type = 1
// need to read all columns if I'm dumping to .tab file
safef(query, sizeof(query), "select snp_id, allele from %s where loc_type = 1", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    // verbose(1, "SNP = %s\n", row[0]);
    allele = expandAllele(row[1]);
    fprintf(f, "%s\t", row[0]);
    fprintf(f, "%s\n", allele);
    count++;
    // if (count == 1000) 
        // {
        // sqlFreeResult(&sr);
        // hFreeConn(&conn);
        // fclose(f);
        // return;
	// }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
fclose(f);
}


void recreateDatabaseTable(char *chromName)
/* create a new chrN_snpTmp table with new definition */
/* could use enum for loc_type here */
{
struct sqlConnection *conn = hAllocConn();
char tableName[64];
char *createString =
"CREATE TABLE %s (\n"
"    snp_id int(11) not null,\n"
"    chromStart int(11) not null,\n"
"    chromEnd int(11) not null,\n"
"    loc_type tinyint(4) not null,\n"
"    orientation tinyint(4) not null,\n"
"    allele blob\n"
");\n";

struct dyString *dy = newDyString(1024);

strcpy(tableName, "chr");
strcat(tableName, chromName);
strcat(tableName, "_snpTmp");

dyStringPrintf(dy, createString, tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
hFreeConn(&conn);
}


void loadDatabase(char *chromName)
/* load one table into database */
{
FILE *f;
struct sqlConnection *conn = hAllocConn();
char tableName[64], fileName[64];

strcpy(tableName, "chr");
strcat(tableName, chromName);
strcat(tableName, "_snpTmp");

strcpy(fileName, "chr");
strcat(fileName, chromName);
strcat(fileName, "_snpTmp.tab");

f = mustOpen(fileName, "r");
hgLoadTabFile(conn, ".", tableName, &f);

hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* read chrN_snpTmp, handle allele, rewrite to individual chrom tables */
{
struct hashCookie cookie;
struct hashEl *hel;
char *chromName;

if (argc != 3)
    usage();

snpDb = argv[1];
contigGroup = argv[2];
hSetDb(snpDb);

chromHash = loadChroms(contigGroup);
if (chromHash == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 0;
    }

// doExpandAllele("22");
// return 0;

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    verbose(1, "*****************************\n");
    verbose(1, "chrom = %s\n", chromName);
    doExpandAllele(chromName);
    // I'm not changing the table format, I could just do a 'delete from table'
    // recreateDatabaseTable(chromName);
    // loadDatabase(chromName);
    }

return 0;
}
