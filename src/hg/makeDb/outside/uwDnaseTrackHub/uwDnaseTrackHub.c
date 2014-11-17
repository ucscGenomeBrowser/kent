/* uwDnaseTrackHub - Create trackDb.txt file for UW DnaseI hypersensitivity data. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "portable.h"
#include "bigWig.h"

char *assembly = "hg38";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "uwDnaseTrackHub - Create track hub for UW DnaseI hypersensitivity data\n"
  "usage:\n"
  "   uwDnaseTrackDb meta.tab run_pooled color.tab outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct colorTab
/* Color and other info */
    {
    struct colorTab *next;  /* Next in singly linked list. */
    char *label;	/* Short human readable label */
    double pos;	/* Position between 0 and 1 */
    int r;	/* Red component 0-255 */
    int g;	/* Green component 0-255 */
    int b;	/* Blue component 0-255 */
    char *wigFile;	/* Wig file name */
    };

struct colorTab *colorTabLoad(char **row)
/* Load a colorTab from row fetched with select * from colorTab
 * from database.  Dispose of this with colorTabFree(). */
{
struct colorTab *ret;

AllocVar(ret);
ret->label = cloneString(row[0]);
ret->pos = sqlDouble(row[1]);
ret->r = sqlSigned(row[2]);
ret->g = sqlSigned(row[3]);
ret->b = sqlSigned(row[4]);
ret->wigFile = cloneString(row[5]);
return ret;
}

