/* uwDnaseTrackHub - Create trackDb.txt file for UW DnaseI hypersensitivity data. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "portable.h"
#include "bigWig.h"
#include "encode/wgEncodeCell.h"

char *assembly = "hg38";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "uwDnaseTrackHub - Create track hub for UW DnaseI hypersensitivity data\n"
  "usage:\n"
  "   uwDnaseTrackDb meta.tab run_pooled color.tab outDir\n"
  "options:\n"
  "   -cellFile\n"
  );
}

#define CELL_FILE_OPTION "cellFile"

/* Command line validation table. */
static struct optionSpec options[] = {
    {CELL_FILE_OPTION, OPTION_STRING},
   {NULL, 0},
};

/* Cell info dumped from hgFixed.wgEncodeCell */
char *cellFile = NULL;
struct hash *cellHash = NULL;
struct hash *tissueHash = NULL;
struct hash *treatmentHash = NULL;

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


void writeLongLabel(FILE *f, char *end, struct colorTab *col, struct expMeta *meta)
/* Construct long label from metadata */
{
char *label = cloneString(col->label);
char *cellType = nextWord(&label);
fprintf(f, "\tlongLabel %s ", cellType);
if (cellFile)
    {
    struct wgEncodeCell *cellInfo = hashFindVal(cellHash, meta->cell);
    if (cellInfo != NULL)
        fprintf(f,"%s ", cellInfo->description);
    }
if (differentString("n/a", meta->treatment))
    fprintf(f,"(%s) ", label);
fprintf(f, "%s\n", end);
}

void writeSubgroups(FILE *f, char *view, char *cellType, char *treatment)
/* Write out a subgroups line */
{
struct wgEncodeCell *cellInfo = NULL;
if (cellFile)
    cellInfo = hashFindVal(cellHash, cellType);
fprintf(f, "\tsubGroups ");
if (view != NULL)
    fprintf(f, "view=%s ", view);
fprintf(f, "cellType=%s treatment=%s", cellType, treatment);
if (cellInfo)
    {
    char *tissue = (char *)hashFindVal(tissueHash, cellInfo->tissue);
    fprintf(f, " tissue=%s cancer=%s", tissue, cellInfo->cancer);
    }
fprintf(f, "\n");
}

void writeCellSubgroup(FILE *f, char *group)
{
fprintf(f, "%s cellType Cell_Line", group);
struct hashEl *hel, *helList = hashElListHash(cellHash);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    char *value = hel->name;
    fprintf(f, " %s=%s", value, value);
    }
fprintf(f, "\n");
}

struct hash *getTreatmentHash(struct expMeta *metaList)
{
struct hash *treatmentHash = hashNew(0);
struct expMeta *meta;
for (meta = metaList; meta != NULL; meta = meta->next)
    {
    char *treatment = meta->treatment;
    if (hashLookup(treatmentHash, treatment) == NULL)
	{
	char underbarred[64];
	if (sameString("n/a", treatment))
	    {
	    hashAdd(treatmentHash, treatment, "n_a");
	    }
	else
	    {
	    safef(underbarred, sizeof(underbarred), "%s", treatment);
	    subChar(underbarred, ' ', '_');
	    hashAdd(treatmentHash, treatment, cloneString(underbarred));
	    }
	}
    }
return treatmentHash;
}

void writeTreatmentSubgroup(FILE *f, char *group)
{
fprintf(f, "%s treatment Treatment", group);
struct hashEl *hel, *helList = hashElListHash(treatmentHash);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    char *value = (char *)hel->val;
    if (sameString(value, "n_a"))
        fprintf(f, " n_a=n/a");
    else
        fprintf(f, " %s=%s", value, value);
    }
fprintf(f, "\n");
}

void writeTissueSubgroup(FILE *f, char *group)
{
if (cellFile == NULL)
    return;
fprintf(f, "%s tissue Tissue", group);
struct hashEl *hel, *helList = hashElListHash(tissueHash);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    char *value = (char *)hel->val;
    fprintf(f, " %s=%s", value, value);
    }
