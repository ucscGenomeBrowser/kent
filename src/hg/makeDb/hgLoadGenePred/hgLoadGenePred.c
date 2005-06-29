/* hgLoadGenePred - Load genePred tables. */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "genePred.h"
#include "genePredReader.h"
#include "hdb.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: hgLoadGenePred.c,v 1.2 2005/06/29 00:31:32 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"bin", OPTION_BOOLEAN},
    {"genePredExt", OPTION_BOOLEAN},
    {"skipInvalid", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean gBin = FALSE;
boolean gGenePredExt = FALSE;
boolean gSkipInvalid = FALSE;

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n"
         "hgLoadGenePred - Load up a mySQL database genePred table\n"
         "usage:\n"
         "   hgLoadGenePred database table genePredFile\n"
         "\n"
         "This will sort the input file by chromsome and validated\n"
         "the genePreds/\n"
         "\n"
         "Options:\n"
         "   -bin - add binning\n"
         "   -genePredExt - use extended genePred format\n"
         "   -skipInvalid - instead of aborting on genePreds that\n"
         "    don't pass genePredCheck, generate a warning and skip\n"
         "    them.  You really should fix the data instead of using\n"
         "    this option\n", msg);
}

void setupTable(struct sqlConnection *conn, char *table)
/* create a psl table as needed */
{
unsigned sqlOpts = gBin ? genePredWithBin : 0;
unsigned fldOpts =  gGenePredExt ? genePredAllFlds : 0;
char* sqlCmd = genePredGetCreateSql(table, fldOpts, sqlOpts, hGetMinIndexLength());
sqlRemakeTable(conn, table, sqlCmd);
freez(&sqlCmd);
}

boolean checkGene(struct genePred *gene)
/* validate that a genePred is ok, either exit or return false if it's not */
{
int chromSize = hChromSize(gene->chrom);
if (genePredCheck("invalid genePred", stderr, chromSize, gene) == 0)
    return TRUE;
else
    {
    if (gSkipInvalid)
        warn("Warning: skipping %s", gene->name);
    else
        errAbort("Error: invalid genePreds, database unchanged");
    return FALSE;
    }
}

void copyGene(struct genePred *gene, FILE *tabFh)
/* copy one gene to the tab file */
{
unsigned holdOptFields = gene->optFields;
unsigned optFields = (genePredIdFld|genePredName2Fld|genePredCdsStatFld|genePredExonFramesFld);

if (gGenePredExt && ((optFields & optFields) != optFields))
    errAbort("genePred %s doesn't have fields required for -genePredExt", gene->name);

if (checkGene(gene))
    {
    if (!gGenePredExt)
        gene->optFields = 0;  /* omit optional fields */

    if (gBin)
        fprintf(tabFh, "%u\t", hFindBin(gene->txStart, gene->txEnd));
    genePredTabOut(gene, tabFh);

    gene->optFields = holdOptFields; /* restore optional fields */
    }
}

void mkTabFile(char *genePredFile, FILE *tabFh)
/* create a tab file to load from a genePred, optionally
 * adding binning or stripping extended fields if not requested */
{
struct genePred *genes = genePredReaderLoadFile(genePredFile, NULL);
struct genePred *gene;

slSort(&genes, genePredCmp);
for (gene = genes; gene != NULL; gene = gene->next)
    copyGene(gene, tabFh);
genePredFreeList(&genes);
}

void hgLoadGenePred(char *db, char *table, char *genePredFile)
/* hgLoadGenePred - Load up a mySQL database genePred table. */
{
struct sqlConnection *conn = sqlConnect(db);
char *tmpDir = ".";
FILE *tabFh = hgCreateTabFile(tmpDir, table);

hSetDb(db);
mkTabFile(genePredFile, tabFh);
setupTable(conn, table);
hgLoadTabFile(conn, tmpDir, table, &tabFh);
sqlDisconnect(&conn);
hgRemoveTabFile(tmpDir, table);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("wrong # args");
gBin = optionExists("bin");
gGenePredExt = optionExists("genePredExt");
gSkipInvalid = optionExists("skipInvalid");
hgLoadGenePred(argv[1], argv[2], argv[3]);
return 0;
}
