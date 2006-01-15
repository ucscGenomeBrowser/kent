/* snpLoadFasta - Read fasta files with flank sequences.   Load into database.  */

#include "common.h"
#include "hdb.h"
#include "linefile.h"
#include "snpFasta.h"

static char const rcsid[] = "$Id: snpLoadFasta.c,v 1.4 2006/01/15 05:42:29 heather Exp $";

/* from snpFixed.SnpClassCode */
char *classStrings[] = {
    "unknown",
    "single",
    "in-del",
    "het",
    "microsatelite",
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
  "  snpFasta database file\n");
}

void readFasta(char *filename)
/* Parse each line. */
{
struct lineFile *lf = lineFileOpen(filename, TRUE);
char *line;
int lineSize;
struct snpFasta *el;
char *chopAtRs;
char *chopAtMolType;
char *chopAtAllele;
int wordCount9, wordCount2;
char *row[9], *rsId[2], *molType[2], *class[2], *allele[2];
int count = 0;

while (lineFileNext(lf, &line, &lineSize))
    {
    AllocVar(el);
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

void writeSnpFastaTable(FILE *f)
{
/* write tab separated file */
struct snpFasta *el;

for (el = list; el != NULL; el = el->next)
    {
    fprintf(f, "%s\t", el->rsId);
    fprintf(f, "%s\t", el->molType);
    fprintf(f, "%s\t", el->class);
    fprintf(f, "%s\t", el->observed);
    fprintf(f, "n/a\t");
    fprintf(f, "n/a\t");
    fprintf(f, "\n");
    }
}

void loadFasta()
/* write the tab file, create the table and load the tab file */
{
struct sqlConnection *conn = hAllocConn();

FILE *f = hgCreateTabFile(".", "snpFasta");
writeSnpFastaTable(f);

// index length is useless here
snpFastaTableCreate(conn, 32);

hgLoadTabFile(conn, ".", "snpFasta", &f);
hFreeConn(&conn);
}

int main(int argc, char *argv[])
{

if (argc != 3)
    usage();

readFasta(argv[2]);
hSetDb(argv[1]);
loadFasta();

return 0;
}
