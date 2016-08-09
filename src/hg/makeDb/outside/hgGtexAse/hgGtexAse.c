/* hgGtexAse - Load tables of GTEx Allele-Specific Expression summary data from Lappalainen lab 
        at NY Genome Center.  Contact:  Stephane Castel.

    Header line:  chr, pos, rsid, samples, individuals, median_coverage, 
                        min_ae, q1_ae, median_ae, q3_ae, max_ae, std_ae
*/

/* Copyright (C) 2016 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "portable.h"
#include "verbose.h"
#include "memgfx.h"
#include "options.h"
#include "basicBed.h"
#include "linefile.h"
#include "gtexTissue.h"
#include "gtexAse.h"

// Versions are used to suffix tablenames
static char *version = "V6";
boolean trackDb = FALSE;

char *database, *table;
char *tabDir = ".";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGtexAse - Write files of ASE GTEx ASE summary files:\n"
  "             BED 9+9 of ASE (gtexAse.h) with all fields, named by SNP, colored by median ASE\n"
  "usage:\n"
  "   hgGtexAse database tableRoot indir outdir\n"
  "     where files in indir are named TISSUEABBREV.ase_stats.txt\n"
  "             and the combined file starts with 'all'\n"
  "options:\n"
  "    -version=VN (default \'%s\')\n"
  "    -trackDb    add composite to trackDb  \n"
  , version);
}

static struct optionSpec options[] = {
    {"version", OPTION_STRING},
    {"trackDb", OPTION_BOOLEAN},
    {NULL, 0},
};


unsigned assignColor(int score, unsigned color)
/* No ASE -> Gray.  Moderate ASE -> Grayed down tissue color. High ASE -> tissue color */
{
//#define GRAY    0x7D7D7D
#define GRAY    0xA3A3A3
if (score < 300)
    return GRAY;
if (score >= 600)
    return color;
struct rgbColor rgb = mgColorIxToRgb(NULL, color);
struct rgbColor grayRgb = mgColorIxToRgb(NULL, GRAY);
rgb.r += (grayRgb.r - rgb.r) / 2;
rgb.g += (grayRgb.g - rgb.g) / 2;
rgb.b += (grayRgb.b - rgb.b) / 2;
return MAKECOLOR_32(rgb.r, rgb.g, rgb.b);
}

int assignScore(double val)
{
return val * 2000.0;
}

void parseAse(char *aseFile, char *inDir, char *outDir, char *table, unsigned tissueColor)
{
verbose(2, "Parsing file '%s'\n", aseFile);
char *words[32];
char inPath[64];
safef(inPath, sizeof(inPath), "%s/%s", inDir, aseFile);
struct lineFile *lf = lineFileOpen(inPath, TRUE);
chopSuffix(aseFile);
struct gtexAse *bed = NULL;
char buf[64];
char *line;
char outFile[64];
safef(outFile, sizeof(outFile), "%s/%s.tab", outDir, table);
FILE *f = fopen(outFile, "w");

/* skip header */
lineFileNext(lf, &line, NULL);
if (!startsWith("chr", line))
    errAbort("Bad format in first line of file '%s': must start with 'chr'", aseFile);

while (lineFileNext(lf, &line, NULL))
    {
    verbose(5, "    %s\n", line);
    int count = chopTabs(line, words);
    lineFileExpectWords(lf, 12, count);
    AllocVar(bed);
    safef(buf, sizeof(buf), "chr%s", words[0]);
    bed->chrom = cloneString(buf);
    bed->chromStart = bed->thickStart = sqlUnsigned(words[1])-1;
    bed->chromEnd = bed->thickEnd = sqlUnsigned(words[1]);
    bed->name = words[2];
    if (isEmpty(bed->name))
        {
        safef(buf, sizeof buf, "%s:%d", bed->chrom, bed->chromStart+1);
        bed->name = cloneString(buf);
        }
    bed->strand[0] = '.';
    bed->samples = sqlUnsigned(words[3]);
    bed->donors = sqlUnsigned(words[4]);
    bed->coverage = sqlDouble(words[5]);
    bed->minASE = sqlDouble(words[6]);
    bed->q1ASE = sqlDouble(words[7]);

    bed->medianASE = sqlDouble(words[8]);
    bed->score = assignScore(bed->medianASE);
    bed->itemRgb = assignColor(bed->score, tissueColor);

    bed->q3ASE = sqlDouble(words[9]);
    bed->maxASE = sqlDouble(words[10]);
    bed->stdASE = sqlDouble(words[11]);
    gtexAseTabOut(bed, f);
    }
lineFileClose(&lf);
fclose(f);
}

