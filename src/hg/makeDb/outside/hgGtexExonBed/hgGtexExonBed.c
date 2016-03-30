/* hgGtexExonBed - Load BED6 tables of per-exon data from NIH Common Fund Exon Tissue Expression (GTEX)
        Format:  chrom, chromStart, chromEnd, name, score, strand,
    Uses hgFixed data tables loaded via hgGtex
*/
/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "verbose.h"
#include "options.h"
#include "hash.h"
#include "jksql.h"
#include "hgRelate.h"
#include "memgfx.h"
#include "basicBed.h"
#include "linefile.h"
#include "gtexTissue.h"
#include "gtexTissueMedian.h"

#define GTEX_TISSUE_MEDIAN_TABLE  "gtexExonTissueMedian"

// Versions are used to suffix tablenames
static char *version = "V6";

boolean doLoad = FALSE;
boolean trackDb = FALSE;
boolean bright = FALSE;
char *database, *table;
char *tabDir = ".";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGtexExonBed - Write BED files (one per tissue) of per exon data from GTEX exon data tables.\n"
  "          These will be scored using bedScore on the float score field, then loaded. \n"
  "usage:\n"
  "   hgGtexExonBed database tableRoot exonCoordsFile\n"
  "options:\n"
  "    -version=VN (default \'%s\')\n"
  "    -trackDb         trackDb only\n"
  "    -bright          assign bright colors to tracks\n"
  , version);
}

static struct optionSpec options[] = {
    {"version", OPTION_STRING},
    {"trackDb", OPTION_BOOLEAN},
    {"bright", OPTION_BOOLEAN},
    {NULL, 0},
};

struct hash *parseExons(char *coordsFile)
/* Get exon coordinates. 
 * Format: <exonId> <chr> <start+1> <end> <strand>
 *   Tab-separated, lacks 'chr' prefix */
{
char *words[6];
struct lineFile *lf = lineFileOpen(coordsFile, TRUE);
struct bed *bed = NULL;
char buf[64];
struct hash *exonHash = hashNew(0);
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    int count = chopTabs(line, words);
    lineFileExpectWords(lf, 5, count);
    AllocVar(bed);
    bed->name = words[0];
    safef(buf, sizeof(buf), "chr%s", words[1]);
    bed->chrom = cloneString(buf);
    bed->chromStart = sqlUnsigned(words[2])-1;
    bed->chromEnd = sqlUnsigned(words[3]);
    bed->strand[0] = words[4][0];
    hashAdd(exonHash, bed->name, bed);
    }
lineFileClose(&lf);
return exonHash;
}

void hgGtexExonBed(char *database, char *tableRoot, char *coordsFile)
/* Main function to load tables*/
{
char **row;
struct sqlResult *sr;
char query[128];
struct sqlConnection *conn = sqlConnect("hgFixed");
struct hash *exonBedHash = NULL;

if (!trackDb)
    {
    // Read in exon coordinates from GTEX GENCODE-based reference file
    verbose(2, "Reading exons coordinates from file %s\n", coordsFile);
    exonBedHash = parseExons(coordsFile);

    verbose(3, "Found %d exons in coordinate file %s\n", 
            hashNumEntries(exonBedHash), coordsFile);
    }

// Read in tissue info (in id order) from hgFixed table
struct gtexTissue *gtexTissues = gtexGetTissues(version);
struct gtexTissue *tissue = NULL;

char trackDbFile[64];
safef(trackDbFile, sizeof(trackDbFile), "trackDb.%s.ra", tableRoot);
FILE *f = fopen(trackDbFile, "w");

FILE **tissueFiles = NULL;
AllocArray(tissueFiles, slCount(gtexTissues));
char tissueFile[64];
int i;

// Print trackDb
fprintf(f, "track gtexExon\n");
fprintf(f, "compositeTrack on\n");
fprintf(f, "shortLabel GTEx Exon\n");
fprintf(f, "longLabel  Exon Expression in 53 tissues from GTEx RNA-seq of 2921 samples (214 donors) \n");
fprintf(f, "dragAndDrop subTracks\n");
fprintf(f, "group expression\n");
fprintf(f, "type bed 6\n");
fprintf(f, "subGroup1 tissue Tissue \\\n");
for (i=0, tissue = gtexTissues; tissue != NULL; tissue = tissue->next, i++)
    {
    fprintf(f, "\t\t%s=%s %s\n", tissue->name, tissue->name, tissue->next ? "\\": "");
    }
fprintf(f, "\n");
for (i=0, tissue = gtexTissues; tissue != NULL; tissue = tissue->next, i++)
    {
    // create a file for each tissue
    tissue->name[0] = toupper(tissue->name[0]);
    safef(tissueFile, sizeof(tissueFile), "%sTissueExonMedian%s", tableRoot, tissue->name);
    tissue->name[0] = tolower(tissue->name[0]);

    // TODO: BED file please!
    if (!trackDb)
        tissueFiles[i] = hgCreateTabFile(".", tissueFile);

    // print out to trackDb
    fprintf(f, "\ttrack %s\n", tissueFile);
    fprintf(f, "\tparent %sExon\n", tableRoot);
    fprintf(f, "\tshortLabel %s\n", tissue->name);
    sqlSafef(query, sizeof(query), "select count(*) from gtexSample where tissue='%s'", 
                tissue->name);
    int sampleCount = sqlQuickNum(conn, query);
    fprintf(f, "\tlongLabel Exon Expression in %s from GTEX RNA-seq of %d samples\n", 
                tissue->description, sampleCount);
    fprintf(f, "\tsubGroups tissue=%s\n", tissue->name);
    // FIXME
    struct rgbColor rgb = (struct rgbColor){.r=COLOR_32_BLUE(tissue->color), .g=COLOR_32_GREEN(tissue->color), .b=COLOR_32_RED(tissue->color)};
    if (bright)
        rgb = gtexTissueBrightenColor(rgb);
    fprintf(f, "\tcolor %d,%d,%d\n", rgb.r, rgb.g, rgb.b);
    fprintf(f, "\tspectrum on\n");
    fprintf(f, "\n");
    }
fclose(f);

if (trackDb)
    return;

// Read median data from hgFixed table and create BEDs
verbose(2, "Reading gtexTissueExonMedian table\n");
sqlSafef(query, sizeof(query),"SELECT * from %s%s", GTEX_TISSUE_MEDIAN_TABLE, version);
sr = sqlGetResult(conn, query);
struct gtexTissueMedian *exonMedians;
while ((row = sqlNextRow(sr)) != NULL)
    {
    exonMedians = gtexTissueMedianLoad(row);
    struct bed *bed = hashFindVal(exonBedHash, exonMedians->geneId);
    for (i=0, tissue = gtexTissues; i<exonMedians->tissueCount; tissue = tissue->next, i++)
        {
        if (exonMedians->scores[i] == 0.0)
            continue;
        fprintf(tissueFiles[i], "%s\t%d\t%d\t%s\t%d\t%c\t%0.3f\n", 
                    bed->chrom, bed->chromStart, bed->chromEnd, 
                    exonMedians->geneId, 0, bed->strand[0], exonMedians->scores[i]);
        }
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
version = optionVal("version", version);
trackDb = optionExists("trackDb");
bright = optionExists("bright");
hgGtexExonBed(argv[1], argv[2], argv[3]);
return 0;
}




