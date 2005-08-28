/* overlapSelect - select records based on overlap of chromosome ranges */

#include "common.h"
#include "linefile.h"
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
    {"merge", OPTION_BOOLEAN},
    {"mergeOutput", OPTION_BOOLEAN},
    {"statsOutput", OPTION_BOOLEAN},
    {"idOutput", OPTION_BOOLEAN},
    {"aggregate", OPTION_BOOLEAN},
    {NULL, 0}
};
/* incompatible with aggregate */
static char *aggIncompatible[] =
{
    "overlapSimilarity", "merge", "mergeOutput", "idMatch", NULL
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
float overlapThreshold = 0.0;
float overlapSimilarity = 0.0;


struct ioFiles
/* object containing input files */
{
    struct lineFile *inLf;
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

static void outputMerge(struct chromAnn* inCa, struct ioFiles *ioFiles,
                        struct slRef *overlappingRecs)
/* output for the -mergeOutput option; pairs of inRec and overlap */
{
struct slRef *selectCaRef;
for (selectCaRef = overlappingRecs; selectCaRef != NULL; selectCaRef = selectCaRef->next)
    {
    struct chromAnn *selectCa = selectCaRef->val;
    fprintf(ioFiles->outFh, "%s\t%s\n", inCa->recLine, selectCa->recLine);
    }
}

static void outputIds(struct chromAnn* inCa, struct ioFiles *ioFiles,
                      struct slRef *overlappingRecs)
/* output for the -idOutput option; pairs of inRec and overlap ids */
{
struct slRef *selectCaRef;
for (selectCaRef = overlappingRecs; selectCaRef != NULL; selectCaRef = selectCaRef->next)
    {
    struct chromAnn *selectCa = selectCaRef->val;
    fprintf(ioFiles->outFh, "%s\t%s\n", getPrintId(inCa), getPrintId(selectCa));
    }
}

static void outputStats(struct chromAnn* inCa, struct ioFiles *ioFiles,
                        struct slRef *overlappingRecs)
/* output for the -statOutput option; pairs of inRec and overlap ids */
{
struct slRef *selectCaRef;
for (selectCaRef = overlappingRecs; selectCaRef != NULL; selectCaRef = selectCaRef->next)
    {
    struct chromAnn *selectCa = selectCaRef->val;
    fprintf(ioFiles->outFh, "%s\t%s\t%0.3g\t%0.3g\n", getPrintId(inCa), getPrintId(selectCa),
            selectFracOverlap(inCa, selectCa), selectFracOverlap(selectCa, inCa));
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

overlaps = selectIsOverlapped(selectOpts, inCa, overlapThreshold, overlapSimilarity,
                              overlappingRecsPtr);
if ((nonOverlapping) ? !overlaps : overlaps)
    {
    if (mergeOutput)
        outputMerge(inCa, ioFiles, overlappingRecs);
    else if (idOutput)
        outputIds(inCa, ioFiles, overlappingRecs);
    else if (statsOutput)
        outputStats(inCa, ioFiles, overlappingRecs);
    else
        fprintf(ioFiles->outFh, "%s\n", inCa->recLine);
    }
else if (ioFiles->dropFh != NULL)
    {
    if (idOutput)
        fprintf(ioFiles->dropFh, "%s\n", getPrintId(inCa));
    else
        fprintf(ioFiles->dropFh, "%s\n", inCa->recLine);
    }

slFreeList(&overlappingRecs);
}

static void doAggregateOverlap(struct chromAnn* inCa, struct ioFiles *ioFiles)
/* Do aggreate overlap process of chromAnn object given the criteria,
 * and if so output */
{
float overlap = selectAggregateOverlap(selectOpts, inCa);
boolean overlaps;
if (overlapThreshold <= 0.0)
    overlaps = (overlap > 0.0); /* any overlap */
else
    overlaps = (overlap >= overlapThreshold);
if ((nonOverlapping) ? !overlaps : overlaps)
    {
    if (idOutput)
        fprintf(ioFiles->outFh, "%s\n", getPrintId(inCa));
    else if (statsOutput)
        fprintf(ioFiles->outFh, "%s\t%0.3g\n", getPrintId(inCa), overlap);
    else
        fprintf(ioFiles->outFh, "%s\n", inCa->recLine);
    }
else if (ioFiles->dropFh != NULL)
    {
    if (idOutput)
        fprintf(ioFiles->dropFh, "%s\n", getPrintId(inCa));
    else
        fprintf(ioFiles->dropFh, "%s\n", inCa->recLine);
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
char *line;
struct chromAnn* inCa;
while (lineFileNextReal(ioFiles->inLf, &line))
    {
    inCa = chromAnnFromPsl(inCaOpts, ioFiles->inLf, line);
    doOverlap(inCa, ioFiles);
    chromAnnFree(&inCa);
    }
}

void genePredSelect(struct ioFiles *ioFiles)
/* copy genePred records that matches the selection criteria */
{
char *line;
struct chromAnn* inCa;
while (lineFileNextReal(ioFiles->inLf, &line))
    {
    inCa = chromAnnFromGenePred(inCaOpts, ioFiles->inLf, line);
    doOverlap(inCa, ioFiles);
    chromAnnFree(&inCa);
    }
}

void bedSelect(struct ioFiles *ioFiles)
/* copy bed records that matches the selection criteria */
{
char *line;
struct chromAnn* inCa;
while (lineFileNextReal(ioFiles->inLf, &line))
    {
    inCa = chromAnnFromBed(inCaOpts, ioFiles->inLf, line);
    doOverlap(inCa, ioFiles);
    chromAnnFree(&inCa);
    }
}

void coordColsSelect(struct ioFiles *ioFiles)
/* copy records that matches the selection criteria when the coordinates are
 * specified by start column. */
{
/*FIXME: should keep header lines in files */
char *line;
struct chromAnn* inCa;
while (lineFileNextReal(ioFiles->inLf, &line))
    {
    inCa = chromAnnFromCoordCols(inCaOpts, ioFiles->inLf, line, &inCoordCols);
    doOverlap(inCa, ioFiles);
    chromAnnFree(&inCa);
    }
}

void loadSelectTable(char *selectFile)
/* load the select table from a file */
{
struct lineFile *lf = (selectFmt == PSL_FMT)
    ? pslFileOpen(selectFile)
    : lineFileOpen(selectFile, TRUE);
switch (selectFmt)
    {
    case PSL_FMT:
        selectAddPsls(selectOpts, lf);
        break;
    case GENEPRED_FMT:
        selectAddGenePreds(selectOpts, lf);
        break;
    case BED_FMT:
        selectAddBeds(selectOpts, lf);
        break;
    case COORD_COLS_FMT:
        selectAddCoordCols(selectOpts, lf, &selectCoordCols);
        break;
    }
lineFileClose(&lf);
}

void overlapSelect(char *selectFile, char *inFile, char *outFile, char *dropFile)
/* select records based on overlap of chromosome ranges */
{
struct ioFiles ioFiles;
ZeroVar(&ioFiles);
ioFiles.inLf = (inFmt == PSL_FMT) ? pslFileOpen(inFile) : lineFileOpen(inFile, TRUE);

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
    fputs("#inId\t" "selectId\t" "inOverlap\t" "selectOverlap\n", ioFiles.outFh);

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
lineFileClose(&ioFiles.inLf);
carefulClose(&ioFiles.outFh);
carefulClose(&ioFiles.dropFh);
}

void usage(char *msg)
/* usage message and abort */
{
errAbort("%s:\n"
         "overlapSelect [options] selectFile inFile outFile\n"
         "\n"
         "Select records based on overlaping chromosome ranges.\n"
         "The ranges are specified in the selectFile, with each block\n"
         "specifying a range.  Records are copied from the inFile to outFile\n"
         "based on the selection criteria.  Selection is based on blocks or\n"
         "exons rather than entire range.\n"
         "\n"
         "Options:\n"
         "  -selectFmt=fmt - specify selectFile format:\n"
         "          psl - PSL format (default for *.psl files).\n"
         "          genePred - gepePred format (default for *.gp or\n"
         "                     *.genePred files).\n"
         "          bed - BED format (default for *.bed files).\n"
         "                If BED doesn't have blocks, the bed range is used.\n" 
         "  -selectCoordCols=spec - Select file is tab-separate with coordinates\n"
         "          as described by spec, which is one of:\n"
         "            o chromCol - chrom in this column followed by start and end.\n"
         "            o chromCol,startCol,endCol - chrom, start, and end in specified\n"
         "              columns.\n"
         "            o chromCol,startCol,endCol,strandCol - chrom, start,, end, and\n"
         "              strand in specified columns.\n"
         "  -selectCds - Use only CDS in the select file\n"
         "  -selectRange - Use entire range instead of blocks from records in\n"
         "          the select file.\n"
         "  -inFmt=fmt - specify inFile format, same values as -selectFmt.\n"
         "  -inCoordCols=spec - in file is tab-separate with coordinates specified by\n"
         "          spec, in format described above.\n"
         "  -inCds - Use only CDS in the in file\n"
         "  -inRange - Use entire range instead of blocks of records in\n"
         "          the in file.\n"
         "  -nonOverlapping - select non-overlaping instead of overlaping records\n"
         "  -strand - must be on the same strand to be considered overlaping\n"
         "  -excludeSelf - don't compare records with the same coordinates and name.\n"
         "  -idMatch - only select overlapping records if they have the same id\n"
         "  -aggregate - instead of computing overlap bases on individual select entries, \n"
         "      compute it based on the total number of inFile bases overlap by select file\n"
         "      records. -overlapSimilarity and -mergeOutput will not work with\n"
         "      this option.\n"
         "  -overlapThreshold=0.0 - minimun fraction of an inFile block that\n"
         "      must be overlapped by a single select record to be considered overlapping.\n"
         "      Note that this is only coverage by a single select record, not total coverage.\n"
         "  -overlapSimilarity=0.0 - minimun fraction of inFile and select records that\n"
         "      Note that this is only coverage by a single select record and this is;\n"
         "      bidirectional inFile and selectFile must overlap by this amount.  A value of 1.0\n"
         "      will select identical records (or CDS if both CDS options are specified.\n"
         "      Not currently supported with -aggregate.\n"
         "  -statsOutput - output overlap statistics instead of selected records. \n"
         "      If no overlap criteria is specified, all overlapping entries are reported,\n"
         "      Otherwise only the pairs passing the citeria are reported. This results\n"
         "      in a tab-seperated file with the columns:\n"
         "         inId selectId inOverlap selectOverlap \n"
         "      Where inOverlap is the fraction of the inFile record overlapped by the select file\n"
         "      record and selectOverlap is the fraction of the select record overlap by the in file\n"
         "      record.  With -aggregate, output is:\n"
         "         inId inOverlap\n"
         "  -mergeOutput - output file with be a merge of the input file with the\n"
         "      selectFile records that selected it.  The format is\n"
         "         inRec<tab>selectRec.\n"
         "      if multiple select records hit, inRec is repeated.\n"
         "      This will increase the memory required\n"
         "      Not currently supported with -aggregate.\n"
         "  -idOutput - output a table seprate file of pairs of\n"
         "        inId selectId\n"
         "      with -aggregate, omly a single column of inId is written\n"
         "  -dropped=file  - output rows that were dropped to this file.\n"
         "  -verbose=n - verbose > 1 prints some details\n",
         msg);
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

overlapThreshold = optionFloat("overlapThreshold", 0.0);
overlapSimilarity = optionFloat("overlapSimilarity", 0.0);
if ((overlapThreshold != 0.0) && (overlapSimilarity != 0.0))
    errAbort("can't specify both -overlapThreshold and -overlapSimilarity");

/* output options */
if (optionExists("merge")) /* FIXME: this is tmp */
    errAbort("please use -mergeOutput instead of -merge; trying to keep option names sane");
mergeOutput = optionExists("mergeOutput");
idOutput = optionExists("idOutput");
statsOutput = optionExists("statsOutput");
if ((mergeOutput + idOutput + statsOutput) > 1)
    errAbort("can only specify one of -mergeOutput, -idOutput, or -statsOutput");
if (mergeOutput)
    {
    if (nonOverlapping)
        errAbort("can't use -mergeOutput with -nonOverlapping");
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
