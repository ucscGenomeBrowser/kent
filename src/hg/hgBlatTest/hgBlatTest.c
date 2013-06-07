/* hgBlatTest - Test hgBlat web page. - was cloned from hgblatTest */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "portable.h"
#include "options.h"
#include "errCatch.h"
#include "ra.h"
#include "fa.h"
#include "nib.h"
#include "htmlPage.h"
#include "../near/hgNear/hgNear.h"
#include "hdb.h"
#include "qa.h"

#include <time.h>


/* Command line variables. */
char *dataDir = "./";
char *clOrg = NULL;	/* Organism from command line. */
char *clDb = NULL;	/* DB from command line */
char *clType = NULL;	/* Type var from command line. */
char *clSort = NULL;	/* Sort var from command line. */
char *clOutput = NULL;	/* Output var from command line. */

int clRepeat = 3;	/* Number of repetitions. */

char *raName = "hgBlatTest.ra";
struct hash *raList = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgBlatTest - Test hgBlat web page\n"
  "usage:\n"
  "   hgBlatTest url log\n"
  "options:\n"
  "   -org=Human - Restrict to Human (or Mouse, Fruitfly, etc.)\n"
  "   -db=hg16 - Restrict to particular database\n"
  "   -type=DNA - DNA query\n"
  "   -sort=query,score - sorty output by query and score, default\n"
  "   -output=query,score - sorty output by query and score, default\n"
  "   -dataDir=dataDir - Use selected data dir, default %s\n"
  "   -repeat=N - Number of times to repeat test (on random genes), default %d\n"
  , dataDir, clRepeat);
}


static struct optionSpec options[] = {
   {"org",     OPTION_STRING},
   {"db",      OPTION_STRING},
   {"type",    OPTION_STRING},
   {"sort",    OPTION_STRING},
   {"output",  OPTION_STRING},
   {"dataDir", OPTION_STRING},
   {"repeat",  OPTION_INT},
   {NULL,      0},
};

struct blatTest
/* Test on one column. */
    {
    struct blatTest *next;
    struct qaStatus *status;	/* Result of test. */
    char *info[6];
    };

enum blatTestInfoIx {
   ntiiTest = 0,
   ntiiOrg = 1,
   ntiiDb = 2,
   ntiiType = 3,
   ntiiSort = 4,
   ntiiOutput = 5,
};

char *blatTestInfoTypes[] =
   {
   "test",
   "organism", 
   "db", 
   "type", 
   "sort", 
   "output" 
   };

struct blatTest *blatTestList = NULL;	/* List of all tests, latest on top. */

struct blatTest *blatTestNew(struct qaStatus *status, char *test,
	char *org, char *db, 
	char *type, char *sort, 
	char *output )
/* Save away column test results. */
{
struct blatTest *bt;
AllocVar(bt);
bt->status = status;
bt->info[ntiiTest] = cloneString(naForNull(test));
bt->info[ntiiOrg] = cloneString(naForNull(org));
bt->info[ntiiDb] = cloneString(naForNull(db));
bt->info[ntiiType] = cloneString(naForNull(type));
bt->info[ntiiSort] = cloneString(naForNull(sort));
bt->info[ntiiOutput] = cloneString(naForNull(output));
slAddHead(&blatTestList, bt);
return bt;
}

void blatTestLogOne(struct blatTest *test, FILE *f)
/* Log test result to file. */
{
int i;
for (i=0; i<ArraySize(test->info); ++i)
    fprintf(f, "%s ", test->info[i]);
fprintf(f, "%s\n", test->status->errMessage);
}

char *nearStartTablePat = "<!-- Start Rows -->";
char *nearEndTablePat = "<!-- End Rows -->";
char *nearEndRowPat = "<!-- Row -->";

int nearCountRows(struct htmlPage *page)
/* Count number of rows in big table. */
{
return qaCountBetween(page->htmlText, nearStartTablePat,
	nearEndTablePat, nearEndRowPat);
}

int nearCountUniqAccRows(struct htmlPage *page)
/* Count number of unique rows in table containing just hyperlinked 
 * accessions. */
{
char *s, *e, *row, *acc;
int count = 0;
struct hash *uniqHash = hashNew(0);

if (page == NULL)
    return -1;

/* Set s to first row. */
s = stringIn(nearStartTablePat, page->htmlText);
if (s == NULL)
    return -1;
s += strlen(nearStartTablePat);

for (;;)
    {
    e = stringIn(nearEndRowPat, s);
    if (e == NULL)
        break;
    row = cloneStringZ(s, e-s);
    acc = qaStringBetween(row, "_blank>", "</a>");
    if (acc == NULL)
        {
	warn("Can't find between _blank> and </a> while counting uniq row %s",
		row);
	freez(&row);
	break;
	}
    if (!hashLookup(uniqHash, acc))
        {
	hashAdd(uniqHash, acc, NULL);
	++count;
	}
    freez(&row);
    freez(&acc);
    s = e + strlen(nearEndRowPat);
    }
hashFree(&uniqHash);
return count;
}

struct htmlPage *quickSubmit(struct htmlPage *basePage,
	char *org, char *db, char *type, char *sort, char *output, char *userSeq, 
	char *testName, char *button, char *buttonVal)
/* Submit page and record info.  Return NULL if a problem. */
{
struct blatTest *test;
struct qaStatus *qs;
struct htmlPage *page;
if (basePage != NULL)
    {
    if (org != NULL)
	htmlPageSetVar(basePage, NULL, "org", org);
    if (db != NULL)
	htmlPageSetVar(basePage, NULL, "db", db);
    if (userSeq != NULL)
	htmlPageSetVar(basePage, NULL, "userSeq", userSeq);
    qs = qaPageFromForm(basePage, basePage->forms, 
	    button, buttonVal, &page);
    test = blatTestNew(qs, testName, org, db, type, sort, output);
    }
return page;
}

