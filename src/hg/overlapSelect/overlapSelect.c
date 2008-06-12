/* overlapSelect - select records based on overlap of chromosome ranges */

#include "common.h"
#include "selectTable.h"
#include "coordCols.h"
#include "chromAnn.h"
#include "dystring.h"
#include "options.h"

/* FIXME:
 * - would be nice to be able to specify ranges in the same manner
 *   as featureBits
 * - should keep header lines in files
 * - don't need to save if infile records if stats output
 */

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
    {"oppositeStrand", OPTION_BOOLEAN},
    {"excludeSelf", OPTION_BOOLEAN},
    {"idMatch", OPTION_BOOLEAN},
    {"dropped", OPTION_STRING},
    {"overlapThreshold", OPTION_FLOAT},
    {"overlapThresholdCeil", OPTION_FLOAT},
    {"overlapSimilarity", OPTION_FLOAT},
    {"overlapSimilarityCeil", OPTION_FLOAT},
    {"overlapBases", OPTION_INT},
    {"merge", OPTION_BOOLEAN},
    {"mergeOutput", OPTION_BOOLEAN},
    {"statsOutput", OPTION_BOOLEAN},
    {"statsOutputAll", OPTION_BOOLEAN},
    {"statsOutputBoth", OPTION_BOOLEAN},
    {"idOutput", OPTION_BOOLEAN},
    {"aggregate", OPTION_BOOLEAN},
    {NULL, 0}
};
/* incompatible with aggregate */
static char *aggIncompatible[] =
{
    "overlapSimilarity", "overlapSimilarityCeil", "overlapThresholdCeil", "overlapBases", "merge", "mergeOutput", "idMatch", NULL
};

/* file format constants */
enum recordFmt {
    UNKNOWN_FMT,
    PSL_FMT,
    PSLQ_FMT,
    CHAIN_FMT,
    CHAINQ_FMT,
    GENEPRED_FMT,
    BED_FMT,
    COORD_COLS_FMT
};

/* Options parsed from the command line */
enum recordFmt selectFmt = UNKNOWN_FMT;
struct coordCols selectCoordCols;
unsigned selectCaOpts = 0;

unsigned inFmt = UNKNOWN_FMT;
struct coordCols inCoordCols;
unsigned inCaOpts = 0;

unsigned selectOpts = 0;
boolean useAggregate = FALSE;
boolean nonOverlapping = FALSE;
boolean mergeOutput = FALSE;
boolean idOutput = FALSE;
boolean statsOutput = FALSE;
boolean outputAll = FALSE;
boolean outputBoth = FALSE;
struct overlapCriteria criteria = {0.0, 1.1, 0.0, 1.1, -1};

enum recordFmt parseFormatSpec(char *fmt)
/* parse a format specification */
{
if (sameString(fmt, "psl"))
    return PSL_FMT;
if (sameString(fmt, "pslq"))
    return PSLQ_FMT;
if (sameString(fmt, "chain"))
    return CHAIN_FMT;
if (sameString(fmt, "chainq"))
    return CHAINQ_FMT;
if (sameString(fmt, "genePred"))
    return GENEPRED_FMT;
if (sameString(fmt, "bed"))
    return BED_FMT;
errAbort("invalid file format: %s", fmt);
return UNKNOWN_FMT;
}

enum recordFmt getFileFormat(char *path)
/* determine the file format from the specified file extension */
{
char *filePath = path;
char filePathBuf[PATH_LEN];

if (endsWith(filePath, ".gz") || endsWith(filePath, ".bz2") || endsWith(filePath, ".Z"))
    {
    /* strip .gz/.bz2/.Z extension */
    splitPath(path, NULL, filePathBuf, NULL);
    filePath = filePathBuf;
    }
if (endsWith(filePath, ".psl"))
    return PSL_FMT;
if (endsWith(filePath, ".chain"))
    return CHAIN_FMT;
if (endsWith(filePath, ".genePred") || endsWith(filePath, ".gp"))
    return GENEPRED_FMT;
if (endsWith(filePath, ".bed"))
    return BED_FMT;
errAbort("can't determine file format of %s", filePath);
return UNKNOWN_FMT;
}

static struct  chromAnnReader *createChromAnnReader(char *fileName,
                                                    enum recordFmt fmt,
                                                    unsigned caOpts,
                                                    struct coordCols *cols)