fprintf(f, "\n");
}

void writeTrackDbTxt(char *fileName, struct expMeta *metaList, struct colorTab *colorList,
                        struct wgEncodeCell *cellList)
/* Write out trackDb file */
{
FILE *f = mustOpen(fileName, "w");

/* Make hash of metadata keyed by accession */
struct hash *metaHash = hashNew(0);
struct expMeta *meta;
for (meta = metaList; meta != NULL; meta = meta->next)
    hashAdd(metaHash, meta->acc, meta);

treatmentHash = getTreatmentHash(metaList);

/* Make hash of cell and tissue info */
cellHash = hashNew(0);
tissueHash = hashNew(0);
struct wgEncodeCell *cellInfo;
char underbarred[64];
if (cellFile)
    {
    for (cellInfo = cellList; cellInfo != NULL; cellInfo = cellInfo->next)
        {
        if (hashLookup(cellHash, cellInfo->term) == NULL)
            hashAdd(cellHash, cellInfo->term, cellInfo);
        if (hashLookup(tissueHash, cellInfo->tissue) == NULL)
            {
            safef(underbarred, sizeof(underbarred), "%s", cellInfo->tissue);
            subChar(underbarred, ' ', '_');
            hashAdd(tissueHash, cellInfo->tissue, cloneString(underbarred));
            }
        }
    }

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
fprintf(f, "shortLabel UW DNase\n");
fprintf(f, "longLabel DNase I Hypersensitivity from UW/ENCODE2\n");
fprintf(f, "group regulation\n");
fprintf(f, "subGroup1 view Views Hot=HotSpots Peaks=Peaks Signal=Signal\n");

/* Write out cell types as subGroup2 */
writeCellSubgroup(f, "subGroup2");

/* Write out treatment as subGroup3. Also create hash with underbarred values for treatments. */
writeTreatmentSubgroup(f, "subGroup3");

if (cellFile)
    {
    writeTissueSubgroup(f, "subGroup4");
    fprintf(f, "subGroup5 cancer Cancer cancer=cancer normal=normal unknown=unknown\n");
    }

/* Write out some more relatively constant parts to finish up root. */
//fprintf(f, "dimensions dimensionY=cellType\n");
fprintf(f, "sortOrder view=+ cellType=+ ");
if (cellFile)
    fprintf(f, "tissue=+ cancer=+ ");
fprintf(f, "treatment=+\n");
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
//fprintf(f, "    pValueFilter 0.0\n");
//fprintf(f, "    pValueFilterLimits 1:324\n");
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
    writeLongLabel(f, "DNAseI HS HotSpots from UW/ENCODE2", col, meta);
    writeSubgroups(f, "Hot", meta->cell, hashFindVal(treatmentHash, meta->treatment));
    fprintf(f, "\ttype bigBed\n");
    fprintf(f, "\tbigDataUrl data/%s.pooled.broadPeak\n", acc);
    fprintf(f, "\tcolor %d,%d,%d\n", lighten(col->r), lighten(col->g), lighten(col->b));
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
//fprintf(f, "    pValueFilter 0.0\n");
//fprintf(f, "    pValueFilterLimits 1:324\n");
fprintf(f, "    scoreFilter 0\n");
fprintf(f, "    scoreFilterLimits 0:1000\n");
fprintf(f, "\n");

/* Write out peak subtracks. */
for (col = colorList; col != NULL; col = col->next)
    {
    char *acc = fileToAcc(col->wigFile);
    meta = hashMustFindVal(metaHash, acc);
    fprintf(f, "\ttrack %sPeaksW%s\n", rootName, acc+1);
    fprintf(f, "\tparent %sPeaks off\n", rootName);
    fprintf(f, "\tshortLabel %s Pk\n", col->label);
    writeLongLabel(f, "DNAseI HS Peaks from UW/ENCODE2", col, meta);
    writeSubgroups(f, "Peaks", meta->cell, hashFindVal(treatmentHash, meta->treatment));
    fprintf(f, "\ttype bigBed\n");
    fprintf(f, "\tbigDataUrl data/%s.pooled.narrowPeak\n", acc);
    fprintf(f, "\tcolor %d,%d,%d\n", lighten(col->r), lighten(col->g), lighten(col->b));
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
    writeLongLabel(f, "DNAseI HS Signal from UW/ENCODE2", col, meta);
    writeSubgroups(f, "Signal", meta->cell, hashFindVal(treatmentHash, meta->treatment));

    /* Figure out bounds of bigWig */
    struct bbiSummaryElement *sum = hashMustFindVal(wigSumHash, acc);
    fprintf(f, "\ttype bigWig %g %g\n", sum->minVal, sum->maxVal);
    fprintf(f, "\tbigDataUrl data/%s.norm.bw\n", acc);
    fprintf(f, "\tcolor %d,%d,%d\n", lighten(col->r), lighten(col->g), lighten(col->b));
    fprintf(f, "\n");
    }

/** Write out container multiWig track */
char multiName[128];
safef(multiName, sizeof(multiName), "%sWig", rootName);
fprintf(f, "track %s\n", multiName);
fprintf(f, "shortLabel UW DNAse Col\n");
fprintf(f, "longLabel UW DNAse Hypersensitivity Signal Colored by Similarity\n");
fprintf(f, "visibility full\n");
fprintf(f, "type bigWig 0 10000\n");
fprintf(f, "container multiWig\n");
fprintf(f, "configurable on\n");
fprintf(f, "viewLimits 0:200\n");
fprintf(f, "showSubtrackColorOnUi on\n");
fprintf(f, "aggregate transparentOverlay\n");
writeCellSubgroup(f, "subGroup1");
writeTreatmentSubgroup(f, "subGroup2");
if (cellFile)
    {
    writeTissueSubgroup(f, "subGroup3");
    fprintf(f, "subGroup4 cancer Cancer cancer=cancer normal=normal unknown=unknown\n");
    }
fprintf(f, "sortOrder cellType=+ ");
if (cellFile)
    fprintf(f, "tissue=+ cancer=+ ");
fprintf(f, "treatment=+\n");
fprintf(f, "\n");

/* Write out colored wig components */
for (col = colorList; col != NULL; col = col->next)
    {
    char *acc = fileToAcc(col->wigFile);
    meta = hashMustFindVal(metaHash, acc);
    fprintf(f, "\ttrack %sW%s\n", multiName, acc+1);
    fprintf(f, "\tparent %s off\n", multiName);
    fprintf(f, "\tshortLabel %s Sg\n", col->label);
    writeLongLabel(f, "DNAseI HS Signal from UW/ENCODE2", col, meta);
    writeSubgroups(f,  NULL, meta->cell, hashFindVal(treatmentHash, meta->treatment));
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

/*verbose(1, "Loaded %d from %s and %d from %s\n", 
    slCount(metaList), metaFile, slCount(colorList), colorFile);
    */

struct wgEncodeCell *cellList = NULL;
if (cellFile != NULL)
    {
    cellList = wgEncodeCellLoadAllByChar(cellFile, '\t');
    //verbose(1, "Loaded %d from %s\n", slCount(cellFile), cellFile);
    }

/* Make output directory and text files */
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s", outDir, assembly);
makeDirsOnPath(path);
safef(path, sizeof(path), "%s/%s", outDir, "hub.txt");
writeHubTxt(path);
safef(path, sizeof(path), "%s/%s", outDir, "genomes.txt");
writeGenomesTxt(path);
safef(path, sizeof(path), "%s/%s/trackDb.txt", outDir, assembly);
writeTrackDbTxt(path, metaList, colorList, cellList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
cellFile = optionVal(CELL_FILE_OPTION, NULL);
uwDnaseTrackDb(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
