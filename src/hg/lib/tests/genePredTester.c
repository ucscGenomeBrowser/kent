/* genePredTester - test program for genePred and genePredReader */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "genePred.h"
#include "genePredReader.h"
#include "options.h"
#include "psl.h"
#include "gff.h"
#include "genbank.h"
#include "localmem.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "binRange.h"
#include "verbose.h"


void usage(char *msg)
/* Explain usage and exit. */
{
errAbort(
    "%s\n\n"
    "genePredTester - test program for genePred and genePredReader\n"
    "\n"
    "Usage:\n"
    "   genePredTester [options] task [args...]\n"
    "\n"
    "Tasks and arguments:\n"
    "\n"
    "o readTable db table\n"
    "  Read rows from db.table into genePred objects, determining the\n"
    "  columns dynamically. If -output is specified, the objects are\n"
    "  written to that file. Field name columns are ignored.\n"
    "\n"
    "o loadTable db table gpFile\n"
    "  Create and load the specified table from gbFile.  Field command options\n"
    "  defined optional columns in file.\n"
    "\n"
    "o readFile gpFile\n"
    "  Read rows from the tab-separated file, Field command options\n"
    "  defined optional columns in file.  If -output is specified, the\n"
    "  objects are written to that file.\n"
    "\n"
    "o fromPsl pslFile cdsFile \n"
    "  Create genePred objects from PSL.If -output is specified, the\n"
    "  objects are written to that file.\n"
    "\n"
    "o fromGff gffFile \n"
    "  Create genePred objects from a GFF file..If -output is specified, the\n"
    "  objects are written to that file.\n"
    "\n"
    "o fromGtf gtfFile \n"
    "  Create genePred objects from a GTF file..If -output is specified, the\n"
    "  objects are written to that file.\n"
    "\n"
    "Options:\n"
    "  -verbose=1 - set verbose level\n"
    "  -scoreFld - include id field\n"
    "  -name2Fld - include name2 field\n"
    "  -cdsStatFld - include cdsStat fields\n"
    "  -exonFramesFld - include exonFrames field\n"
    "  -genePredExt - include all extended fields\n"
    "  -maxRows=n - maximum number of records to process\n"
    "  -minRows=1 - error if less than this number of rows read\n"
    "  -needRows=n - error if this number of rows read doesn't match\n"
    "  -where=where - optional where clause for query\n"
    "  -chrom=chrom - restrict file reading to this chromosome\n"
    "  -output=fname - write output to this file \n"
    "  -info=fname - write info about genePred to this file.\n"
    "  -withBin - create bin column when loading a table\n"
    "  -exonSelectWord=feat - use feat as the exon feature name when creating\n"
    "   from GFF\n"
    "  -ignoreUnconverted - don't print warnings when genens couldn't be converted\n"
    "   to genePreds\n"
    "  -impliedStopAfterCds - implied stop codon in GFF/GTF after CDS\n"
    "  -noCheck - don't geneCheck results\n",
    msg);
}

static struct optionSpec options[] = {
    {"scoreFld", OPTION_BOOLEAN},
    {"name2Fld", OPTION_BOOLEAN},
    {"cdsStatFld", OPTION_BOOLEAN},
    {"exonFramesFld", OPTION_BOOLEAN},
    {"genePredExt", OPTION_BOOLEAN},
    {"maxRows", OPTION_INT},
    {"minRows", OPTION_INT},
    {"needRows", OPTION_INT},
    {"where", OPTION_STRING},
    {"output", OPTION_STRING},
    {"info", OPTION_STRING},
    {"withBin", OPTION_BOOLEAN},
    {"noCheck", OPTION_BOOLEAN},
    {"exonSelectWord", OPTION_STRING},
    {"ignoreUnconverted", OPTION_BOOLEAN},
    {"impliedStopAfterCds", OPTION_BOOLEAN},
    {NULL, 0},
};
unsigned gOptFields = genePredNoOptFld;;
unsigned gCreateOpts = genePredBasicSql;
unsigned gGxfCreateOpts = genePredGxfDefaults;
int gMaxRows = BIGNUM;
int gMinRows = 1;
int gNeedRows = -1;
char *gWhere = NULL;
char *gChrom = NULL;
char *gOutput = NULL;
char *gInfo = NULL;
char *gExonSelectWord = NULL;
boolean gIgnoreUnconverted = FALSE;
boolean gNoCheck = FALSE;
int errCount = 0;

void checkGenePred(char *desc, struct genePred* gp)
/* check a genePred */
{
if (!gNoCheck)
    {
    if (genePredCheck(desc, stderr, -1, gp) > 0)
        errCount++;
    }
}

void checkNumRows(char *src, int numRows)
/* check the number of row constraints */
{
if (numRows < gMinRows)
    errAbort("expected at least %d rows from %s, got %d", gMinRows, src, numRows);
if ((gNeedRows >= 0) && (numRows != gNeedRows))
    errAbort("expected %d rows from %s, got %d", gNeedRows, src, numRows);
verbose(2, "read %d rows from %s\n", numRows, src);
}