/* construct a reader.  The coordCols spec is only used for tab files */
{
switch (fmt)
    {
    case PSL_FMT:
    case PSLQ_FMT:
        return chromAnnPslReaderNew(fileName, caOpts);
    case CHAIN_FMT:
    case CHAINQ_FMT:
        return chromAnnChainReaderNew(fileName, caOpts);
    case GENEPRED_FMT:
        return chromAnnGenePredReaderNew(fileName, caOpts);
    case BED_FMT:
        return chromAnnBedReaderNew(fileName, caOpts);
    case COORD_COLS_FMT:
        return chromAnnTabReaderNew(fileName, cols, caOpts);
    case UNKNOWN_FMT:
        break; 
    }
assert(FALSE);
return NULL;
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
    inCa->recWrite(inCa, outFh, '\t');
    selectCa->recWrite(selectCa, outFh, '\n');
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

/* format string for stats output */
static char *statsFmt = "%s\t%s\t%0.3g\t%0.3g\t%d\t%0.3g\n";

static void outputStats(struct chromAnn* inCa, FILE *outFh,
                        struct slRef *overlappingRecs)
/* output for the -statOutput option; pairs of inRec and overlap ids */
{
if (overlappingRecs == NULL)
    {
    // -statsOutputAll and nothing overlapping
    fprintf(outFh, statsFmt, getPrintId(inCa), "", 0.0, 0.0, 0, 0.0);
    }
struct slRef *selectCaRef;
for (selectCaRef = overlappingRecs; selectCaRef != NULL; selectCaRef = selectCaRef->next)
    {
    struct chromAnn *selectCa = selectCaRef->val;
    unsigned overBases = selectOverlapBases(inCa, selectCa);
    fprintf(outFh, statsFmt, getPrintId(inCa), getPrintId(selectCa),
            selectFracOverlap(inCa, overBases), selectFracOverlap(selectCa, overBases), overBases,
            selectFracSimilarity(inCa, selectCa, overBases));
    }
}

static void outputStatsSelNotUsed(FILE *outFh)
/* output stats for select chromAnns that were not used */
{
struct selectTableIter iter = selectTableFirst();
struct chromAnn *selCa;
while ((selCa = selectTableNext(&iter)) != NULL)
    {
    if (!selCa->used)
        fprintf(outFh, statsFmt, "", getPrintId(selCa), 0.0, 0.0, 0, 0.0);
    }
}

static void doItemOverlap(struct chromAnn* inCa, FILE *outFh, FILE *dropFh)
/* Do individual item overlap process of chromAnn object given the criteria,
 * and if so output */
{
struct slRef *overlappingRecs = NULL;
struct slRef **overlappingRecsPtr = NULL;  /* used to indicate if recs should be collected */
boolean overlaps = FALSE;
if (mergeOutput || idOutput || statsOutput)
    overlappingRecsPtr = &overlappingRecs;

overlaps = selectIsOverlapped(selectOpts, inCa, &criteria, overlappingRecsPtr);
if (((nonOverlapping) ? !overlaps : overlaps) || outputAll)
    {
    if (mergeOutput)
        outputMerge(inCa, outFh, overlappingRecs);
    else if (idOutput)
        outputIds(inCa, outFh, overlappingRecs);
    else if (statsOutput)
        outputStats(inCa, outFh, overlappingRecs);
    else
        inCa->recWrite(inCa, outFh, '\n');
    }
else if (dropFh != NULL)
    {
    if (idOutput)
        fprintf(dropFh, "%s\n", getPrintId(inCa));
    else
        inCa->recWrite(inCa, dropFh, '\n');
    }

slFreeList(&overlappingRecs);
}

static void doItemOverlaps(struct chromAnnReader* inCar, FILE *outFh, FILE *dropFh)
/* Do individual item overlap processings */
{
struct chromAnn *inCa;
while ((inCa = inCar->caRead(inCar)) != NULL)
    {
    doItemOverlap(inCa, outFh, dropFh);
    chromAnnFree(&inCa);
    }
}


static void doAggregateOverlap(struct chromAnn* inCa, FILE *outFh, FILE *dropFh)
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
        fprintf(outFh, "%s\n", getPrintId(inCa));
    else if (statsOutput)
        fprintf(outFh, "%s\t%0.3g\t%d\t%d\n", getPrintId(inCa),
                stats.inOverlap, stats.inOverBases, stats.inBases);
    else
        inCa->recWrite(inCa, outFh, '\n');
    }
else if (dropFh != NULL)
    {
    if (idOutput)
        fprintf(dropFh, "%s\n", getPrintId(inCa));
    else
        inCa->recWrite(inCa, dropFh, '\n');
    }
}

static void doAggregateOverlaps(struct chromAnnReader* inCar, FILE *outFh, FILE *dropFh)
/* Do aggreate overlap processing */
{
struct chromAnn *inCa;
while ((inCa = inCar->caRead(inCar)) != NULL)
    {
    doAggregateOverlap(inCa, outFh, dropFh);
    chromAnnFree(&inCa);
    }
}

