/* overlapSelect - select records based on overlap of chromosome ranges */

#include "common.h"
#include "linefile.h"
#include "selectTable.h"
#include "psl.h"
#include "bed.h"
#include "coordCols.h"
#include "genePred.h"
#include "dystring.h"
#include "options.h"

/* FIXME: would be nice to be able to specify ranges in the same manner
 * as featureBits */

static struct optionSpec optionSpecs[] = {
    {"selectFmt", OPTION_STRING},
    {"selectCoordCols", OPTION_STRING},
    {"inFmt", OPTION_STRING},
    {"inCoordCols", OPTION_STRING},
    {"nonOverlaping", OPTION_BOOLEAN},
    {"strand", OPTION_BOOLEAN},
    {"excludeSelf", OPTION_BOOLEAN},
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
unsigned inFmt = UNKNOWN_FMT;
struct coordCols selectCoordCols;
struct coordCols inCoordCols;
boolean nonOverlaping = FALSE;
unsigned selectOptions = 0;

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

static boolean pslSelected(struct psl* psl)
/* Check if a PSL should be selected for output */
{
int iBlk;
char strand;
boolean overlaps = FALSE;
if (psl->strand[1] == '\0')
    strand = psl->strand[0];
else
    strand = (psl->strand[0] != psl->strand[1]) ? '-' : '+';

for (iBlk = 0; (iBlk < psl->blockCount) && !overlaps; iBlk++)
    {
    int start = psl->tStarts[iBlk];
    int end = start + psl->blockSizes[iBlk];
    if (psl->strand[1] == '-')
        reverseIntRange(&start, &end, psl->tSize);
    overlaps = selectIsOverlapped(selectOptions, psl->qName,
                                  psl->tName, start, end, strand);
    }
if (nonOverlaping)
    return !overlaps;
else
    return overlaps;
}

void pslSelect(char* inFile, char* outFile)
/* copy psl records that matches the selection criteria */
{
struct lineFile *lf = pslFileOpen(inFile);
FILE* outFh = mustOpen(outFile, "w");
char* row[PSL_NUM_COLS];

while (lineFileNextRowTab(lf, row, PSL_NUM_COLS))
    {
    struct psl* psl = pslLoad(row);
    if (pslSelected(psl))
        pslTabOut(psl, outFh);
    pslFree(&psl);
    }
carefulClose(&outFh);
lineFileClose(&lf);
}

static boolean genePredSelected(struct genePred* gp)
/* Check if a genePred should be selected for output */
{
int iExon;
boolean overlaps = FALSE;
for (iExon = 0; (iExon < gp->exonCount) && !overlaps; iExon++)
    {
    overlaps = selectIsOverlapped(selectOptions, gp->name,
                                  gp->chrom, gp->exonStarts[iExon],
                                  gp->exonEnds[iExon], gp->strand[0]);
    }
if (nonOverlaping)
    return !overlaps;
else
    return overlaps;
}

void genePredSelect(char* inFile, char* outFile)
/* copy genePred records that matches the selection criteria */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE* outFh = mustOpen(outFile, "w");
char* row[GENEPRED_NUM_COLS];
while (lineFileNextRowTab(lf, row, GENEPRED_NUM_COLS))
    {
    struct genePred *gp = genePredLoad(row);
    if (genePredSelected(gp))
        genePredTabOut(gp, outFh);
    genePredFree(&gp);
    }
carefulClose(&outFh);
lineFileClose(&lf);
}

static boolean bedSelected(struct bed* bed)
/* Check if a bed should be selected for output */
{
boolean overlaps = FALSE;
if (bed->blockCount == 0)
    {
    overlaps = selectIsOverlapped(selectOptions, bed->name,
                                  bed->chrom, bed->chromStart,
                                  bed->chromEnd, bed->strand[0]);
    }
else
    {
    int iBlk;
    for (iBlk = 0; (iBlk < bed->blockCount) && !overlaps; iBlk++)
        {
        int start = bed->chromStart + bed->chromStarts[iBlk];
        overlaps = selectIsOverlapped(selectOptions, bed->name, 
                                      bed->chrom, start,
                                      start + bed->blockSizes[iBlk],
                                      bed->strand[0]);
        }
    }
if (nonOverlaping)
    return !overlaps;
else
    return overlaps;
}