void writeOptField(FILE* infoFh, struct genePred *gp, char* desc,
                   enum genePredFields field)
/* write info about one field */
{
fprintf(infoFh, "%s: %s\n", desc, ((gp->optFields & field) ? "yes" : "no"));
}

void writeInfo(struct genePred *gp)
/* write info about the genePred to the info file */
{
if (gInfo != NULL)
    {
    FILE *infoFh = mustOpen(gInfo, "w");
    writeOptField(infoFh, gp, "genePredScoreFld", genePredScoreFld);
    writeOptField(infoFh, gp, "genePredName2Fld", genePredName2Fld);
    writeOptField(infoFh, gp, "genePredCdsStatFld", genePredCdsStatFld);
    writeOptField(infoFh, gp, "genePredExonFramesFld", genePredExonFramesFld);
    carefulClose(&infoFh);
    }
}

void readTableTask(char *db, char *table)
/* Implements the readTable task */
{
FILE *outFh = NULL;
struct sqlConnection *conn = sqlConnect(db);
struct genePredReader* gpr = genePredReaderQuery(conn, table, gWhere);
struct genePred* gp;
int numRows = 0;

if (gOutput != NULL)
    outFh = mustOpen(gOutput, "w");

while ((numRows < gMaxRows) && ((gp = genePredReaderNext(gpr)) != NULL))
    {
    checkGenePred(table, gp);
    if (outFh != NULL)
        genePredTabOut(gp, outFh);
    if (numRows == 0)
        writeInfo(gp);
    genePredFree(&gp);
    numRows++;
    }

carefulClose(&outFh);
genePredReaderFree(&gpr);

sqlDisconnect(&conn);
checkNumRows(table, numRows);
}

void loadTableTask(char *db, char *table, char* gpFile)
/* Implements the loadTable task */
{
struct genePredReader* gpr = genePredReaderFile(gpFile, gChrom);
char *sqlCmd = genePredGetCreateSql(table, gOptFields, gCreateOpts, 0);
struct sqlConnection *conn;
struct genePred* gp;
int numRows = 0;
char tabFile[PATH_LEN];
FILE *tabFh;

safef(tabFile, sizeof(tabFile), "genePred.%d.tmp", getpid());
tabFh = mustOpen(tabFile, "w");

while ((numRows < gMaxRows) && ((gp = genePredReaderNext(gpr)) != NULL))
    {
    if (gCreateOpts & genePredWithBin)
        fprintf(tabFh, "%u\t", binFromRange(gp->txStart, gp->txEnd));
    checkGenePred(gpFile, gp);
    genePredTabOut(gp, tabFh);
    if (numRows == 0)
        writeInfo(gp);
    genePredFree(&gp);
    numRows++;
    }

carefulClose(&tabFh);
conn = sqlConnect(db);
sqlRemakeTable(conn, table, sqlCmd);
sqlLoadTabFile(conn, tabFile, table, 0);
sqlDisconnect(&conn);

unlink(tabFile);
freeMem(sqlCmd);
genePredReaderFree(&gpr);
checkNumRows(gpFile, numRows);
}

void readFile(char *gpFile)
/* Implements the readFile task */
{
FILE *outFh = NULL;
struct genePredReader* gpr = genePredReaderFile(gpFile, gChrom);
struct genePred* gp;
int numRows = 0;

if (gOutput != NULL)
    outFh = mustOpen(gOutput, "w");

while ((numRows < gMaxRows) && ((gp = genePredReaderNext(gpr)) != NULL))
    {
    checkGenePred(gpFile, gp);
    if (outFh != NULL)
        genePredTabOut(gp, outFh);
    if (numRows == 0)
        writeInfo(gp);
    genePredFree(&gp);
    numRows++;
    }

carefulClose(&outFh);
genePredReaderFree(&gpr);
checkNumRows(gpFile, numRows);
}

struct hash* loadCds(char* cdsFile)
/* load a CDS file into a hash */
{
struct lineFile* lf = lineFileOpen(cdsFile, TRUE);
struct hash* cdsTbl = hashNew(0);
char *row[2];

while (lineFileNextRowTab(lf, row, 2))
    {
    struct genbankCds* cds;
    lmAllocVar(cdsTbl->lm, cds);
    if (!genbankCdsParse(row[1], cds))
        errAbort("invalid CDS for %s: %s", row[0], row[1]);
    hashAdd(cdsTbl, row[0], cds);
    }

lineFileClose(&lf);
return cdsTbl;
}