struct hash *getTissueByAbbrev(char *version)
/* Make hash of tissue info by abbreviation (to retrieve UCSC short name) */
{
struct gtexTissue *tis, *tissues = gtexGetTissues(version);
struct hash *tisHash = hashNew(0);
for (tis = tissues; tis != NULL; tis = tis->next)
    hashAdd(tisHash, tis->abbrev, tis);
return tisHash;
}

void hgGtexAse(char *database, char *tableRoot, char *inDir, char *outDir)
/* Main function to parse data files and load tables */
{
// Create trackDb file 
char trackDbFile[64];
safef(trackDbFile, sizeof(trackDbFile), "%s/trackDb.%s.ra", outDir, tableRoot);
FILE *f = fopen(trackDbFile, "w");

// Start trackDb
if (trackDb)
    {
    fprintf(f, "track %s\n", tableRoot);
    fprintf(f, "compositeTrack on\n");
    fprintf(f, "shortLabel GTEx ASE\n");
    fprintf(f, "longLabel  Allele-Specific Expression Summary in 53 tissues from GTEx Analysis (NY Genome Ctr)\n");
    fprintf(f, "dragAndDrop subTracks\n");
    //fprintf(f, "group expression\n");
    fprintf(f, "itemRgb on\n");
    fprintf(f, "darkerLabels on\n");
    fprintf(f, "scoreFilter 500\n");
    fprintf(f, "type bigBed 9 +\n\n");
    }

// Read median data from files in inDir directory, and create output files
struct slName *file, *files = listDir(inDir, "*.txt");
struct hash *tissues = getTissueByAbbrev(version);

unsigned color;
char *name, *descr;
for (file = files; file != NULL; file = file->next)
    {
    char *fileName = file->name;
    if (startsWith("all", fileName))
        {
        color = 0;
        name = "combined";
        descr = "All Tissues";
        }
    else
        {
        char *tissueAbbrev = cloneString(fileName);
        chopSuffix(tissueAbbrev);
        chopSuffix(tissueAbbrev);
        struct gtexTissue *tissue = hashMustFindVal(tissues, tissueAbbrev);
        color = tissue->color;
        name = tissue->name;
        descr = tissue->description;
        }

    char *tissueUpper = cloneString(name);
    toUpperN(tissueUpper, 1);
    char table[64];
    safef(table, sizeof(table), "%s%s", tableRoot, tissueUpper);
    parseAse(fileName, inDir, outDir, table, color);

    fprintf(f, "    track %s\n", table);
    fprintf(f, "    parent %s\n", tableRoot);
    fprintf(f, "    shortLabel %s\n", name);
    fprintf(f, "    longLabel Median Allele-Specific Expression in %s from GTEx Analysis (NY Genome Ctr)\n", descr);
    fprintf(f, "    type bigBed 9 +\n");
    fprintf(f, "    bigDataUrl %s.bb\n", table);
    struct rgbColor rgb;
    rgb.b = (color >> 0) & 0xff;
    rgb.g = (color >> 8) & 0xff;
    rgb.r = (color >> 16) & 0xff;
    fprintf(f, "    color %d,%d,%d\n", rgb.r, rgb.g, rgb.b);
    fprintf(f, "\n");
    }
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
version = optionVal("version", version);
trackDb = optionExists("trackDb");
hgGtexAse(argv[1], argv[2], argv[3], argv[4]);
return 0;
}