void bedSelect(char* inFile, char* outFile)
/* copy bed records that matches the selection criteria */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE* outFh = mustOpen(outFile, "w");
char* row[12];
int numCols;
while ((numCols = lineFileChopNextTab(lf, row, ArraySize(row))) > 0)
    {
    struct bed *bed = bedLoadN(row, numCols);
    if (bedSelected(bed))
        bedTabOutN(bed, numCols, outFh);
    bedFree(&bed);
    }
carefulClose(&outFh);
lineFileClose(&lf);
}

void coordColsSelect(char* inFile, char* outFile)
/* copy records that matches the selection criteria when the coordinates are
 * specified by start column. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE* outFh = mustOpen(outFile, "w");
char *line;
char** row;
int numCols;
struct coordColVals colVals;
struct dyString *lineBuf = dyStringNew(512);

row = needMem(inCoordCols.minNumCols*sizeof(char*));

while (lineFileNextReal(lf, &line))
    {
    dyStringClear(lineBuf);
    dyStringAppend(lineBuf, line);
    lineFileReuse(lf);
    numCols = lineFileChopNextTab(lf, row, inCoordCols.minNumCols);

    colVals = coordColParseRow(&inCoordCols, lf, row, numCols);
    if (selectIsOverlapped(selectOptions, NULL, colVals.chrom,
                           colVals.start, colVals.end, colVals.strand))
        fprintf(outFh, "%s\n", lineBuf->string);
    }
carefulClose(&outFh);
lineFileClose(&lf);
freez(&row);
dyStringFree(&lineBuf);
}

void loadSelectTable(char *selectFile)
/* load the select table from a file */
{
switch (selectFmt)
    {
    case PSL_FMT:
        selectAddPsls(selectFile);
        break;
    case GENEPRED_FMT:
        selectAddGenePreds(selectFile);
        break;
    case BED_FMT:
        selectAddBeds(selectFile);
        break;
    case COORD_COLS_FMT:
        selectAddCoordCols(selectFile, &selectCoordCols);
        break;
    }
}

void overlapSelect(char *selectFile, char *inFile, char *outFile)
/* select records based on overlap of chromosome ranges */
{
loadSelectTable(selectFile);

switch (inFmt)
    {
    case PSL_FMT:
        pslSelect(inFile, outFile);
        break;
    case GENEPRED_FMT:
        genePredSelect(inFile, outFile);
        break;
    case BED_FMT:
        bedSelect(inFile, outFile);
        break;
    case COORD_COLS_FMT:
        coordColsSelect(inFile, outFile);
        break;
    }
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
         "  -inFmt=fmt - specify inFile format, same values as -selectFmt.\n"
         "  -selectCoordCols=spec - Select file is tab-separate with coordinates\n"
         "          as described by spec, which is one of:\n"
         "            o chromCol - chrom in this column followed by start and end.\n"
         "            o chromCol,startCol,endCol - chrom, start, and end in specified\n"
         "              columns.\n"
         "            o chromCol,startCol,endCol,strandCol - chrom, start,, end, and\n"
         "              strand in specified columns.\n"
         "  -inCoordCols=spec - in file is tab-separate with coordinates specified by\n"
         "          spec, in format described above.\n"
         "  -nonOverlaping - select non-overlaping instead of overlaping records\n"
         "  -strand - must be on the same strand to be considered overlaping\n"
         "  -excludeSelf - don't compare alignments with the same id.\n",
         msg);
}

/* entry */
int main(int argc, char** argv) {
char *selectFile, *inFile, *outFile;
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

nonOverlaping = optionExists("nonOverlaping");
if (optionExists("strand"))
    selectOptions |= SEL_USE_STRAND;
if (optionExists("excludeSelf"))
    selectOptions |= SEL_EXCLUDE_SELF;

overlapSelect(selectFile, inFile, outFile);
return 0;
}
