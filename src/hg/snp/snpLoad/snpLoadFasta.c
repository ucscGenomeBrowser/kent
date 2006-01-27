/* snpLoadFasta - Read fasta files with flank sequences.   Load into database.  */

#include "common.h"
#include "hdb.h"
#include "linefile.h"
#include "snpFasta.h"

static char const rcsid[] = "$Id: snpLoadFasta.c,v 1.6 2006/01/27 20:28:27 heather Exp $";

/* from snpFixed.SnpClassCode */
char *classStrings[] = {
    "unknown",
    "single",
    "in-del",
    "het",
    "microsatellite",
    "named",
    "no var",
    "mixed",
    "mnp",
};

static char *database = NULL;
struct snpFasta *list;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "snpLoadFasta - Read SNP fasta files and load into database.\n"
  "usage:\n"
  "  snpFasta database \n");
}

void readFasta(char *chromName)
/* Parse each line. */
{
char fileName[64];
struct lineFile *lf;
char *line;
int lineSize;
struct snpFasta *el;
char *chopAtRs;
char *chopAtMolType;
char *chopAtAllele;
int wordCount9, wordCount2;
char *row[9], *rsId[2], *molType[2], *class[2], *allele[2];
int count = 0;

strcpy(fileName, "ch");
strcat(fileName, chromName);
strcat(fileName, ".gnl");

lf = lineFileOpen(fileName, TRUE);

while (lineFileNext(lf, &line, &lineSize))
    {
    AllocVar(el);
    el->chrom = cloneString(chromName);
    wordCount9 = chopString(line, "|", row, ArraySize(row));
    wordCount2 = chopString(row[2], " ", rsId, ArraySize(rsId));
    wordCount2 = chopString(row[6], "=", molType, ArraySize(molType));
    wordCount2 = chopString(row[7], "=", class, ArraySize(class));
    wordCount2 = chopString(row[8], "=", allele, ArraySize(allele));
    // verbose(5, "%s ", rsId[0]);
    // verbose(5, "%s ", molType[1]);
    // verbose(5, "%s ", class[1]);
    // verbose(5, "%s\n", allele[1]);

    el->rsId = cloneString(rsId[0]);
    el->molType = cloneString(molType[1]);
    stripChar(el->molType, '"');
    el->observed = cloneString(allele[1]);
    stripChar(el->observed, '"');
    el->class = classStrings[atoi(class[1])];
    el->next = list;
    list = el;
    count++;
    }
/* could reverse order here */
verbose(1, "%d elements found\n", count);
}

void createTable()
/* create the table one time, load each time */
{
struct sqlConnection *conn = hAllocConn();
snpFastaTableCreate(conn);
hFreeConn(&conn);
}

void writeSnpFastaTable(FILE *f)
{
/* write tab separated file */
struct snpFasta *el;

for (el = list; el != NULL; el = el->next)
    {
    fprintf(f, "%s\t", el->rsId);
    fprintf(f, "%s\t", el->chrom);
    fprintf(f, "%s\t", el->molType);
    fprintf(f, "%s\t", el->class);
    fprintf(f, "%s\t", el->observed);
    fprintf(f, "n/a\t");
    fprintf(f, "n/a\t");
    fprintf(f, "\n");
    }
}

void loadFasta(char *chromName)
/* write the tab file, create the table and load the tab file */
{
struct sqlConnection *conn = hAllocConn();
FILE *f;
char fileName[64];

strcpy(fileName, "chr");
strcat(fileName, chromName);
strcat(fileName, "_snpFasta");
f = hgCreateTabFile(".", fileName);
writeSnpFastaTable(f);
snpFastaTableCreate(conn);

hgLoadNamedTabFile(conn, ".", "snpFasta", fileName, &f);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
{
struct slName *chromList, *chromPtr;
char fileName[64];

if (argc != 2)
    usage();

database = argv[1];
hSetDb(database);
verbose(1, "calling createTable\n");
createTable();

verbose(1, "getting chroms\n");
chromList = hAllChromNamesDb(database);
verbose(1, "got chroms\n");

// for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    // {
    // stripString(chromPtr->name, "chr");
    // verbose(1, "chrom = %s\n", chromPtr->name);
    // readFasta(chromPtr->name);
    // loadFasta(chromPtr->name);
    // slFreeList(&list);
    // }

readFasta("1");
loadFasta("1");
slFreeList(&list);

return 0;
}
