/* overlapSelect - select records based on overlap of chromosome ranges */

#include "common.h"
#include "rowReader.h"
#include "selectTable.h"
#include "psl.h"
#include "bed.h"
#include "coordCols.h"
#include "chromAnn.h"
#include "genePred.h"
#include "dystring.h"
#include "options.h"

/* FIXME: would be nice to be able to specify ranges in the same manner
 * as featureBits */

static struct optionSpec optionSpecs[] = {
    {"selectFmt", OPTION_STRING},
    {"selectCoordCols", OPTION_STRING},
    {"selectCds", OPTION_BOOLEAN},
    {"selectRange", OPTION_BOOLEAN},
    {"inFmt", OPTION_STRING},
    {"inCoordCols", OPTION_STRING},
    {"inCds", OPTION_BOOLEAN},
    {"inRange", OPTION_BOOLEAN},
    {"nonOverlapping", OPTION_BOOLEAN},
    {"strand", OPTION_BOOLEAN},
    {"excludeSelf", OPTION_BOOLEAN},
    {"idMatch", OPTION_BOOLEAN},
    {"dropped", OPTION_STRING},
    {"overlapThreshold", OPTION_FLOAT},
    {"overlapSimilarity", OPTION_FLOAT},
    {"overlapBases", OPTION_INT},
    {"merge", OPTION_BOOLEAN},
    {"mergeOutput", OPTION_BOOLEAN},
    {"statsOutput", OPTION_BOOLEAN},
    {"statsOutputAll", OPTION_BOOLEAN},
    {"idOutput", OPTION_BOOLEAN},
    {"aggregate", OPTION_BOOLEAN},
    {NULL, 0}
};
/* incompatible with aggregate */
static char *aggIncompatible[] =
{
    "overlapSimilarity", "overlapBases", "merge", "mergeOutput", "idMatch", NULL
};

/* file format constants */
#define UNKNOWN_FMT    0
#define PSL_FMT        1
#define GENEPRED_FMT   2
#define BED_FMT        3
#define COORD_COLS_FMT 4

/* Options parsed from the command line */
unsigned selectFmt = UNKNOWN_FMT;
struct coordCols selectCoordCols;
unsigned selectOpts = 0;
unsigned inCaOpts = 0;
unsigned inFmt = UNKNOWN_FMT;
struct coordCols inCoordCols;
boolean useAggregate = FALSE;
boolean nonOverlapping = FALSE;
boolean mergeOutput = FALSE;
boolean idOutput = FALSE;
boolean statsOutput = FALSE;
boolean outputAll = FALSE;
struct overlapCriteria criteria = {0.0, 0.0, -1};

struct ioFiles
/* object containing input files */
{
    struct rowReader *inRr;
    FILE* outFh;
    FILE* dropFh;
};

unsigned parseFormatSpec(char *fmt)
/* parse a format specification */
{
if (sameString(fmt, "psl"))
    return PSL_FMT;
if (sameString(fmt, "genePred"))
    return GENEPRED_FMT;
if (sameString(fmt, "bed"))
    return BED_FMT;
errAbort("invalid file format: %s", fmt);
return UNKNOWN_FMT;
}

unsigned getFileFormat(char *path)
/* determine the file format from the specified file extension */
{
char *filePath = path;
char filePathBuf[PATH_LEN];

if (endsWith(filePath, ".gz"))
    {
    /* strip .gz extension */
    splitPath(path, NULL, filePathBuf, NULL);
    filePath = filePathBuf;
    }

if (endsWith(filePath, ".psl"))
    return PSL_FMT;
if (endsWith(filePath, ".genePred") || endsWith(filePath, ".gp"))
    return GENEPRED_FMT;
if (endsWith(filePath, ".bed"))
    return BED_FMT;
errAbort("can't determine file format of %s", filePath);
return UNKNOWN_FMT;
}

static char *getPrintId(struct chromAnn* ca)
/* get id for output, or <unknown> if not known */
{
return (ca->name == NULL) ? "<unknown>" : ca->name;
}

static void outputMerge(struct chromAnn* inCa, FILE *outFh,
                        struct slRef *overlappingRecs)