void loadSelectTable(char *selectFile)
/* load the select table from a file */
{
struct chromAnnReader *selCar = createChromAnnReader(selectFile, selectFmt, selectCaOpts, &selectCoordCols);
selectTableAddRecords(selCar);
selCar->carFree(&selCar);
}

void overlapSelect(char *selectFile, char *inFile, char *outFile, char *dropFile)
/* select records based on overlap of chromosome ranges */
{
struct chromAnnReader *inCar
    = createChromAnnReader(inFile, inFmt, inCaOpts, &inCoordCols);
loadSelectTable(selectFile);
FILE *outFh = mustOpen(outFile, "w");
FILE *dropFh = NULL;
if (dropFile != NULL)
    dropFh = mustOpen(dropFile, "w");
if (idOutput)
    {
    if (useAggregate)
        fputs("#inId\n", outFh);
    else
        fputs("#inId\t" "selectId\n", outFh);
    }
if (statsOutput)
    {
    if (useAggregate)
        fputs("#inId\t" "inOverlap\t" "inOverBases\t" "inBases\n", outFh);
    else
        fputs("#inId\t" "selectId\t" "inOverlap\t" "selectOverlap\t" "overBases\t" "similarity\n", outFh);
    }

if (useAggregate)
    doAggregateOverlaps(inCar, outFh, dropFh);
else
    doItemOverlaps(inCar, outFh, dropFh);

inCar->carFree(&inCar);
if (statsOutput && outputBoth)
    outputStatsSelNotUsed(outFh);

carefulClose(&outFh);
carefulClose(&dropFh);
/* enable for memory analysis */
#if 1
selectTableFree();
#endif
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

/* select file options */
if (optionExists("selectFmt") && optionExists("selectCoordCols"))
    errAbort("can't specify both -selectFmt and -selectCoordCols");

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
    selectCaOpts |= chromAnnCds;
if (optionExists("selectRange"))
    selectCaOpts |= chromAnnRange;
if ((selectFmt == PSLQ_FMT) || (selectFmt == CHAINQ_FMT))
    selectCaOpts |= chromAnnUseQSide;

/* in file options */
if (optionExists("inFmt") && optionExists("inCoordCols"))
    errAbort("can't specify both -inFmt and -inCoordCols");
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

inCaOpts = chromAnnSaveLines; // need lines for output
if (optionExists("inCds"))
    inCaOpts |= chromAnnCds;
if (optionExists("inRange"))
    inCaOpts |= chromAnnRange;
if ((inFmt == PSLQ_FMT) || (inFmt == CHAINQ_FMT))
    inCaOpts |= chromAnnUseQSide;

/* select options */
useAggregate = optionExists("aggregate");
nonOverlapping = optionExists("nonOverlapping");
if (optionExists("strand") && optionExists("oppositeStrand"))
    errAbort("can only specify one of -strand and -oppositeStrand");
if (optionExists("strand"))
    selectOpts |= selStrand;
if (optionExists("oppositeStrand"))
    selectOpts |= selOppositeStrand;
if (optionExists("excludeSelf") && (optionExists("idMatch")))
    errAbort("can't specify both -excludeSelf and -idMatch");
if (optionExists("excludeSelf"))
    selectOpts |= selExcludeSelf;
if (optionExists("idMatch"))
    selectOpts |= selIdMatch;

criteria.threshold = optionFloat("overlapThreshold", 0.0);
criteria.thresholdCeil = optionFloat("overlapThresholdCeil", 1.1);
criteria.similarity = optionFloat("overlapSimilarity", 0.0);
criteria.similarityCeil = optionFloat("overlapSimilarityCeil", 1.1);
criteria.bases = optionInt("overlapBases", -1);

/* output options */
mergeOutput = optionExists("mergeOutput");
idOutput = optionExists("idOutput");
statsOutput = optionExists("statsOutput") || optionExists("statsOutputAll") || optionExists("statsOutputBoth");
if ((mergeOutput + idOutput + statsOutput) > 1)
    errAbort("can only specify one of -mergeOutput, -idOutput, -statsOutput, -statsOutputAll, or -statsOutputBoth");
outputAll = optionExists("statsOutputAll");
outputBoth = optionExists("statsOutputBoth");
if (outputBoth)
    outputAll = TRUE;
if (mergeOutput)
    {
    if (nonOverlapping)
        errAbort("can't use -mergeOutput with -nonOverlapping");
    if (useAggregate)
        errAbort("can't use -mergeOutput with -aggregate");
    if ((selectFmt == CHAIN_FMT) || (selectFmt == CHAINQ_FMT)
        || (inFmt == CHAIN_FMT) || (inFmt == CHAINQ_FMT))
    if (useAggregate)
        errAbort("can't use -mergeOutput with chains");
    selectCaOpts |= chromAnnSaveLines;
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
