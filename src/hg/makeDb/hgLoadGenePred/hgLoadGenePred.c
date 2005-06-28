/* hgLoadGenePred - Load genePred tables. */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "genePred.h"
#include "genePredReader.h"
#include "hdb.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: hgLoadGenePred.c,v 1.1 2005/06/28 06:23:24 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"bin", OPTION_BOOLEAN},
    {"genePredExt", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean gBin = FALSE;
boolean gGenePredExt = FALSE;

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n"
         "hgLoadGenePred - Load up a mySQL database genePred table\n"
         "usage:\n"
         "   hgLoadGenePred database table genePredFile\n"
         "\n"
         "This will sort the input file by chromsome\n"
         "\n"
         "Options:\n"
         "   -bin - add binning\n"
         "   -genePredExt - use extended genePred format\n", msg);
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

void copyGene(struct genePred *gene, FILE *tabFh)
/* copy one gene to the tab file */
{
unsigned holdOptFields = gene->optFields;
unsigned optFields = (genePredIdFld|genePredName2Fld|genePredCdsStatFld|genePredExonFramesFld);

if (gGenePredExt && ((optFields & optFields) != optFields))
    errAbort("genePred %s doesn't have fields required for -genePredExt", gene->name);

if (!gGenePredExt)
    gene->optFields = 0;  /* dropped optional fields */

if (gBin)
    fprintf(tabFh, "%u\t", hFindBin(gene->txStart, gene->txEnd));
genePredTabOut(gene, tabFh);

gene->optFields = holdOptFields; /* restore optional fields */
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
hgLoadGenePred(argv[1], argv[2], argv[3]);
return 0;
}