void fromPsl(char *pslFile, char* cdsFile)
/* Implements the fromPsl task */
{
struct psl* pslList = pslLoadAll(pslFile);
struct hash *cdsTbl = loadCds(cdsFile);
struct psl *psl;
FILE *outFh = NULL;
int numRows = 0;

if (gOutput != NULL)
    outFh = mustOpen(gOutput, "w");

for (psl = pslList; (psl != NULL) && (numRows < gMaxRows);
     psl = psl->next, numRows++)
    {
    struct genbankCds* cds = hashFindVal(cdsTbl, psl->qName);
    struct genePred* gp = genePredFromPsl2(psl, gOptFields, cds, 5);
    checkGenePred(pslFile, gp);
    if (outFh != NULL)
        genePredTabOut(gp, outFh);
    if (numRows == 0)
        writeInfo(gp);
    genePredFree(&gp);
    }
pslFreeList(&pslList);
carefulClose(&outFh);
hashFree(&cdsTbl);
checkNumRows(pslFile, numRows);
}

void groupConvertWarn(char *gxfFile, boolean isGtf, struct gffGroup *group)
/* issue a warning when a GxF group can't be converted to a genePred */
{
struct gffLine *line;

fprintf(stderr, "Warning: Can't convert %s to genePred: %s\n",
        (isGtf ? "GTF" : "GFF"), gxfFile);
for (line = group->lineList; line != NULL; line = line->next)
    {
    fprintf(stderr, "\t%s\t%s\t%s\t%d\t%d\t%g\t%c\t%c\t%s\t%s\t%s\t%d\n",
            line->seq, line->source, line->feature, line->start, line->end,
            line->score, line->strand, line->frame, line->group, line->geneId,
            line->exonId, line->exonNumber);
    }
}

void fromGxf(char *gxfFile, boolean isGtf)
/* Implements the fromGff/fromGtf tasks */
{
struct gffFile *gxf = gffRead(gxfFile);
struct gffGroup *group;
struct genePred *gp;
int numRows = 0;
FILE *outFh = NULL;

if (gOutput != NULL)
    outFh = mustOpen(gOutput, "w");

gffGroupLines(gxf);
for (group = gxf->groupList; group != NULL; group = group->next)
    {
    if (isGtf)
        gp = genePredFromGroupedGtf(gxf, group, group->name,
                                    gOptFields, gGxfCreateOpts);
    else
        gp = genePredFromGroupedGff(gxf, group,  group->name, gExonSelectWord,
                                    gOptFields, gGxfCreateOpts);
    if (gp == NULL)
        {
        if (!gIgnoreUnconverted)
            groupConvertWarn(gxfFile, isGtf, group);
        }
    else
        {
        checkGenePred(gxfFile, gp);
        if (outFh != NULL)
            genePredTabOut(gp, outFh);
        if (numRows == 0)
            writeInfo(gp);
        genePredFree(&gp);
        numRows++;
        }
    }
gffFileFree(&gxf);
carefulClose(&outFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *task;
optionInit(&argc, argv, options);
if (argc < 2)
    usage("must supply a task");
task = argv[1];
if (optionExists("scoreFld"))
    gOptFields |= genePredScoreFld;
if (optionExists("name2Fld"))
    gOptFields |= genePredName2Fld;    
if (optionExists("cdsStatFld"))
    gOptFields |= genePredCdsStatFld;    
if (optionExists("exonFramesFld"))
    gOptFields |= genePredExonFramesFld;
if (optionExists("genePredExt"))
    gOptFields |= genePredAllFlds;
gMaxRows = optionInt("maxRows", gMaxRows);
gMinRows = optionInt("minRows", gMinRows);
gNeedRows = optionInt("minRows", gNeedRows);
gWhere = optionVal("where", gWhere);
gChrom = optionVal("chrom", gChrom);
gOutput = optionVal("output", gOutput);
gInfo = optionVal("info", gInfo);
gNoCheck = optionExists("noCheck");
gExonSelectWord = optionVal("exonSelectWord", gExonSelectWord);
gIgnoreUnconverted = optionExists("ignoreUnconverted");
if (optionExists("impliedStopAfterCds"))
    gGxfCreateOpts |= genePredGxfImpliedStopAfterCds;
if (optionExists("withBin"))
    gCreateOpts |= genePredWithBin;

if (sameString(task, "readTable"))
    {
    if (argc != 4)
        usage("readTable task requires two arguments");
    readTableTask(argv[2], argv[3]);
    }
else if (sameString(task, "loadTable"))
    {
    if (argc != 5)
        usage("loadTable task requires three arguments");
    loadTableTask(argv[2], argv[3], argv[4]);
    }
else if (sameString(task, "readFile"))
    {
    if (argc != 3)
        usage("readFile task requires one argument");
    readFile(argv[2]);
    }
else if (sameString(task, "fromPsl"))
    {
    if (argc != 4)
        usage("fromPsl task requires two argument");
    fromPsl(argv[2], argv[3]);
    }
else if (sameString(task, "fromGff"))
    {
    if (argc != 3)
        usage("fromGff task requires one argument");
    fromGxf(argv[2], FALSE);
    }
else if (sameString(task, "fromGtf"))
    {
    if (argc != 3)
        usage("fromGtf task requires one argument");
    fromGxf(argv[2], TRUE);
    }
else 
    {
    usage("invalid task");
    }

return (errCount == 0) ? 0 : 1;
}