/* output for the -mergeOutput option; pairs of inRec and overlap */
{
struct slRef *selectCaRef;
for (selectCaRef = overlappingRecs; selectCaRef != NULL; selectCaRef = selectCaRef->next)
    {
    struct chromAnn *selectCa = selectCaRef->val;
    chromAnnWrite(inCa, outFh, '\t');
    chromAnnWrite(selectCa, outFh, '\n');
    }
}

static void outputIds(struct chromAnn* inCa, FILE *outFh,
                      struct slRef *overlappingRecs)
/* output for the -idOutput option; pairs of inRec and overlap ids */
{
struct slRef *selectCaRef;
for (selectCaRef = overlappingRecs; selectCaRef != NULL; selectCaRef = selectCaRef->next)
    {
    struct chromAnn *selectCa = selectCaRef->val;
    fprintf(outFh, "%s\t%s\n", getPrintId(inCa), getPrintId(selectCa));
    }
}

static void outputStats(struct chromAnn* inCa, FILE *outFh,
                        struct slRef *overlappingRecs)
/* output for the -statOutput option; pairs of inRec and overlap ids */
{
struct slRef *selectCaRef;
for (selectCaRef = overlappingRecs; selectCaRef != NULL; selectCaRef = selectCaRef->next)
    {
    struct chromAnn *selectCa = selectCaRef->val;
    unsigned overBases = selectOverlapBases(inCa, selectCa);
    fprintf(outFh, "%s\t%s\t%0.3g\t%0.3g\t%d\n", getPrintId(inCa), getPrintId(selectCa),
            selectFracOverlap(inCa, overBases), selectFracOverlap(selectCa, overBases), overBases);
    }
}

static void doItemOverlap(struct chromAnn* inCa, struct ioFiles *ioFiles)
/* Do individual item overlap process of chromAnn object given the criteria,
 * and if so output */
{
struct slRef *overlappingRecs = NULL;
struct slRef **overlappingRecsPtr = NULL;  /* used to indicate if recs should be collected */
boolean overlaps = FALSE;
if (mergeOutput || idOutput || statsOutput)
    overlappingRecsPtr = &overlappingRecs;

overlaps = selectIsOverlapped(selectOpts, inCa, &criteria, overlappingRecsPtr);
if ((nonOverlapping) ? !overlaps : overlaps)
    {
    if (mergeOutput)
        outputMerge(inCa, ioFiles->outFh, overlappingRecs);
    else if (idOutput)
        outputIds(inCa, ioFiles->outFh, overlappingRecs);
    else if (statsOutput)
        outputStats(inCa, ioFiles->outFh, overlappingRecs);
    else
        chromAnnWrite(inCa, ioFiles->outFh, '\n');
    }
else if (ioFiles->dropFh != NULL)
    {
    if (idOutput)
        fprintf(ioFiles->dropFh, "%s\n", getPrintId(inCa));
    else
        chromAnnWrite(inCa, ioFiles->dropFh, '\n');
    }

slFreeList(&overlappingRecs);
}

static void doAggregateOverlap(struct chromAnn* inCa, struct ioFiles *ioFiles)
/* Do aggreate overlap process of chromAnn object given the criteria,
 * and if so output */
{
struct overlapAggStats stats = selectAggregateOverlap(selectOpts, inCa);
boolean overlaps;
if (criteria.threshold <= 0.0)
    overlaps = (stats.inOverlap > 0.0); /* any overlap */
else
    overlaps = (stats.inOverlap >= criteria.threshold);
if (((nonOverlapping) ? !overlaps : overlaps) || outputAll)
    {
    if (idOutput)
        fprintf(ioFiles->outFh, "%s\n", getPrintId(inCa));
    else if (statsOutput)
        fprintf(ioFiles->outFh, "%s\t%0.3g\t%d\t%d\n", getPrintId(inCa),
                stats.inOverlap, stats.inOverBases, stats.inBases);
    else
        chromAnnWrite(inCa, ioFiles->outFh, '\n');
    }
else if (ioFiles->dropFh != NULL)
    {
    if (idOutput)
        fprintf(ioFiles->dropFh, "%s\n", getPrintId(inCa));
    else
        chromAnnWrite(inCa, ioFiles->dropFh, '\n');
    }
}