void serialSubmit(struct htmlPage **pPage,
	char *org, char *db, char *type, char *sort, char *output, char *userSeq, 
	char *testName, char *button, char *buttonVal)
/* Submit page, replacing old page with new one. */
{
struct htmlPage *oldPage = *pPage;
if (oldPage != NULL)
    {
    *pPage = quickSubmit(oldPage, org, db, type, sort, output, userSeq, 
    	testName, button, buttonVal);
    htmlPageFree(&oldPage);
    }
}

void quickErrReport()
/* Report error at head of list if any */
{
struct blatTest *test = blatTestList;
if (test->status->errMessage != NULL)
    blatTestLogOne(test, stderr);
}

void testCol(struct htmlPage *emptyConfig, char *org, char *db, char *col, char *gene)
/* Test one column. */
{
struct htmlPage *printPage = NULL;
char visVar[256];
safef(visVar, sizeof(visVar), "near.col.%s.vis", col);
htmlPageSetVar(emptyConfig, NULL, visVar, "on");
htmlPageSetVar(emptyConfig, NULL, orderVarName, "geneDistance");
htmlPageSetVar(emptyConfig, NULL, countVarName, "25");

//printPage = quickSubmit(emptyConfig, NULL, org, db, col, gene, "colPrint", "Submit", "on");
if (printPage != NULL)
    {
    int expectCount = 25;
    int lineCount = nearCountRows(printPage);
    if (lineCount != expectCount)
	qaStatusSoftError(blatTestList->status, 
		"Got %d rows, expected %d", lineCount, expectCount);
    }
quickErrReport();
htmlPageFree(&printPage);
htmlPageSetVar(emptyConfig, NULL, visVar, NULL);
}

struct htmlPage *emptyConfigPage(struct htmlPage *dbPage, char *org, char *db)
/* Get empty configuration page. */
{
//return quickSubmit(dbPage, NULL, org, db, NULL, NULL, "emptyConfig", hideAllConfName, "on");
return NULL;  //debug
}

void testColInfo(struct htmlPage *dbPage, char *org, char *db, char *col) 
/* Click on all colInfo columns. */
{
struct htmlPage *infoPage = NULL; //debug 
//    quickSubmit(dbPage, NULL, org, db, col, NULL, "colInfo", colInfoVarName, col);

if (infoPage != NULL)
    {
    if (stringIn("No additional info available", infoPage->htmlText))
	qaStatusSoftError(blatTestList->status, 
		"%s failed - no %s.html?", colInfoVarName, col);
    }
quickErrReport();
htmlPageFree(&infoPage);
}

void testDbColumns(struct htmlPage *dbPage, char *org, char *db, 
	struct slName *geneList)
/* Test on one database. */
{
struct htmlPage *emptyConfig;
struct slName *colList = NULL, *col;
struct htmlFormVar *var;
struct slName *gene;

uglyf("testDbColumns %s %s\n", org, db);
emptyConfig = emptyConfigPage(dbPage, org, db);
if (emptyConfig != NULL )
    {
    for (var = emptyConfig->forms->vars; var != NULL; var = var->next)
	{
	if (startsWith("near.col.", var->name) && endsWith(var->name, ".vis"))
	    {
	    char *colNameStart = var->name + strlen("near.col.");
	    char *colNameEnd = strchr(colNameStart, '.');
	    *colNameEnd = 0;
	    col = slNameNew(colNameStart);
	    slAddHead(&colList, col);
	    *colNameEnd = '.';
	    }
	}
    slReverse(&colList);

    for (gene = geneList; gene != NULL; gene = gene->next)
	{
	htmlPageSetVar(emptyConfig, NULL, searchVarName, gene->name);
	for (col = colList; col != NULL; col = col->next)
	    {
	    testCol(emptyConfig, org, db, col->name, gene->name);
	    }
	}
    for (col = colList; col != NULL; col = col->next)
        {
	testColInfo(dbPage, org, db, col->name);
	}
    }
htmlPageFree(&emptyConfig);
}


void testSortX(struct htmlPage *emptyConfig, char *org, char *db, char *sort, char *gene, char *accColumn)
/* Test one column. */
{
char accVis[256];
struct htmlPage *printPage = NULL;
safef(accVis, sizeof(accVis), "near.col.%s.vis", accColumn);
htmlPageSetVar(emptyConfig, NULL, accVis, "on");
htmlPageSetVar(emptyConfig, NULL, orderVarName, sort);
htmlPageSetVar(emptyConfig, NULL, countVarName, "25");
htmlPageSetVar(emptyConfig, NULL, searchVarName, gene);

//printPage = quickSubmit(emptyConfig, sort, org, db, NULL, gene, "sortType", "submit", "on");
if (printPage != NULL)
    {
    int lineCount = nearCountRows(printPage);
    if (lineCount < 1)
	qaStatusSoftError(blatTestList->status, "No rows for sort %s", sort);
    }
quickErrReport();
htmlPageFree(&printPage);
}



void testDbSorts(struct htmlPage *dbPage, char *org, char *db, 
	char *accColumn, struct slName *geneList)
