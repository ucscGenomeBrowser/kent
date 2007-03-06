/* spDbAddVarSplice - This adds information on the varient splices to the sp/uniProt database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"

static char const rcsid[] = "$Id: spDbAddVarSplice.c,v 1.2 2007/03/06 00:37:03 fanhsu Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spDbAddVarSplice - This adds information on the varient splices to the sp/uniProt database\n"
  "usage:\n"
  "   spDbAddVarSplice varsplice.fasta outDir\n"
  "After this program is done, add the varProtein.txt file into both protein\n"
  "and varProtein tables, add varDisplayId.txt to displayId table, and varAcc.txt\n"
  "into varAcc table\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

FILE *openToWrite(char *dir, char *file)
/* Return dir/file open for writing. */
{
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s", dir, file);
return mustOpen(path, "w");
}

void spDbAddVarSplice(char *inFile, char *outDir)
/* spDbAddVarSplice - This adds information on the varient splices to the sp/uniProt database. */
{
makeDir(outDir);
FILE *varProtein = openToWrite(outDir, "varProtein.txt");
FILE *varAcc = openToWrite(outDir, "varAcc.txt");
FILE *varDisplayId = openToWrite(outDir, "varDisplayId.txt");
struct lineFile *lf = lineFileOpen(inFile, TRUE);
aaSeq seq;
ZeroVar(&seq);
while (faPepSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    char *row[4];
    int rowSize = chopString(seq.name, "-|", row, ArraySize(row));
    if (rowSize != 3)
        errAbort("Expecting name to be in format accession-N|DISP_ID, got %s\n", seq.name);
    chopString(seq.name, "-|", row, ArraySize(row));
    char *acc = row[0];
    int accLen = strlen(acc);
    char *version = row[1];
    char *displayId = row[2];
    int displayIdLen = strlen(displayId);

    /* Do some tests. */
    if (accLen < 6 || accLen > 8 || isdigit(acc[0]) || !isdigit(acc[accLen-1]))
        errAbort("wierd accession %s before line %d of %s", acc, lf->lineIx, lf->fileName);
    if (!isdigit(version[0]) || strlen(version) > 4)
        errAbort("wierd version %s before line %d of %s", version, lf->lineIx, lf->fileName);
    if (countChars(displayId, '_') != 1 || displayIdLen < 6 || displayIdLen > 16)
        errAbort("wierd displayId %s before line %d of %s", displayId, lf->lineIx, lf->fileName);

    /* Print out parsed results. */
    fprintf(varAcc, "%s-%s\t%s\t%s\n", acc, version, acc, version);
    fprintf(varProtein, "%s-%s\t%s\n", acc, version, seq.dna);
    fprintf(varDisplayId, "%s-%s\t%s-%s\n", acc, version, acc, version);
    }
carefulClose(&varAcc);
carefulClose(&varProtein);
carefulClose(&varDisplayId);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
spDbAddVarSplice(argv[1], argv[2]);
return 0;
}