static void doOverlap(struct chromAnn* inCa, struct ioFiles *ioFiles)
/* Check if a chromAnn object is overlapped given the criteria, and if so
 * output */
{
if (useAggregate)
    doAggregateOverlap(inCa, ioFiles);
else
    doItemOverlap(inCa, ioFiles);
}

void pslSelect(struct ioFiles *ioFiles)
/* copy psl records that matches the selection criteria */
{
struct chromAnn* inCa;
while (rowReaderNext(ioFiles->inRr))
    {
    inCa = chromAnnFromPsl(inCaOpts, ioFiles->inRr);
    doOverlap(inCa, ioFiles);
    chromAnnFree(&inCa);
    }
}

void genePredSelect(struct ioFiles *ioFiles)
/* copy genePred records that matches the selection criteria */
{
struct chromAnn* inCa;
while (rowReaderNext(ioFiles->inRr))
    {
    inCa = chromAnnFromGenePred(inCaOpts, ioFiles->inRr);
    doOverlap(inCa, ioFiles);
    chromAnnFree(&inCa);
    }
}

void bedSelect(struct ioFiles *ioFiles)
/* copy bed records that matches the selection criteria */
{
struct chromAnn* inCa;
while (rowReaderNext(ioFiles->inRr))
    {
    inCa = chromAnnFromBed(inCaOpts, ioFiles->inRr);
    doOverlap(inCa, ioFiles);
    chromAnnFree(&inCa);
    }
}

void coordColsSelect(struct ioFiles *ioFiles)
/* copy records that matches the selection criteria when the coordinates are
 * specified by start column. */
{
/*FIXME: should keep header lines in files */
struct chromAnn* inCa;
while (rowReaderNext(ioFiles->inRr))
    {
    inCa = chromAnnFromCoordCols(inCaOpts, &inCoordCols, ioFiles->inRr);
    doOverlap(inCa, ioFiles);
    chromAnnFree(&inCa);
    }
}

void loadSelectTable(char *selectFile)
/* load the select table from a file */
{
struct rowReader *rr = rowReaderOpen(selectFile, (selectFmt == PSL_FMT));
switch (selectFmt)
    {
    case PSL_FMT:
        selectAddPsls(selectOpts, rr);
        break;
    case GENEPRED_FMT:
        selectAddGenePreds(selectOpts, rr);
        break;
    case BED_FMT:
        selectAddBeds(selectOpts, rr);
        break;
    case COORD_COLS_FMT:
        selectAddCoordCols(selectOpts, &selectCoordCols, rr);
        break;
    }
rowReaderFree(&rr);
}

void overlapSelect(char *selectFile, char *inFile, char *outFile, char *dropFile)
/* select records based on overlap of chromosome ranges */
{
struct ioFiles ioFiles;
ZeroVar(&ioFiles);
ioFiles.inRr = rowReaderOpen(inFile, (inFmt == PSL_FMT));

loadSelectTable(selectFile);
ioFiles.outFh = mustOpen(outFile, "w");
if (dropFile != NULL)
    ioFiles.dropFh = mustOpen(dropFile, "w");
if (idOutput)
    {
    if (useAggregate)
        fputs("#inId\n", ioFiles.outFh);
    else
        fputs("#inId\t" "selectId\n", ioFiles.outFh);
    }
if (statsOutput)
    {
    if (useAggregate)
        fputs("#inId\t" "inOverlap\t" "inOverBases\t" "inBases\n", ioFiles.outFh);
    else
        fputs("#inId\t" "selectId\t" "inOverlap\t" "selectOverlap\t" "overBases\n", ioFiles.outFh);
    }

switch (inFmt)
    {
    case PSL_FMT:
        pslSelect(&ioFiles);
        break;
    case GENEPRED_FMT:
        genePredSelect(&ioFiles);
        break;
    case BED_FMT:
        bedSelect(&ioFiles);
        break;
    case COORD_COLS_FMT:
        coordColsSelect(&ioFiles);
        break;
    }
rowReaderFree(&ioFiles.inRr);
carefulClose(&ioFiles.outFh);
carefulClose(&ioFiles.dropFh);
}

