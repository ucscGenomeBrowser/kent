/* snpLoadFasta - Read fasta files with flank sequences.   Load into database.  */

#include "common.h"
#include "hdb.h"
#include "linefile.h"
#include "snpFasta.h"

static char const rcsid[] = "$Id: snpLoadFasta.c,v 1.2 2005/12/03 12:13:56 heather Exp $";

static char *database = NULL;

struct snpFastaElement
    {
    char *name;
    char *molType;
    char *observed;
    struct dyString *leftFlank;
    struct dyString *rightFlank;
    struct snpFastaElement *next;
     };

struct snpFastaElement *list;


void usage()
/* Explain usage and exit. */
{
errAbort(
  "snpLoadFasta - Read SNP fasta files and load into database.\n"
  "usage:\n"
  "  snpFasta database file\n");
}

void readFasta(char *filename)
/* Read in list of machines to use. */
{
struct lineFile *lf = lineFileOpen(filename, TRUE);
char *line;
int lineSize;
struct snpFastaElement *snpFasta;
char *chopAtRs;
char *chopAtMolType;
char *chopAtAllele;
int wordCount9, wordCount2;
char *row[9], *name[2], *molType[2], *allele[2];
int count = 0;

while (lineFileNext(lf, &line, &lineSize))
    {
    AllocVar(snpFasta);
    wordCount9 = chopString(line, "|", row, ArraySize(row));
    wordCount2 = chopString(row[2], " ", name, ArraySize(name));
    wordCount2 = chopString(row[6], "=", molType, ArraySize(molType));
    wordCount2 = chopString(row[8], "=", allele, ArraySize(allele));
    // printf("%s ", name[0]);
    // printf("%s ", molType[1]);
    // printf("%s\n", allele[1]);

    snpFasta->name = cloneString(name[0]);
    snpFasta->molType = cloneString(molType[1]);
    snpFasta->observed = cloneString(allele[1]);
    snpFasta->next = list;
    list = snpFasta;
    count++;
    }
/* could reverse order here */
verbose(1, "%d elements found\n", count);
}

void writeSnpFastaTable(FILE *f)
{
/* write tab separated file */
struct snpFastaElement *el;

for (el = list; el != NULL; el = el->next)
    {
    fprintf(f, "%s\t", el->name);
    fprintf(f, "%s\t", el->molType);
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
