/* hgLoadGenePred - Load genePred tables. */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "genePred.h"
#include "genePredReader.h"
#include "hdb.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: hgLoadGenePred.c,v 1.3 2006/05/11 00:55:22 markd Exp $";

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
         "   hgLoadGenePred database table genePredFile [...]\n"
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

struct genePred *loadGenes(int numGenePreds, char **genePredFiles)
/* load and sort genes */
{
int i;
struct genePred *genes = NULL;
for (i = 0; i < numGenePreds; i++)
    genes = slCat(genes, genePredReaderLoadFile(genePredFiles[i], NULL));
slSort(&genes, genePredCmp);
return genes;
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

void mkTabFile(struct genePred *genes, FILE *tabFh)
/* create a tab file to load, optionally adding binning or stripping extended
 * fields if not requested */
{
struct genePred *gene;

for (gene = genes; gene != NULL; gene = gene->next)
    copyGene(gene, tabFh);
}

void hgLoadGenePred(char *db, char *table, int numGenePreds, char **genePredFiles)
/* hgLoadGenePred - Load up a mySQL database genePred table. */
{
struct genePred *genes = loadGenes(numGenePreds, genePredFiles);
struct sqlConnection *conn = sqlConnect(db);
char *tmpDir = ".";
FILE *tabFh = hgCreateTabFile(tmpDir, table);

hSetDb(db);
mkTabFile(genes, tabFh);
genePredFreeList(&genes);
setupTable(conn, table);
hgLoadTabFile(conn, tmpDir, table, &tabFh);
sqlDisconnect(&conn);
hgRemoveTabFile(tmpDir, table);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 4)
    usage("wrong # args");
gBin = optionExists("bin");
gGenePredExt = optionExists("genePredExt");
gSkipInvalid = optionExists("skipInvalid");
hgLoadGenePred(argv[1], argv[2], argc-3, argv+3);
return 0;
}