void usage(char *msg)
/* usage message and abort */
{
static char *usageMsg =
#include "usage.msg"
    ;
errAbort("%s:  %s", msg, usageMsg);
}

/* entry */
int main(int argc, char** argv)
{
char *selectFile, *inFile, *outFile, *dropFile;
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("wrong # args");
selectFile = argv[1];
inFile = argv[2];
outFile = argv[3];

/* file format options */
if (optionExists("selectFmt") && optionExists("selectCoordCols"))
    errAbort("can't specify both -selectFmt and -selectCoordCols");
if (optionExists("intFmt") && optionExists("intCoordCols"))
    errAbort("can't specify both -intFmt and -intCoordCols");


if (optionExists("selectFmt"))
    selectFmt = parseFormatSpec(optionVal("selectFmt", NULL));
else if (optionExists("selectCoordCols"))
    {
    selectCoordCols = coordColsParseSpec("selectCoordCols",
                                         optionVal("selectCoordCols", NULL));
    selectFmt = COORD_COLS_FMT;
    }
else
    selectFmt = getFileFormat(selectFile);

if (optionExists("selectCds"))
    selectOpts |= selSelectCds;
if (optionExists("selectRange"))
    selectOpts |= selSelectRange;

if (optionExists("inFmt"))
    inFmt = parseFormatSpec(optionVal("inFmt", NULL));
else if (optionExists("inCoordCols"))
    {
    inCoordCols = coordColsParseSpec("inCoordCols",
                                     optionVal("inCoordCols", NULL));
    inFmt = COORD_COLS_FMT;
    }
else
    inFmt = getFileFormat(inFile);
inCaOpts = chromAnnSaveLines; /* always need lines for output */
if (optionExists("inCds"))
    inCaOpts |= chromAnnCds;
if (optionExists("inRange"))
    inCaOpts |= chromAnnRange;

/* select options */
useAggregate = optionExists("aggregate");
nonOverlapping = optionExists("nonOverlapping");
if (optionExists("strand"))
    selectOpts |= selUseStrand;
if (optionExists("excludeSelf") && (optionExists("idMatch")))
    errAbort("can't specify both -excludeSelf and -idMatch");
if (optionExists("excludeSelf"))
    selectOpts |= selExcludeSelf;
if (optionExists("idMatch"))
    selectOpts |= selIdMatch;

criteria.threshold = optionFloat("overlapThreshold", 0.0);
criteria.similarity = optionFloat("overlapSimilarity", 0.0);
criteria.bases = optionInt("overlapBases", -1);
if ((criteria.threshold != 0.0) && (criteria.similarity != 0.0))
    errAbort("can't specify both -overlapThreshold and -overlapSimilarity");

/* output options */
mergeOutput = optionExists("mergeOutput");
idOutput = optionExists("idOutput");
statsOutput = optionExists("statsOutput") || optionExists("statsOutputAll");
if ((mergeOutput + idOutput + statsOutput) > 1)
    errAbort("can only specify one of -mergeOutput, -idOutput, -statsOutput, or -statsOutputAll");
if (optionExists("statsOutputAll"))
    {
    if (!useAggregate)
        errAbort("-statsOutputAll only works with -aggregate");
    outputAll = TRUE;
    }
if (mergeOutput)
    {
    if (nonOverlapping)
        errAbort("can't use -mergeOutput with -nonOverlapping");
    if (useAggregate)
        errAbort("can't use -mergeOutput with -aggregate");
    selectOpts |= selSaveLines;
    }
dropFile = optionVal("dropped", NULL);

/* check for options incompatible with aggregate mode */
if (useAggregate)
    {
    int i;
    for (i = 0; aggIncompatible[i] != NULL; i++)
        {
        if (optionExists(aggIncompatible[i]))
            errAbort("-%s is not allowed -aggregate", aggIncompatible[i]);
        }
    }

overlapSelect(selectFile, inFile, outFile, dropFile);
return 0;
}