struct colorTab *colorTabLoadAll(char *fileName) 
/* Load all colorTab from a whitespace-separated file.
 * Dispose of this with colorTabFreeList(). */
{
struct colorTab *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[6];

while (lineFileRowTab(lf, row))
    {
    el = colorTabLoad(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct expMeta
/* Experiment metadata */
    {
    struct expMeta *next;  /* Next in singly linked list. */
    char *acc;	/* Experiment accession */
    char *cell;	/* Cell line */
    char *sex;  /* Sex - M/F/U */
    int taxon;	/* NCBI taxon - 9606 for human */
    char *treatment;	/* Treatment - n/a for none */
    char *label;	/* Human readable label */
    };


struct expMeta *expMetaLoad(char **row)
/* Load a expMeta from row fetched with select * from expMeta
 * from database.  Dispose of this with expMetaFree(). */
{
struct expMeta *ret;

AllocVar(ret);
ret->acc = cloneString(row[0]);
ret->cell = cloneString(row[1]);
ret->sex = cloneString(row[2]);
ret->taxon = sqlSigned(row[3]);
ret->treatment = cloneString(row[4]);
subChar(ret->treatment, '_', ' ');
ret->label = cloneString(row[5]);
return ret;
}

struct expMeta *expMetaLoadAll(char *fileName) 
/* Load all expMeta from a whitespace-separated file.
 * Dispose of this with expMetaFreeList(). */
{
struct expMeta *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[6];

while (lineFileRowTab(lf, row))
    {
    el = expMetaLoad(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void writeGenomesTxt(char *outFile)
/* Write genome.txt part of hub */
{
FILE *f = mustOpen(outFile, "w");
fprintf(f, "genome %s\n", assembly);
fprintf(f, "trackDb %s/trackDb.txt\n", assembly);
fprintf(f, "\n");
carefulClose(&f);
}

void writeHubTxt(char *outFile)
/* Write out simple hub.txt file. */
{
FILE *f = mustOpen(outFile, "w");
fprintf(f, "hub uw_dnase_hg38_01\n");
fprintf(f, "shortLabel ENCODE2 UW DNase\n");
fprintf(f, "longLabel ENCODE2 University of Washington DNase mapped to hg38.\n");
fprintf(f, "genomesFile genomes.txt\n");
fprintf(f, "email genome@soe.ucsc.edu\n");
carefulClose(&f);
}

char *fileToAcc(char *path)
/* Extract accession out of path */
{
static char acc[FILENAME_LEN];
splitPath(path, NULL, acc, NULL);
chopSuffix(acc);
return acc;
}

int lighten(double col)
/* Return color component from 0-255 blended 1/3 with white */
{
return    round(255.0/3 + 0.666667*col);
}

void writeSubgroups(FILE *f, char *view, char *acc, char *treatment)
/* Write out a subgroups line */
{
fprintf(f, "\tsubGroups view=%s cellType=%s treatment=%s\n", view, acc, treatment);
}

void writeTrackDbTxt(char *fileName, struct expMeta *metaList, struct colorTab *colorList)
/* Write out trackDb file */
{
FILE *f = mustOpen(fileName, "w");

/* Make hash of metadata keyed by accession */
struct hash *metaHash = hashNew(0);
struct expMeta *meta;
for (meta = metaList; meta != NULL; meta = meta->next)
    hashAdd(metaHash, meta->acc, meta);

/* Make hash of wig summaries */
struct hash *wigSumHash = hashNew(0);
struct colorTab *col;
for (col = colorList; col != NULL; col = col->next)
    {
    char *acc = fileToAcc(col->wigFile);
    struct bbiFile *bbi = bigWigFileOpen(col->wigFile);
    struct bbiSummaryElement sum = bbiTotalSummary(bbi);
    hashAdd(wigSumHash, acc, CloneVar(&sum));
    bigWigFileClose(&bbi);
    }

/** Write out multi-view composite track */
/* Write out some of the constant bits. */
char *rootName = "uwEnc2Dnase";
fprintf(f, "track %s\n", rootName);
fprintf(f, "compositeTrack on\n");
fprintf(f, "shortLabel ENCODE2 UW DNase\n");
fprintf(f, "longLabel ENCODE2 University of Washington DNase I Hypersensitivity\n");
fprintf(f, "group regulation\n");
fprintf(f, "subGroup1 view Views Hot=HotSpots Peaks=Peaks Signal=Signal\n");

/* Write out cell types as subGroup2 */
fprintf(f, "subGroup2 cellType Cell_Line");
for (col = colorList; col != NULL; col = col->next)
    {
    char *acc = fileToAcc(col->wigFile);
    char underbarred[64];
    safef(underbarred, sizeof(underbarred), "%s", col->label);
    subChar(underbarred, ' ', '_');
    fprintf(f, " %s=%s", acc, underbarred);
    }
fprintf(f, "\n");

/* Write out treatment as subGroup3. Also create hash with underbarred values for treatments. */
fprintf(f, "subGroup3 treatment Treatment");
struct hash *treatmentHash = hashNew(0);
for (meta = metaList; meta != NULL; meta = meta->next)
    {
    char *treatment = meta->treatment;
    if (hashLookup(treatmentHash, treatment) == NULL)
	{
	char underbarred[64];
	if (sameString("n/a", treatment))
	    {
	    hashAdd(treatmentHash, treatment, "n_a");
	    fprintf(f, " n_a=n/a");
	    }
	else
	    {
	    safef(underbarred, sizeof(underbarred), "%s", treatment);
	    subChar(underbarred, ' ', '_');
	    hashAdd(treatmentHash, treatment, cloneString(underbarred));
	    fprintf(f, " %s=%s", underbarred, underbarred);
	    }
	}
    }
fprintf(f, "\n");

/* Write out some more relatively constant parts to finish up root. */
fprintf(f, "dimensions dimensionY=cellType\n");
fprintf(f, "sortOrder view=+ cellType=+ treatment=+\n");
fprintf(f, "noInherit on\n");
fprintf(f, "type bed 3 +\n");
fprintf(f, "\n");

/* Write out hotspot view */
fprintf(f, "    track %sHot\n", rootName);
fprintf(f, "    shortLabel Hot Spots\n");
fprintf(f, 
    "    longLabel HotSpot5 hotspot calls on BWA. Dupe, sponge and mitochondria filtered\n");
fprintf(f, "    view Hot\n");
fprintf(f, "    visibility hide\n");
fprintf(f, "    parent %s\n", rootName);
fprintf(f, "    pValueFilter 0.0\n");
fprintf(f, "    pValueFilterLimits 1:324\n");
fprintf(f, "    scoreFilter 0\n");
fprintf(f, "    scoreFilterLimits 0:1000\n");
fprintf(f, "\n");

/* Write out hotspot subtracks. */
for (col = colorList; col != NULL; col = col->next)
    {
    char *acc = fileToAcc(col->wigFile);
    meta = hashMustFindVal(metaHash, acc);
    fprintf(f, "\ttrack %sHotW%s\n", rootName, acc+1);
    fprintf(f, "\tparent %sHot off\n", rootName);
    fprintf(f, "\tshortLabel %s Ht\n", col->label);
    fprintf(f, "\tlongLabel %s DNAseI HS HotSpots from ENCODE2/UW %s\n", col->label, acc);
    writeSubgroups(f, "Hot", acc, hashFindVal(treatmentHash, meta->treatment));
    fprintf(f, "\ttype bigBed\n");
    fprintf(f, "\tbigDataUrl data/%s.pooled.broadPeak\n", acc);
    fprintf(f, "\n");
    }

/* Write out peaks view */
fprintf(f, "    track %sPeaks\n", rootName);
fprintf(f, "    shortLabel Peaks\n");
fprintf(f, 
    "    longLabel HotSpot5 peak calls on BWA. Dupe, sponge and mitochondria filtered\n");
fprintf(f, "    view Peaks\n");
fprintf(f, "    visibility hide\n");
fprintf(f, "    parent %s\n", rootName);
fprintf(f, "    pValueFilter 0.0\n");
fprintf(f, "    pValueFilterLimits 1:324\n");
fprintf(f, "    scoreFilter 0\n");
fprintf(f, "    scoreFilterLimits 0:1000\n");
fprintf(f, "\n");

/* Write out peak subtracks. */
for (col = colorList; col != NULL; col = col->next)
    {
    char *acc = fileToAcc(col->wigFile);
    fprintf(f, "\ttrack %sPeaksW%s\n", rootName, acc+1);
    fprintf(f, "\tparent %sPeaks off\n", rootName);
    fprintf(f, "\tshortLabel %s Pk\n", col->label);
    fprintf(f, "\tlongLabel %s DNAseI HS Peaks from ENCODE2/UW %s\n", col->label, acc);
    writeSubgroups(f, "Peaks", acc, hashFindVal(treatmentHash, meta->treatment));
    fprintf(f, "\ttype bigBed\n");
    fprintf(f, "\tbigDataUrl data/%s.pooled.narrowPeak\n", acc);
    fprintf(f, "\n");
    }

/* Write out signal view. */
fprintf(f, "    track %sSignal\n", rootName);
fprintf(f, "    shortLabel Signal\n");
fprintf(f, 
    "    longLabel HotSpot5 signal on BWA. Dupe, sponge and mitochondria filtered\n");
fprintf(f, "    view Signal\n");
fprintf(f, "    visibility full\n");
fprintf(f, "    parent %s\n", rootName);
fprintf(f, "    viewLimits 0:100\n");
fprintf(f, "    minLimit 0\n");
fprintf(f, "    maxLimit 100000\n");
fprintf(f, "    autoScale off\n");
fprintf(f, "    maxHeightPixels 100:32:16\n");
fprintf(f, "    windowingFunction mean+whiskers\n");
fprintf(f, "\n");

/* Write out signal subtracks. */
for (col = colorList; col != NULL; col = col->next)
    {
    char *acc = fileToAcc(col->wigFile);
    meta = hashMustFindVal(metaHash, acc);
    fprintf(f, "\ttrack %sSignalW%s\n", rootName, acc+1);
    fprintf(f, "\tparent %sSignal off\n", rootName);
    fprintf(f, "\tshortLabel %s Sg\n", col->label);
    if (sameWord(meta->treatment, "n/a"))
	fprintf(f, "\tlongLabel %s DNAseI HS Signals from ENCODE2/UW %s\n", col->label, acc);
    else
        fprintf(f, "\tlongLabel %s %s DNAseI HS Signals from ENCODE2/UW %s\n", 
	    meta->cell, meta->treatment, acc);
    writeSubgroups(f, "Signal", acc, hashFindVal(treatmentHash, meta->treatment));

    /* Figure out bounds of bigWig */
    struct bbiSummaryElement *sum = hashMustFindVal(wigSumHash, acc);
    fprintf(f, "\ttype bigWig %g %g\n", sum->minVal, sum->maxVal);
    fprintf(f, "\tbigDataUrl data/%s.norm.bw\n", acc);
    fprintf(f, "\n");
    }

/** Write out container multiWig track */
char multiName[128];
safef(multiName, sizeof(multiName), "%sWig", rootName);
fprintf(f, "track %s\n", multiName);
fprintf(f, "shortLabel ENCODE2 DNAse Col\n");
fprintf(f, "longLabel ENCODE2 UW DNAse Hypersensitivity Signal Colored by Similarity\n");
fprintf(f, "visibility full\n");
fprintf(f, "type bigWig 0 10000\n");
fprintf(f, "container multiWig\n");
fprintf(f, "configurable on\n");
fprintf(f, "viewLimits 0:200\n");
fprintf(f, "showSubtrackColorOnUi on\n");
fprintf(f, "aggregate transparentOverlay\n");
fprintf(f, "\n");

/* Write out colored wig components */
for (col = colorList; col != NULL; col = col->next)
    {
    char *acc = fileToAcc(col->wigFile);
    meta = hashMustFindVal(metaHash, acc);
    fprintf(f, "\ttrack %sW%s\n", multiName, acc+1);
    fprintf(f, "\tparent %s off\n", multiName);
    fprintf(f, "\tshortLabel %s Sg\n", col->label);
    if (sameWord(meta->treatment, "n/a"))
	fprintf(f, "\tlongLabel %s DNAseI HS Signals from ENCODE2/UW %s\n", col->label, acc);
    else
        fprintf(f, "\tlongLabel %s %s DNAseI HS Signals from ENCODE2/UW %s\n", 
	    meta->cell, meta->treatment, acc);
    fprintf(f, "\tbigDataUrl data/%s.norm.bw\n", acc);

    /* Figure out bounds of bigWig */
    struct bbiSummaryElement *sum = hashMustFindVal(wigSumHash, acc);
    fprintf(f, "\ttype bigWig %g %g\n", sum->minVal, sum->maxVal);
    fprintf(f, "\tcolor %d,%d,%d\n", lighten(col->r), lighten(col->g), lighten(col->b));
    fprintf(f, "\tpriority %g\n", col->pos+1);
    fprintf(f, "\n");
    }


/* Clean up */
carefulClose(&f);
}

void uwDnaseTrackDb(char *metaFile, char *dataDir, char *colorFile, char *outDir)
/* uwDnaseTrackDb - Create trackDb.txt file for UW DnaseI hypersensitivity data. */
{
/* Load list inputs */
struct expMeta *metaList = expMetaLoadAll(metaFile);
struct colorTab *colorList = colorTabLoadAll(colorFile);
verbose(1, "Loaded %d from %s and %d from %s\n", 
    slCount(metaList), metaFile, slCount(colorList), colorFile);

/* Make output directory and text files */
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s", outDir, assembly);
makeDirsOnPath(path);
safef(path, sizeof(path), "%s/%s", outDir, "hub.txt");
writeHubTxt(path);
safef(path, sizeof(path), "%s/%s", outDir, "genomes.txt");
writeGenomesTxt(path);
safef(path, sizeof(path), "%s/%s/trackDb.txt", outDir, assembly);
writeTrackDbTxt(path, metaList, colorList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
uwDnaseTrackDb(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