/* Test on one database. */
{
struct htmlPage *emptyConfig;
struct htmlFormVar *sortVar = htmlFormVarGet(dbPage->forms, orderVarName);
struct slName *gene, *sort;

uglyf("testDbSorts %s %s\n", org, db);
if (sortVar == NULL)
    errAbort("Couldn't find var %s", orderVarName);

emptyConfig = emptyConfigPage(dbPage, org, db);
if (emptyConfig != NULL)
    {
    for (sort = sortVar->values; sort != NULL; sort= sort->next)
	{
	for (gene = geneList; gene != NULL; gene = gene->next)
	    {
	    testSortX(emptyConfig, org, db, sort->name, gene->name, accColumn);
	    }
	}
    htmlPageFree(&emptyConfig);
    }
}

void testDbFilters(struct htmlPage *dbPage, char *org, char *db, 
	char *accColumn, struct slName *geneList)
/* Test filter that returns just geneList. */
{
struct slName *gene;
int rowCount;
char accFilter[256];
safef(accFilter, sizeof(accFilter), "near.as.%s.wild", accColumn);

/* Start out with filter page. */
struct htmlPage *page = NULL; //debug
//quickSubmit(dbPage, NULL, org, db, accColumn, NULL,
//	"accOneFilterPage", advFilterVarName, "on");
verbose(1, "testFilters %s %s\n", org, db);
if (page == NULL)
    return;

/* Set up to filter exactly one gene. */
    {
    htmlPageSetVar(page, NULL, accFilter, geneList->name);
    htmlPageSetVar(page, NULL, searchVarName, geneList->name);
    //serialSubmit(&page, NULL, org, db, accColumn, geneList->name,
    //	"accOneFilterSubmit", "Submit", "on");
    if (page == NULL)
	return;

    /* Make sure really got one gene. */
    rowCount = nearCountUniqAccRows(page);
    if (rowCount != 1)
	{
	qaStatusSoftError(blatTestList->status, 
	    "Acc exact filter returned %d items", rowCount);
	}
    }

/* Set up filter for all genes in list. */
    {
    struct dyString *dy = newDyString(0);
    int geneCount = slCount(geneList);
    for (gene = geneList; gene != NULL; gene = gene->next)
	dyStringPrintf(dy, "%s ", gene->name);
    htmlPageSetVar(page, NULL, accFilter, dy->string);
    htmlPageSetVar(page, NULL, countVarName, "all");  /* despite 3 genes requested, must see all if many dupes */
    //serialSubmit(&page, NULL, org, db, accColumn, dy->string,
    //   "accMultiFilterSubmit", "Submit", "on");
    dyStringFree(&dy);
    if (page == NULL)
	return;
    rowCount = nearCountUniqAccRows(page);
    if (rowCount != geneCount)
	{
	qaStatusSoftError(blatTestList->status, 
	    "Acc multi filter expecting %d, got %d items", geneCount, rowCount);
	}
    }

/* Set up filter for wildcard in list. */
    {
    struct dyString *dy = newDyString(0);
    char len = strlen(geneList->name);
    dyStringAppendN(dy, geneList->name, len-1);
    dyStringAppendC(dy, '*');
    htmlPageSetVar(page, NULL, accFilter, dy->string);
//    serialSubmit(&page, NULL, org, db, accColumn, dy->string,
//	    "accWildFilterSubmit", "Submit", "on");
    dyStringFree(&dy);
    if (page == NULL)
	return;
    rowCount = nearCountRows(page);
    if (rowCount < 1)
	{
	qaStatusSoftError(blatTestList->status, 
	    "Acc wild filter no match");
	}
    }


/* Clear out advanced filters. */
    {
    htmlPageFree(&page);
//    page = quickSubmit(dbPage, NULL, org, db, NULL, NULL,
//	"advFilterClear", advFilterClearVarName, "on");
    }
htmlPageFree(&page);
}


// ==========================================================================


// testing mods to hdb.c:  (hopefully compiler will resolve at linktime)
//struct dnaSeq *hGenBankGetMrnaC(struct sqlConnection *conn, char *acc, char *compatTable);
//aaSeq *hGenBankGetPepC(struct sqlConnection *conn, char *acc, char *compatTable);




void testOutput(struct htmlPage *sortPage, struct htmlForm *sortForm, 
    char *org, char *db, char *type, char *sort, char *output,
    struct slName *geneList, char **dna, char **pro)
