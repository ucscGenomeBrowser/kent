/* mafGene - output protein alignments using maf and frames. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "maf.h"
#include "obscure.h"
#include "genePred.h"
#include "mafGene.h"
#include "genePredReader.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafGene - output protein alignments using maf and genePred\n"
  "usage:\n"
  "   mafGene dbName mafTable genePredTable species.lst output\n"
  "arguments:\n"
  "   dbName         name of SQL database\n"
  "   mafTable       name of maf file table\n"
  "   genePredTable  name of the genePred table\n"
  "   species.lst    list of species names\n"
  "   output         put output here\n"
  "options:\n"
  "   -geneName=foobar   name of gene as it appears in genePred\n"
  "   -geneList=foolst   name of file with list of genes\n"
  "   -geneBeds=foo.bed  name of bed file with genes and positions\n"
  "   -chrom=chr1        name of chromosome from which to grab genes\n"
  "   -exons             output exons\n"
  "   -noTrans           don't translate output into amino acids\n"
  "   -delay=N           delay N seconds between genes (default 0)\n" 
  "   -noDash            don't output lines with all dashes\n"
  );
}

static struct optionSpec options[] = {
   {"geneName", OPTION_STRING},
   {"geneList", OPTION_STRING},
   {"geneBeds", OPTION_STRING},
   {"chrom", OPTION_STRING},
   {"exons", OPTION_BOOLEAN},
   {"noTrans", OPTION_BOOLEAN},
   {"noDash", OPTION_BOOLEAN},
   {"delay", OPTION_INT},
   {NULL, 0},
};

char *geneName = NULL;
char *geneList = NULL;
char *geneBeds = NULL;
char *onlyChrom = NULL;
boolean inExons = FALSE;
boolean noTrans = TRUE;
int delay = 0;
boolean newTableType;
boolean noDash = FALSE;

/* load one or more genePreds from the database */
struct genePred *getPredsForName(char *name, char *geneTable, char *db)
{
struct sqlConnection *conn = hAllocConn(db);
struct genePred *list = NULL;
char splitTable[HDB_MAX_TABLE_STRING];
struct genePred *gene;
boolean hasBin;
struct genePredReader *reader;

boolean found =  hFindSplitTable(db, NULL, geneTable,
	splitTable, &hasBin);

if (!found)
    errAbort("can't find table %s\n", geneTable);

char extra[2048];
if (onlyChrom != NULL)
    safef(extra, sizeof extra, "name='%s' and chrom='%s'", name, onlyChrom);
else
    safef(extra, sizeof extra, "name='%s'", name);

reader = genePredReaderQuery( conn, splitTable, extra);

while ((gene  = genePredReaderNext(reader)) != NULL)
    {
    verbose(2, "got gene %s\n",gene->name);
    slAddHead(&list, gene);
    }

if (list == NULL)
    errAbort("no genePred for gene %s in %s\n",name, geneTable);

slReverse(&list);

genePredReaderFree(&reader);
hFreeConn(&conn);

return list;
}

/* output the sequence for one gene for every species to 
 * the file stream 
 */
void outGenePred(FILE *f, struct genePred *pred, char *dbName, 
    char *mafTable, char *geneTable, struct slName *speciesNameList)
{
unsigned options = 0;

if (inExons)
    options |= MAFGENE_EXONS;
if (noTrans)
    options |= MAFGENE_NOTRANS;
if (!noDash)
    options |= MAFGENE_OUTBLANK;

mafGeneOutPred(f, pred, dbName, mafTable, speciesNameList, options);
}

/* read a list of single words from a file */
static struct slName *readList(char *fileName)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct slName *list = NULL;
char *row[1];

while (lineFileRow(lf, row))
    {
    int len = strlen(row[0]);
    struct slName *sn = needMem(sizeof(*sn)+len);
    strcpy(sn->name, row[0]);
    slAddHead(&list, sn);
    }

slReverse(&list);

