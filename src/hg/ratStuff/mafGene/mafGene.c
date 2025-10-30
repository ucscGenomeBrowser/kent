/* mafGene - output protein alignments using maf and frames. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
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
#include "chromAlias.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafGene - output protein alignments using maf and genePred\n"
  "usage:\n"
  "   mafGene dbName mafTable genePredTable species.lst output\n"
  "arguments:\n"
  "   dbName         name of SQL database\n"
  "   mafTable       name of maf file table or bigMaf if ends in .bigMaf or .bb\n"
  "   genePredTable  name of the genePred table or file if useFile is on or ends in .gp\n"
  "   species.lst    list of species names\n"
  "   output         put output here\n"
  "options:\n"
  "   -twoBit=file.2bit  use 2bit file to fill in spaces in the alignment instead of database\n"
  "   -geneName=foobar   name of gene as it appears in genePred\n"
  "   -geneList=foolst   name of file with list of genes\n"
  "   -geneBeds=foo.bed  name of bed file with genes and positions\n"
  "   -chrom=chr1        name of chromosome from which to grab genes\n"
  "   -useFile           genePredTable is a file\n"
  "   -exons             output exons\n"
  "   -noTrans           don't translate output into amino acids\n"
  "   -uniqAA            put out unique pseudo-AA for every different codon\n"
  "   -includeUtr        include the UTRs, use only with -noTrans\n"
  "   -delay=N           delay N seconds between genes (default 0)\n" 
  "   -noDash            don't output lines with all dashes\n"
  );
}

static struct optionSpec options[] = {
   {"geneName", OPTION_STRING},
   {"geneList", OPTION_STRING},
   {"geneBeds", OPTION_STRING},
   {"twoBit", OPTION_STRING},
   {"chrom", OPTION_STRING},
   {"exons", OPTION_BOOLEAN},
   {"noTrans", OPTION_BOOLEAN},
   {"noDash", OPTION_BOOLEAN},
   {"uniqAA", OPTION_BOOLEAN},
   {"useFile", OPTION_BOOLEAN},
   {"includeUtr", OPTION_BOOLEAN},
   {"delay", OPTION_INT},
   {NULL, 0},
};

char *geneName = NULL;
char *geneList = NULL;
char *geneBeds = NULL;
char *onlyChrom = NULL;
char *twoBitFile = NULL;
boolean inExons = FALSE;
boolean noTrans = TRUE;
int delay = 0;
boolean newTableType;
boolean uniqAA = FALSE;
boolean noDash = FALSE;
boolean useFile = FALSE;
boolean includeUtr = FALSE;

/* load one or more genePreds from the database */
struct genePred *getPredsForName(char *name, char *geneTable, char *db)
{
struct sqlConnection *conn = hAllocConn(db);
struct genePred *list = NULL;
char splitTable[HDB_MAX_TABLE_STRING];
struct genePred *gene;
struct genePredReader *reader;

boolean found = hFindSplitTable(db, NULL, geneTable, splitTable, sizeof splitTable, NULL);
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
    char *mafTable, char *geneTable, struct slName *speciesNameList, struct mafFileCache *mafFileCache)
{
unsigned options = 0;

if (uniqAA)
    options |= MAFGENE_UNIQUEAA;
if (inExons)
    options |= MAFGENE_EXONS;
if (noTrans)
    options |= MAFGENE_NOTRANS;
if (!noDash)
    options |= MAFGENE_OUTBLANK;
if (includeUtr)
    options |= MAFGENE_INCLUDEUTR;

mafGeneOutPredExt(f, pred, dbName, mafTable, speciesNameList, options, 0, mafFileCache);
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
    sqlSafef(where, sizeof where, 
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
else if (useFile)
    /* Read genePreds from a file passed instead of a table */
    list = genePredReaderLoadFile(geneTable, NULL);
else if (geneName != NULL)
    list = getPredsForName(geneName, geneTable, dbName);
else if (geneBeds != NULL)
    list = getPredsFromBeds(geneBeds, geneTable, dbName);
else
    list = queryPreds(dbName, geneTable);


boolean isBigMaf =endsWith(mafTable, ".bigMaf") || endsWith(mafTable, ".bb"); 
struct mafFileCache *mafFileCache = NULL;
if (isBigMaf || twoBitFile)
    {
    AllocVar(mafFileCache);
    if (isBigMaf)
        mafFileCache->bbi = bigBedFileOpenAlias(mafTable, chromAliasFindAliases);
    if (twoBitFile != NULL)
        mafFileCache->tbf = twoBitOpen(twoBitFile);
    }

for(; list; list = list->next)
    {
    verbose(2, "outting  gene %s \n",list->name);
    outGenePred(f, list, dbName, mafTable, geneTable,  speciesNames, mafFileCache);
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

boolean isGp = endsWith(argv[3], ".gp");
geneName = optionVal("geneName", geneName);
geneList = optionVal("geneList", geneList);
geneBeds = optionVal("geneBeds", geneBeds);
twoBitFile = optionVal("twoBit", twoBitFile);
onlyChrom = optionVal("chrom", onlyChrom);
inExons = optionExists("exons");
uniqAA = optionExists("uniqAA");
noDash = optionExists("noDash");
noTrans = optionExists("noTrans");
includeUtr = optionExists("includeUtr");
useFile = optionExists("useFile") || isGp;
delay = optionInt("delay", delay);

if (((geneName != NULL) && ((geneList != NULL) || geneBeds != NULL)) ||
    ((geneList != NULL) && ((geneName != NULL) || geneBeds != NULL)) ||
    ((geneBeds != NULL) && ((geneList != NULL) || geneName != NULL)))
    errAbort("must specify only one of geneList, geneName, geneBeds");

if ((geneBeds != NULL) && (onlyChrom != NULL))
    errAbort("cannot specify beds and chrom");

if (includeUtr && !noTrans)
    errAbort("must specify noTrans if includeUtr is selected");

mafGene(argv[1],argv[2],argv[3],argv[4],argv[5]);
return 0;
}
