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
    {"inFmt", OPTION_STRING},
    {"inCoordCols", OPTION_STRING},
    {"inCds", OPTION_BOOLEAN},
    {"nonOverlapping", OPTION_BOOLEAN},
    {"strand", OPTION_BOOLEAN},
    {"excludeSelf", OPTION_BOOLEAN},
    {"dropped", OPTION_STRING},
    {"merge", OPTION_BOOLEAN},
    {"overlapThreshold", OPTION_FLOAT},
    {"idOutput", OPTION_BOOLEAN},
    {NULL, 0}
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
boolean doMerge = FALSE;
boolean nonOverlapping = FALSE;
boolean idOutput = FALSE;
float overlapThreshold = 0.0;

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
if (endsWith(path, ".psl"))
    return PSL_FMT;
if (endsWith(path, ".genePred") || endsWith(path, ".gp"))
    return GENEPRED_FMT;
if (endsWith(path, ".bed"))
    return BED_FMT;
errAbort("can't determine file format of %s", path);
return UNKNOWN_FMT;
}

static void writeId(struct chromAnn* ca, FILE* out)
/* output and id, or <unknown> if not known */
{
fputs(((ca->name == NULL) ? "<unknown>" : ca->name), out);
}

static void outputMerge(struct chromAnn* inCa, struct ioFiles *ioFiles,
                        struct slRef *overlappedRecLines)
/* output for the -merge option; pairs of inRec and overlap */
{
struct slRef *selectCaRef;
for (selectCaRef = overlappedRecLines; selectCaRef != NULL; selectCaRef = selectCaRef->next)
    {
    struct chromAnn *selectCa = selectCaRef->val;
    fputs(inCa->recLine, ioFiles->outFh);
    fputc('\t', ioFiles->outFh);
    fputs(selectCa->recLine, ioFiles->outFh);
    fputc('\n', ioFiles->outFh);
    }
}

static void outputIds(struct chromAnn* inCa, struct ioFiles *ioFiles,
                        struct slRef *overlappedRecLines)
/* output for the -idOutput option; pairs of inRec and overlap ids */
{
struct slRef *selectCaRef;
for (selectCaRef = overlappedRecLines; selectCaRef != NULL; selectCaRef = selectCaRef->next)
    {
    struct chromAnn *selectCa = selectCaRef->val;
    writeId(inCa, ioFiles->outFh);
    fputc('\t', ioFiles->outFh);
    writeId(selectCa, ioFiles->outFh);
    fputc('\n', ioFiles->outFh);
    }
}

static void doOverlap(struct chromAnn* inCa, struct ioFiles *ioFiles)
/* Check if a chromAnn object is overlapped given the criteria, and if so
 * output */
{
struct slRef *overlappedRecLines = NULL, *selectCaRef;
boolean overlaps = FALSE;

overlaps = selectIsOverlapped(selectOpts, inCa, overlapThreshold,
                              ((doMerge || idOutput) ? &overlappedRecLines : NULL));
if ((nonOverlapping) ? !overlaps : overlaps)
    {
    if (doMerge)
        outputMerge(inCa, ioFiles, overlappedRecLines);
    else if (idOutput)
        outputIds(inCa, ioFiles, overlappedRecLines);
    else
        {
        fputs(inCa->recLine, ioFiles->outFh);
        fputc('\n', ioFiles->outFh);
        }
    }
else if (ioFiles->dropFh != NULL)
    {
    if (idOutput)
        writeId(inCa, ioFiles->dropFh);
    else
        fputs(inCa->recLine, ioFiles->dropFh);
    fputc('\n', ioFiles->dropFh);
    }

slFreeList(&overlappedRecLines);
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
struct slRef *overlappedRecLines = NULL;
struct ioFiles ioFiles;
ZeroVar(&ioFiles);
ioFiles.inLf = (inFmt == PSL_FMT) ? pslFileOpen(inFile) : lineFileOpen(inFile, TRUE);

loadSelectTable(selectFile);
ioFiles.outFh = mustOpen(outFile, "w");
if (dropFile != NULL)
    ioFiles.dropFh = mustOpen(dropFile, "w");

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
         "Select records bases on overlaping chromosome ranges.\n"
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
         "  -selectCds - Use only CDS in the select file\n"
         "  -selectCoordCols=spec - Select file is tab-separate with coordinates\n"
         "          as described by spec, which is one of:\n"
         "            o chromCol - chrom in this column followed by start and end.\n"
         "            o chromCol,startCol,endCol - chrom, start, and end in specified\n"
         "              columns.\n"
         "            o chromCol,startCol,endCol,strandCol - chrom, start,, end, and\n"
         "              strand in specified columns.\n"
         "  -inFmt=fmt - specify inFile format, same values as -selectFmt.\n"
         "  -inCoordCols=spec - in file is tab-separate with coordinates specified by\n"
         "          spec, in format described above.\n"
         "  -inCds - Use only CDS in the in file\n"
         "  -nonOverlapping - select non-overlaping instead of overlaping records\n"
         "  -strand - must be on the same strand to be considered overlaping\n"
         "  -excludeSelf - don't compare alignments with the same coordinates and name.\n"
         "  -overlapThreshold=0.0 - minimun fraction of an inFile block that\n"
         "      must be overlapped by a single select record to be considered overlapping.\n"
         "      Note that this is only coverage by a single select record, not total coverage.\n"
         "  -dropped=file - output rows that were dropped to this file.\n",
         "  -merge - output file with be a merge of the input file with the\n"
         "      selectFile records that selected it.  The format is\n"
         "         inRec<tab>selectRec.\n"
         "      if multiple select records hit, inRec is repeated.\n"
         "      This will increase the memory required\n"
         "  -idOutput - output a list of pairs of inId<tab>selectId\n"
         "  -verbose=n - verbose > 1 prints some details\n",
         msg);
}

/* entry */
int main(int argc, char** argv) {
char *selectFile, *inFile, *outFile, *dropFile;
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("wrong # args");
selectFile = argv[1];
inFile = argv[2];
outFile = argv[3];

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
    {
    if (selectFmt != GENEPRED_FMT)
        errAbort("-selectCds only allowed with genePred format select files");
    selectOpts |= selSelectCds;
    }

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
if (inCaOpts & chromAnnCds)
    {
    if (inFmt != GENEPRED_FMT)
        errAbort("-inCds only allowed with genePred format in files");
    }
nonOverlapping = optionExists("nonOverlapping");
if (optionExists("strand"))
    selectOpts |= selUseStrand;
if (optionExists("excludeSelf"))
    selectOpts |= selExcludeSelf;
overlapThreshold = optionFloat("overlapThreshold", 0.0);
doMerge = optionExists("merge");
idOutput = optionExists("idOutput");
if (doMerge && idOutput)
    errAbort("can't specify both -merge and -idOutput");
if (doMerge)
    {
    if (nonOverlapping)
        errAbort("can't use -merge with -nonOverlapping");
    selectOpts |= selSaveLines;
    }

dropFile = optionVal("dropped", NULL);

overlapSelect(selectFile, inFile, outFile, dropFile);
return 0;
}