lineFileClose(&lf);
return list;
}

/* query the list of gene names from the frames table */
static struct genePred *queryPreds(char *dbName, char *geneTable)
{
struct sqlConnection *conn = hAllocConn(dbName);
struct genePred *list = NULL;
char buf[2048];
char *extra = NULL;
struct genePredReader *reader;

if (onlyChrom != NULL)
    {
    safef(buf, sizeof buf, "chrom='%s'", onlyChrom);
    extra = buf;
    }

reader = genePredReaderQuery( conn, geneTable, extra);

list = genePredReaderAll(reader);

hFreeConn(&conn);

return list;
}

struct genePred *getPredsFromBeds(char *file, char *table, char *db)
{
struct sqlConnection *conn = hAllocConn(db);
struct lineFile *lf = lineFileOpen(file, TRUE);
char *words[5000];
int wordsRead;
struct genePred *list = NULL;

while( (wordsRead = lineFileChopNext(lf, words, sizeof(words)/sizeof(char *)) ))
    {
    if (wordsRead != 4)
	errAbort("file '%s' must be bed4. Line %d has %d fields", 
	    file, lf->lineIx, wordsRead);

    char where[10 * 1024];
    safef(where, sizeof where, 
	"name = '%s' and chrom='%s' and txStart = %d and txEnd = %d",
	words[3], words[0], sqlUnsigned(words[1]), sqlUnsigned(words[2]));

    //printf("table %s where %s\n",table,where);
    struct genePredReader *reader = genePredReaderQuery( conn, table, where);
    struct genePred *pred;
    while ((pred = genePredReaderNext(reader)) != NULL)
	slAddHead(&list, pred);

    genePredReaderFree(&reader);
    }

hFreeConn(&conn);

if (list != NULL)
    slReverse(&list);

return list;
}

void mafGene(char *dbName, char *mafTable, char *geneTable, 
   char *speciesList, char *outName)
/* mafGene - output protein alignments using maf and genePred. */
{
struct slName *speciesNames = readList(speciesList);
FILE *f = mustOpen(outName, "w");
struct genePred *list = NULL;

if (geneList != NULL)
    {
    struct slName *geneNames = readList(geneList);
    for(; geneNames; geneNames = geneNames->next)
	{
	struct genePred *pred = 
	    getPredsForName(geneNames->name, geneTable, dbName);
	if (pred != NULL)
	    slCat(&list, pred);
	}
    }
else if (geneName != NULL)
    list = getPredsForName(geneName, geneTable, dbName);
else if (geneBeds != NULL)
    list = getPredsFromBeds(geneBeds, geneTable, dbName);
else
    list = queryPreds(dbName, geneTable);

for(; list; list = list->next)
    {
    verbose(2, "outting  gene %s \n",list->name);
    outGenePred(f, list, dbName, mafTable, geneTable,  speciesNames);
    if (delay)
	{
	verbose(2, "delaying %d seconds\n",delay);
	sleep(delay);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc != 6)
    usage();

geneName = optionVal("geneName", geneName);
geneList = optionVal("geneList", geneList);
geneBeds = optionVal("geneBeds", geneBeds);
onlyChrom = optionVal("chrom", onlyChrom);
inExons = optionExists("exons");
noDash = optionExists("noDash");
noTrans = optionExists("noTrans");
delay = optionInt("delay", delay);

if (((geneName != NULL) && ((geneList != NULL) || geneBeds != NULL)) ||
    ((geneList != NULL) && ((geneName != NULL) || geneBeds != NULL)) ||
    ((geneBeds != NULL) && ((geneList != NULL) || geneName != NULL)))
    errAbort("must specify only one of geneList, geneName, or geneBeds");

if ((geneBeds != NULL) && (onlyChrom != NULL))
    errAbort("cannot specify beds and chrom");

mafGene(argv[1],argv[2],argv[3],argv[4],argv[5]);
return 0;
}
