/* hgGtexAse - Load tables of GTEx Allele-Specific Expression summary data from Lappalainen lab 
        at NY Genome Center.  Contact:  Stephane Castel.

    Header line:  chr, pos, samples, median_coverage, median_ae
*/

/* Copyright (C) 2016 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "portable.h"
#include "verbose.h"
#include "options.h"
#include "basicBed.h"
#include "linefile.h"
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
  "             BED 9+ of ASE with all fields, named by SNP, colored by median ASE\n"
  "usage:\n"
  "   hgGtexAse database tableRoot indir outdir\n"
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

#define GRAY    0x7D7D7D
#define GREEN   0x34D100
#define BLUE    0x3366FF
#define PURPLE  0xCC33FF
#define RED     0xFF3366

unsigned assignColor(float val)
{
int ase = val * 100;
if (ase == 0)
    return GRAY;
if (ase <= 15)
    return GREEN;
if (ase <= 25)
    return BLUE;
if (ase <= 49)
    return PURPLE;
if (ase == 50)
    return RED;
errAbort("ERROR: unexpected ASE value: %0.2f.  Must be 0-.5", val);
return 0;
}

void parseAse(char *aseFile, char *inDir, char *outDir, char *table)
{
verbose(2, "Parsing file `%s`\n", aseFile);
char *words[5];
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
    verbose(3, "    %s\n", line);
    int count = chopTabs(line, words);
    lineFileExpectWords(lf, 5, count);
    AllocVar(bed);
    safef(buf, sizeof(buf), "chr%s", words[0]);
    bed->chrom = cloneString(buf);
    bed->chromStart = bed->thickStart = sqlUnsigned(words[1])-1;
    bed->chromEnd = bed->thickEnd = sqlUnsigned(words[1]);
    bed->name = "rs000000";
    bed->strand[0] = '.';
    bed->samples = sqlUnsigned(words[2]);
    bed->coverage = sqlDouble(words[3]);
    bed->ASE = sqlDouble(words[4]);
    bed->rgb = assignColor(bed->ASE);
    gtexAseTabOut(bed, f);
    }
lineFileClose(&lf);
fclose(f);
}

void hgGtexAse(char *database, char *tableRoot, char *inDir, char *outDir)
/* Main function to parse data files and load tables*/
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
    fprintf(f, "longLabel  Allele-Specific Expression in 53 tissues from GTEx Analysis Group (V6)\n");
    fprintf(f, "dragAndDrop subTracks\n");
    fprintf(f, "group expression\n");
    fprintf(f, "type bed 6\n\n");
    }

// Read median data from files in inDir directory, and create output files
struct slName *file, *files = listDir(inDir, "*.txt");
for (file = files; file != NULL; file = file->next)
    {
    char *fileName = file->name;
    char *tissue = cloneString(fileName);
    chopSuffix(tissue);
    toUpperN(tissue, 1);

    char table[64];
    safef(table, sizeof(table), "%s%s%s", tableRoot, "Bed", tissue);

    parseAse(fileName, inDir, outDir, table);

    fprintf(f, "    track %s\n", table);
    fprintf(f, "    parent %s\n", tableRoot);
    fprintf(f, "    shortLabel %s\n", tissue);
    fprintf(f, "    longLabel Median Allele-Specific Expression in %s from GTEx Analysis Group (V6)\n", tissue);
    fprintf(f, "    type bed 9+\n\n");
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