/* Test on one output. */
{
struct htmlPage *outputPage, *page;
struct htmlForm *mainForm;

struct slName *gene;
char *seq=NULL;

htmlPageSetVar(sortPage, sortForm, "org", org);  
htmlPageSetVar(sortPage, sortForm, "db", db);
htmlPageSetVar(sortPage, sortForm, "type", type);
htmlPageSetVar(sortPage, sortForm, "sort", sort);
htmlPageSetVar(sortPage, sortForm, "output", output);

outputPage = htmlPageFromForm(sortPage, sortPage->forms, "submit", "Submit");
if ((mainForm = htmlFormGet(outputPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form on outputPage");  // we may want to report the error but try to keep going.

int g = 0;
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    if (sameWord(type,"BLAT's guess"))
	{
	if ((rand() % 2) == 0)
	    {
    	    seq = dna[g];
	    }
	else
	    {
    	    seq = pro[g];
	    }
	}
    else if (sameWord(type,"DNA"))
	{
	seq = dna[g];
	}
    else if (sameWord(type,"Protein"))
	{
	seq = pro[g];
	}
    else if (sameWord(type,"translated RNA"))
	{
	seq = dna[g];
	}
    else if (sameWord(type,"translated DNA"))
	{
	seq = dna[g];
	}
    else
	{
	errAbort("unknown type=%s",type);
	}

    
    if (!seq)
	{
    	uglyf("testOutput: seq value NULL for %s.%s.\n",org,db);
	htmlPageFree(&outputPage);
	return;
	}


    page = quickSubmit(outputPage, org, db, type, sort, output, seq, "BlatTest combos", "submit", "Submit");
    quickErrReport();
    if (page != NULL)
	{
	htmlPageFree(&page);
	}

    g++;
    }

htmlPageFree(&outputPage);

}


void testSort(struct htmlPage *typePage, struct htmlForm *typeForm, char *org, char *db, char *type, char *sort,
    struct slName *geneList, char **dna, char **pro)
/* Test on one sort. */
{
struct htmlPage *sortPage;
struct htmlForm *mainForm;
struct htmlFormVar *outputVar;
struct slName *output;

htmlPageSetVar(typePage, typeForm, "org", org);  
htmlPageSetVar(typePage, typeForm, "db", db);
htmlPageSetVar(typePage, typeForm, "type", type);
htmlPageSetVar(typePage, typeForm, "sort", sort);

sortPage = htmlPageFromForm(typePage, typePage->forms, "submit", "Submit");
if ((mainForm = htmlFormGet(sortPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form on sortPage");  // we may want to report the error but try to keep going.
if ((outputVar = htmlFormVarGet(mainForm, "output")) == NULL)
    errAbort("Couldn't get output var");

for (output = outputVar->values; output != NULL; output = output->next)
    {
    if (clOutput == NULL || sameString(clOutput, output->name))
	{
	uglyf("testSort: output->name=%s \n",output->name);
	testOutput(sortPage, mainForm, org, db, type, sort, output->name, geneList, dna, pro);
	}
    }
htmlPageFree(&sortPage);

}


void testType(struct htmlPage *dbPage, struct htmlForm *dbForm, char *org, char *db, char *type, 
    struct slName *geneList, char **dna, char **pro)
/* Test on one type. */
{
struct htmlPage *typePage;
struct htmlForm *mainForm;
struct htmlFormVar *sortVar;
struct slName *sort;

htmlPageSetVar(dbPage, dbForm, "org", org);  
htmlPageSetVar(dbPage, dbForm, "db", db);
htmlPageSetVar(dbPage, dbForm, "type", type);

typePage = htmlPageFromForm(dbPage, dbPage->forms, "submit", "Submit");
if ((mainForm = htmlFormGet(typePage, "mainForm")) == NULL)
    errAbort("Couldn't get main form on typePage");  // we may want to report the error but try to keep going.
if ((sortVar = htmlFormVarGet(mainForm, "sort")) == NULL)
    errAbort("Couldn't get sort var");

for (sort = sortVar->values; sort != NULL; sort = sort->next)
    {
    if (clSort == NULL || sameString(clSort, sort->name))
	{
	uglyf("testType: sort->name=%s \n",sort->name);
	testSort(typePage, mainForm, org, db, type, sort->name, geneList, dna, pro);
	}
    }
htmlPageFree(&typePage);

}

struct hash *findRaSection(struct hash *raList, char *name)
/* find section in ra with this name */
{
struct hash *raHash,*result=NULL;
char *section = NULL;
char *targ = cloneString(name);
eraseWhiteSpace(targ);  /* name entry at top of each group cannot have whitespace */
for (raHash = raList; raHash != NULL; raHash = raHash->next)
    {
    section = hashFindVal(raHash, "name");

    //debug
    //uglyf("section=%s\n",section);
    
    if (section)
	{
	if (sameWord(section,targ))
	    {
    	    result=raHash;
	    }
	}
    }
freez(&targ);
return result;
}

void inheritRa(char **pvar, struct hash *ra, char *name)
/* override previous value if non-null value found */
{
char *temp = hashFindVal(ra, name);
if (temp)
    {
    *pvar = temp;
    }
}

char *getFieldWhereField(char *db, char *table, char *field, char *whereField, char *whereValue)
/* Get random sample from database. */
{
struct sqlConnection *conn = sqlConnect(db);
char query[256], **row;
struct sqlResult *sr;
char *result=NULL;
sqlSafef(query, sizeof(query), "select %s from %s where %s = '%s'", 
	field, table, whereField, whereValue);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    result = cloneString(row[0]);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return result;
}


int genePredCdnaSize(struct genePred *gp)
/* Return total size of all exons. */
{
int totalSize = 0;
int exonIx;

for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    totalSize += (gp->exonEnds[exonIx] - gp->exonStarts[exonIx]);
    }
return totalSize;
}



struct dnaSeq *hDnaFromSeqD(char *db, char *seqName, int start, int end, enum dnaCase dnaCase)
/* Fetch DNA (galt added db) */
{
char fileName[512];
char query[512];
struct dnaSeq *seq = NULL;
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
char **row;
sqlSafef(query, sizeof(query), "select fileName from chromInfo where chrom='%s'", seqName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    safef(fileName,sizeof(fileName),"%s",row[0]);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
seq = nibLoadPart(fileName, start, end-start);
if (dnaCase == dnaUpper)
    touppers(seq->dna);
return seq;
}


struct dnaSeq *getCdnaSeqD(char *db, struct genePred *gp)
/* Load in cDNA sequence associated with gene prediction. */
{
int txStart = gp->txStart;  // hDnaFromSeq
struct dnaSeq *genoSeq = hDnaFromSeqD(db, gp->chrom, txStart, gp->txEnd,  dnaLower);
struct dnaSeq *cdnaSeq;
int cdnaSize = genePredCdnaSize(gp);
int cdnaOffset = 0, exonStart, exonSize, exonIx;

AllocVar(cdnaSeq);
cdnaSeq->dna = needMem(cdnaSize+1);
cdnaSeq->size = cdnaSize;
for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    exonStart = gp->exonStarts[exonIx];
    exonSize = gp->exonEnds[exonIx] - exonStart;
    memcpy(cdnaSeq->dna + cdnaOffset, genoSeq->dna + (exonStart - txStart), exonSize);
    cdnaOffset += exonSize;
    }
assert(cdnaOffset == cdnaSeq->size);
freeDnaSeq(&genoSeq);
return cdnaSeq;
}


void getCdsInMrna(struct genePred *gp, int *retCdsStart, int *retCdsEnd)
/* Given a gene prediction, figure out the
 * CDS start and end in mRNA coordinates. */
{
int missingStart = 0, missingEnd = 0;
int exonStart, exonEnd, exonSize, exonIx;
int totalSize = 0;

for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    exonStart = gp->exonStarts[exonIx];
    exonEnd = gp->exonEnds[exonIx];
    exonSize = exonEnd - exonStart;
    totalSize += exonSize;
    missingStart += exonSize - positiveRangeIntersection(exonStart, exonEnd, gp->cdsStart, exonEnd);
    missingEnd += exonSize - positiveRangeIntersection(exonStart, exonEnd, exonStart, gp->cdsEnd);
    }
*retCdsStart = missingStart;
*retCdsEnd = totalSize - missingEnd;
}


struct dnaSeq *htcGeneMrna(char *db, char * table, char *geneName)
/* Display cDNA predicted from genome */
{
char query[512];
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
char **row;
struct genePred *gp;
struct dnaSeq *seq=NULL;
int cdsStart, cdsEnd;
int rowOffset = 0;
char *fld=NULL;
int f = 0;

sqlSafef(query, sizeof(query), "select * from %s where name = '%s'", table, geneName);
sr = sqlGetResult(conn, query);
while ((fld = sqlFieldName(sr)) != NULL)
    {
    if (sameString(fld,"bin"))
	{
	rowOffset = f+1;
	}
    f++;
    }
uglyf("rowOffset=%d \n",rowOffset);    
								
if ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row+rowOffset);
    seq = getCdnaSeqD(db, gp);
    getCdsInMrna(gp, &cdsStart, &cdsEnd);
    toUpperN(seq->dna + cdsStart, cdsEnd - cdsStart);
    if (gp->strand[0] == '-')
	{
	reverseComplement(seq->dna, seq->size);
	}
    genePredFree(&gp);
    //freeDnaSeq(&seq); don't free it, return it.
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return seq;
}

									    


void testDb(struct htmlPage *orgPage, struct htmlForm *orgForm, char *org, char *db)
/* Test on one database. */
{

struct hash *genomeRa=NULL;
char *dnaTable = NULL;
char *lnkTable = NULL;
char *proTable = NULL;
char *dnaColumn = NULL;
char *lnkColumn = NULL;
char *proColumn = NULL;
char *method = NULL;

genomeRa=findRaSection(raList,"global");
if (!genomeRa)
    {
    errAbort("testDb: .ra has no global section \n");
    }
inheritRa(&dnaTable,  genomeRa, "dna");
inheritRa(&lnkTable,  genomeRa, "lnk");
inheritRa(&proTable,  genomeRa, "pro");
inheritRa(&dnaColumn, genomeRa, "dnaColumn");
inheritRa(&lnkColumn, genomeRa, "lnkColumn");
inheritRa(&proColumn, genomeRa, "proColumn");
inheritRa(&method,    genomeRa, "method");

genomeRa=findRaSection(raList,org);   
if (!genomeRa)
    {
    uglyf("testDb: skipping org=%s, has no .ra section \n",org);
    return;   /* if there's no section for the organism, assume it should be skipped. */
    }
inheritRa(&dnaTable,  genomeRa, "dna");
inheritRa(&lnkTable,  genomeRa, "lnk");
inheritRa(&proTable,  genomeRa, "pro");
inheritRa(&dnaColumn, genomeRa, "dnaColumn");
inheritRa(&lnkColumn, genomeRa, "lnkColumn");
inheritRa(&proColumn, genomeRa, "proColumn");
inheritRa(&method,    genomeRa, "method");

genomeRa=findRaSection(raList,db);   
if (!genomeRa)
    {
    uglyf("testDb: skipping db=%s, has no .ra section \n",db);
    return;   /* if there's no section for the db, assume it should be skipped. */
    }
inheritRa(&dnaTable,  genomeRa, "dna");
inheritRa(&lnkTable,  genomeRa, "lnk");
inheritRa(&proTable,  genomeRa, "pro");
inheritRa(&dnaColumn, genomeRa, "dnaColumn");
inheritRa(&lnkColumn, genomeRa, "lnkColumn");
inheritRa(&proColumn, genomeRa, "proColumn");
inheritRa(&method,    genomeRa, "method");

uglyf("dnaTable=%s \n",dnaTable);
if (!dnaTable)
    {
    uglyf("testDb: dnaTable missing for org=%s db=%s line, skipping \n",org,db);
    return;  
    }

uglyf("lnkTable=%s \n",lnkTable);
if (!lnkTable)
    {
    uglyf("testDb: lnkTable missing for org=%s db=%s line, skipping \n",org,db);
    return;  
    }

uglyf("proTable=%s \n",proTable);
if (!proTable)
    {
    uglyf("testDb: proTable missing for org=%s db=%s line, skipping \n",org,db);
    return;  
    }

uglyf("dnaColumn=%s \n",dnaColumn);
if (!dnaColumn)
    {
    uglyf("testDb: dnaColumn missing for org=%s db=%s, skipping \n",org,db);
    return;  
    }

uglyf("lnkColumn=%s \n",lnkColumn);
if (!lnkColumn)
    {
    uglyf("testDb: lnkColumn missing for org=%s db=%s, skipping \n",org,db);
    return;  
    }

uglyf("proColumn=%s \n",proColumn);
if (!proColumn)
    {
    uglyf("testDb: proColumn missing for org=%s db=%s, skipping \n",org,db);
    return;  
    }

uglyf("method=%s \n",method);
if (!method)
    {
    uglyf("testDb: method missing for org=%s db=%s, skipping \n",org,db);
    return;  
    }





struct slName *geneList = NULL;
char **dna;
char **pro;

// debug
//clRepeat = 1;

dna=needMem(clRepeat*sizeof(char*));
pro=needMem(clRepeat*sizeof(char*));

if (sameWord(method,"1"))
    geneList = sqlRandomSample(db, dnaTable, dnaColumn, clRepeat);
else if (sameWord(method,"2"))
    geneList = sqlRandomSample(db, proTable, dnaColumn, clRepeat);
else
    errAbort("unknown method %s in .ra",method);

if (!geneList)
    {
    uglyf("testDb: sqlRandomSample returned empty geneList for %s.%s \n",db,dnaTable);
    return;
    }

struct htmlPage *dbPage;

//debug
    struct dyString *dy = newDyString(0);
    struct slName *gene;
    //char *dna = NULL;
    //HGID retId = 0;
    struct dnaSeq *dnaseq=NULL;
    aaSeq *proseq=NULL;

    struct sqlConnection *conn = hAllocOrConnect(db);

    uglyf("sqlGetDatabase(conn) = %s\n", sqlGetDatabase(conn) );

    //uglyf("host=%s, db=%s, user=%s, pwd=%s \n", hGetDbHost(), hGetDbName(), hGetDbUser(), hGetDbPassword());
    
    char *acc = NULL;
    int g = 0;
    for (gene = geneList; gene != NULL; gene = gene->next)
	{

	//uglyf("testDb: got to top of geneList loop for %s.%s \n",db,dnaTable);
	
	acc = gene->name;
	
	//uglyf("testDb: got past acc=gene->name for %s.%s \n",db,dnaTable);
	
	//acc = "NM_020967";
	//acc = "AB002332";
	dyStringPrintf(dy, "%s ", acc);
        //dna = hGetSeqAndId(conn, acc, &retId);
	
	//uglyf("testDb: about to call hgGetSeqAndId for %s.%s \n",db,dnaTable);
	//tempdna = hGetSeqAndId(conn, acc, NULL);
	//if (tempdna)
	//    {
	//    dnaseq = faFromMemText(tempdna);
	//    //uglyf("\ntestDb: gene = %s. size=%d.\n tempdna=%s.\n",acc,strlen(tempdna),tempdna);
	//    //uglyf("\ntestDb: dnaseq->dna=%s.\n",dnaseq->dna);
	//    }
	//else
	//    {
	//    uglyf("testDb: hGetSeqAndId returned empty result for %s.%s \n",db,dnaTable);
	//    return;
	//    }

	if (sameWord(method,"1"))
	    {
	    //uglyf("testDb: about to call hgGenBankGetMrnaC for %s.%s \n",db,dnaTable);
    	    dnaseq = hGenBankGetMrnaC(conn, acc, NULL);
      	    if (!dnaseq)
		{
		uglyf("testDb: hGenBankGetMrnaC returned empty result for %s.%s \n",db,dnaTable);
		return;
		}
	    }
	else if (sameWord(method,"2"))
	    {
	    //uglyf("testDb: about to call htcGeneMrna for %s.%s.%s \n",db,dnaTable,acc);
    	    dnaseq = htcGeneMrna(db, dnaTable, acc);
      	    if (!dnaseq)
		{
		uglyf("testDb: htcGeneMrna returned empty result for %s.%s.%s \n",db,dnaTable,acc);
		return;
		}
	    }
    	else
	    {
    	    errAbort("unknown method %s in .ra",method);
	    }



	char *protAcc = getFieldWhereField(db, lnkTable, proColumn, lnkColumn, acc);
	uglyf("testDb: protAcc=%s for %s.%s.%s='%s' \n",protAcc,db,dnaTable,dnaColumn,acc);


	//debug: restore this line:
    	proseq = hGenBankGetPepC (conn, protAcc, proTable);
	if (!proseq)
	    {
	    uglyf("\n testDb: hGenBankGetPepC returned empty result for %s.%s \n",db,proTable);
	    return;
	    }


	//dna = dnaseq->dna;
        //uglyf("testDb: gene = %s. dnasize=%d. retId=%u.\n",acc,strlen(dna),retId);
        //uglyf("testDb: gene = %s. dna=%lu. retId=%u.\n",acc,(unsigned long)dna,retId);
        ///uglyf("testDb: gene = %s. dna=%s. \n",acc,dna);
	if (dnaseq != NULL)
	    {
	    if (dnaseq->dna != NULL)
		{
		//uglyf("\n testDb: gene = %s. size=%d.\n dnaseq->dna=%s.\n",acc,strlen(dnaseq->dna),dnaseq->dna);

		//struct htmlForm *mainForm;
		//if ((mainForm = htmlFormGet(orgPage, "mainForm")) == NULL)
		//    errAbort("Couldn't get main form");

		/*
		uglyf("\nbefore: org=%s, db=%s, type=%s, sort=%s, output=%s, userSeq=%s\n",
		    (htmlFormVarGet(mainForm, "org"))->curVal, 
		    (htmlFormVarGet(mainForm, "db"))->curVal,
		    (htmlFormVarGet(mainForm, "type"))->curVal, 
		    (htmlFormVarGet(mainForm, "sort"))->curVal, 
		    (htmlFormVarGet(mainForm, "output"))->curVal,
		    (htmlFormVarGet(mainForm, "userSeq"))->curVal
		);
		*/
		
		/*
		struct htmlFormVar *var;
		var = htmlFormVarGet(form, name);
		if (var == NULL)
		    {
		    AllocVar(var);
		    var->type = "TEXT";
		    var->tagName = "INPUT";
		    var->name = name;
		    var->curVal;
		    }
		*/
		
	        //if (org != NULL)
		//    htmlPageSetVar(orgPage, mainForm, "org", org);
	    	//if (db != NULL)
		//    htmlPageSetVar(orgPage, mainForm, "db", db);

		//htmlPageSetVar(orgPage, mainForm, "type", "DNA");
		//htmlPageSetVar(orgPage, mainForm, "sort", "score");
		
		//htmlPageSetVar(orgPage, mainForm, "output", "psl");
		
		//htmlPageSetVar(orgPage, mainForm, "userSeq", dnaseq->dna);
		//htmlPageSetVar(orgPage, mainForm, "seqFile", dnaseq->dna);

		/*
		uglyf("\nafter: org=%s, db=%s, type=%s, sort=%s, output=%s, userSeq=%s\n",
		    (htmlFormVarGet(mainForm, "org"))->curVal, 
		    (htmlFormVarGet(mainForm, "db"))->curVal,
		    (htmlFormVarGet(mainForm, "type"))->curVal, 
		    (htmlFormVarGet(mainForm, "sort"))->curVal, 
		    (htmlFormVarGet(mainForm, "output"))->curVal,
		    (htmlFormVarGet(mainForm, "userSeq"))->curVal
		);
		*/

		/*
		struct htmlFormVar *var;
		var = htmlFormVarGet(mainForm, "seqFile");
		if (var == NULL)
		    {
		    uglyf("var=htmlFormVarGet returned %lu \n",(unsigned long)var);
		    }
		else
		    {
		    uglyf("\ntype=%s, tagName=%s, name=%s, curVal=%s \n",
		    var->type,
		    var->tagName,
		    var->name,
		    var->curVal
		    );
		    }
		*/
		
		//struct qaStatus *qs;
		//struct htmlPage *page;
		//qs = qaPageFromForm(orgPage, orgPage->forms, "Submit", "Submit", &page);
		//qs = qaPageFromForm(orgPage, htmlFormGet(orgPage, "mainForm"), "Submit", "Submit", &page);
		
		//page = htmlPageFromForm(orgPage, mainForm, "Submit", "Submit");
		
		//uglyf("qs.errMessage=%s, qs.hardError=%d \n",qs->errMessage,qs->hardError);
		//uglyf("%s\n",page->htmlText);
		
		}
	    }
	if (proseq != NULL)
	    {
	    if (proseq->dna != NULL)
		{
		//uglyf("\n testDb: gene = %s. size=%d. \n proseq->dna=%s.\n",acc,strlen(proseq->dna),proseq->dna);
		}
	    }

	//freez(&dna);

	//save for later
        dna[g]=cloneString(dnaseq->dna);
        pro[g]=cloneString(proseq->dna);

	freez(&dnaseq);
	freez(&proseq);


	//uglyf("dbg: got here3 \n");

	
	g++;
	}
    uglyf("testDb: genelist = %s.\n",dy->string);
    dyStringFree(&dy);
    //uglyf("host=%s, db=%s, user=%s, pwd=%s \n", hGetDbHost(), hGetDbName(), hGetDbUser(), hGetDbPassword());
    hFreeOrDisconnect(&conn);
   
htmlPageSetVar(orgPage, orgForm, "db", db);
//dbPage = quickSubmit(orgPage, org, db, NULL, NULL, NULL, NULL, "dbEmptyPage", "submit", "Submit");
//quickErrReport();

//uglyf("dbg: got here5 \n");

dbPage = htmlPageFromForm(orgPage, orgPage->forms, "submit", "Submit");

//uglyf("dbg: got here6 \n dbPage->htmlText=%s\n",dbPage->htmlText);

if (dbPage != NULL)
    {
    struct htmlForm *mainForm;
    struct htmlFormVar *typeVar;
    struct slName *type;
    //testDbColumns(dbPage, org, db, geneList);
    //testDbSorts(dbPage, org, db, dnaColumn, geneList);
    //testDbFilters(dbPage, org, db, dnaColumn, geneList);

//	uglyf("dbg: got here4 \n");

    if ((mainForm = htmlFormGet(dbPage, "mainForm")) == NULL)
	errAbort("Couldn't get main form on dbPage");  // we may want to report the error but try to keep going.
    if ((typeVar = htmlFormVarGet(mainForm, "type")) == NULL)
	errAbort("Couldn't get type var");

    for (type = typeVar->values; type != NULL; type = type->next)
	{
	if (clType == NULL || sameString(clType, type->name))
	    {
	    uglyf("testDb: type->name=%s \n",type->name);
	    testType(dbPage, mainForm, org, db, type->name, geneList, dna, pro);
	    }
	}
    }

//	uglyf("dbg: got here7 \n");

htmlPageFree(&dbPage);
for (g=0;g<clRepeat;g++)
    {
    freez(&dna[g]);
    freez(&pro[g]);
    }
freez(&dna);
freez(&pro);


//	uglyf("dbg: got here8 \n");

}


void testOrg(struct htmlPage *rootPage, struct htmlForm *rootForm, char *org)
/* Test on organism.  If forceDb is non-null, only test on
 * given database. */
{
struct htmlPage *orgPage;
struct htmlForm *mainForm;
struct htmlFormVar *dbVar;
struct slName *db;

htmlPageSetVar(rootPage, rootForm, "org", org);  
htmlPageSetVar(rootPage, rootForm, "db", NULL);

if (!findRaSection(raList,org))
    {
    uglyf("testOrg: skipping org=%s, has no .ra section \n",org);
    return;   /* if there's no section for the organism, assume it should be skipped. */
    }

printf("\n");  // separation

orgPage = htmlPageFromForm(rootPage, rootPage->forms, "submit", "Submit");
if ((mainForm = htmlFormGet(orgPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form on orgPage");  // we may want to report the error but try to keep going.
if ((dbVar = htmlFormVarGet(mainForm, "db")) == NULL)
    errAbort("Couldn't get db var");

for (db = dbVar->values; db != NULL; db = db->next)
    {
    if (clDb == NULL || sameString(clDb, db->name))
	{
	uglyf("testOrg: db->name=%s \n",db->name);
	testDb(orgPage, mainForm, org, db->name);
	}
    }
htmlPageFree(&orgPage);
}

void statsOnSubsets(struct blatTest *list, int subIx, FILE *f)
/* Report tests of certain subtype. */
{
struct blatTest *test;
struct hash *hash = newHash(0);
struct slName *typeList = NULL, *type;

fprintf(f, "\n%s subtotals\n", blatTestInfoTypes[subIx]);

/* Get list of all types in this field. */
for (test = list; test != NULL; test = test->next)
    {
    char *info = test->info[subIx];
    if (!hashLookup(hash, info))
       {
       type = slNameNew(info);
       hashAdd(hash, info, type);
       slAddHead(&typeList, type);
       }
    }
slNameSort(&typeList);
hashFree(&hash);

for (type = typeList; type != NULL; type = type->next)
    {
    struct qaStatistics *stats;
    AllocVar(stats);
    for (test = list; test != NULL; test = test->next)
        {
	if (sameString(type->name, test->info[subIx]))
	    {
	    qaStatisticsAdd(stats, test->status);
	    }
	}
    qaStatisticsReport(stats, type->name, f);
    freez(&stats);
    }
}


void reportSummary(struct blatTest *list, FILE *f)
/* Report summary of test results. */
{
struct qaStatistics *stats;
struct blatTest *test;
AllocVar(stats);
int i;

for (i=0; i<=ntiiOutput; ++i)
    statsOnSubsets(list, i, f);
for (test = list; test != NULL; test = test->next)
    qaStatisticsAdd(stats, test->status);
qaStatisticsReport(stats, "Total", f);
}


void reportAll(struct blatTest *list, FILE *f)
/* Report all tests. */
{
struct blatTest *test;
for (test = list; test != NULL; test = test->next)
    {
    if (test->status->errMessage != NULL)
	blatTestLogOne(test, f);
    }
}

void hgBlatTest(char *url, char *log)
/* hgblatTest - Test hgBlat web page. */
{
struct htmlPage *rootPage = htmlPageGet(url);
struct htmlForm *mainForm;
struct htmlFormVar *orgVar;
FILE *f = mustOpen(log, "w");
htmlPageValidateOrAbort(rootPage);
/* global settings? 
htmlPageSetVar(rootPage, NULL, orderVarName, "geneDistance");
htmlPageSetVar(rootPage, NULL, countVarName, "25");
*/
if ((mainForm = htmlFormGet(rootPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form");
if ((orgVar = htmlFormVarGet(mainForm, "org")) == NULL)
    errAbort("Couldn't get org var");
if (clOrg != NULL)
    {
    uglyf("clOrg=%s\n",clOrg);
    testOrg(rootPage, mainForm, clOrg);
    }
else
    {
    struct slName *org;
    for (org = orgVar->values; org != NULL; org = org->next)
        {
	uglyf("clOrg=%s\n",org->name);
	testOrg(rootPage, mainForm, org->name);
	}
    }

htmlPageFree(&rootPage);


slReverse(&blatTestList);
reportSummary(blatTestList, stdout);

reportAll(blatTestList, f);
fprintf(f, "---------------------------------------------\n");
reportSummary(blatTestList, f);


}


void hgBlatTestX(char *url, char *log)
{
struct htmlPage *page;
struct qaStatus *qs;
qs = qaPageGet(url, &page);
qaStatusReportOne(stdout, qs, url);
htmlPageFree(&page);
}

int main(int argc, char *argv[])
/* Process command line. */
{

pushCarefulMemHandler(200000000);

/* Set initial seed */
srand( (unsigned)time( NULL ) );
 
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clDb = optionVal("db", clDb);
clOrg = optionVal("org", clOrg);
clType = optionVal("type", clType);
clSort = optionVal("sort", clSort);
clOutput = optionVal("output", clOutput);
dataDir = optionVal("dataDir", dataDir);
clRepeat = optionInt("repeat", clRepeat);

raList = hgReadRa("org", "db", dataDir, raName, NULL);
if (raList == NULL)
    errAbort("Couldn't find anything from %s", raName);

hgBlatTest(argv[1], argv[2]);
//hgBlatTestX(argv[1], argv[2]);

hashFreeList(&raList);
carefulCheckHeap();
return 0;
}



