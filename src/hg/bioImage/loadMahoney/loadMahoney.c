/* loadMahoney - Convert Paul Gray/Mahoney in situs into something bioImageLoad can handle. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "mahoney.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "loadMahoney - Convert Paul Gray/Mahoney in situs into something bioImageLoad can handle\n"
  "usage:\n"
  "   loadMahoney /gbdb/bioImage input.tab outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void raCommon(FILE *f)
/* Write parts of ra common for whole mounts and slices" */
{
fprintf(f, "%s",
"taxon 10090\n"
"imageType RNA in situ\n"
"contributor Gray P.A., Fu H., Luo P., Zhao Q., Yu J., Ferrari A., Tenzen T., Yuk D., Tsung E.F., Cai Z., Alberta J.A., Cheng L., Liu Y., Stenman J.M., Valerius M.T., Billings N., Kim H.A., Greenberg M.E., McMahon A.P., Rowitch D.H., Stiles C.D., Ma Q.\n"
"publication Mouse brain organization revealed through direct genome-scale TF expression analysis. \n"
"pubUrl http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Retrieve&db=pubmed&dopt=Abstract&list_uids=15618518\n"
"setUrl http://mahoney.chip.org/mahoney/\n"
"itemUrl http://mahoney.chip.org/servlet/Mahoney2.ShowTF?mTF_ID=%s\n"
);
}

void wholeMountRa(char *fileName)
/* Output whole mount ra file. */
{
FILE *f = mustOpen(fileName, "w");

fprintf(f, "%s",
"fullDir ../bioImage/full/inSitu/Mouse/mahoney/wholeMount\n"
"screenDir ../bioImage/700/inSitu/Mouse/mahoney/wholeMount\n"
"thumbDir ../bioImage/200/inSitu/Mouse/mahoney/wholeMount\n"
"priority 10\n"
"isEmbryo 1\n"
"age 10.5\n"
"bodyPart whole\n"
"sliceType whole mount\n"
);

raCommon(f);
carefulClose(&f);
}

void slicesRa(char *fileName)
/* Output slices ra file. */
{
FILE *f = mustOpen(fileName, "w");

fprintf(f, "%s",
"fullDir ../bioImage/full/inSitu/Mouse/mahoney/slices\n"
"screenDir ../bioImage/700/inSitu/Mouse/mahoney/slices\n"
"thumbDir ../bioImage/200/inSitu/Mouse/mahoney/slices\n"
"priority 100\n"
"bodyPart brain\n"
"sliceType transverse\n"
);

raCommon(f);
carefulClose(&f);
}

void wholeMountTab(struct slName *inList, struct hash *mahoneyHash,
	char *outFile)
/* Output tab-delimited info on each item in inList. */
{
struct slName *in;
FILE *f = mustOpen(outFile, "w");
fprintf(f, "#fileName\tsubmitId\tgene\tlocusLink\trefSeq\tgenbank\ttreatment\n");
for (in = inList; in != NULL; in = in->next)
    {
    char hex[8];
    char mtf[5];
    char treatment;
    int id;
    struct mahoney *m;
    strncpy(mtf, in->name, 4);
    mtf[4] = 0;
    treatment = in->name[4];
    id = atoi(mtf);
    safef(hex, sizeof(hex), "%x", id);
    m = hashFindVal(mahoneyHash, hex);
    if (m == NULL)
        warn("Can't find %d (%s) in hash while processing %s", id, mtf, in->name);
    else
	{
	fprintf(f, "%s\t%d\t", in->name, id);
	fprintf(f, "%s\t", m->geneName);
	fprintf(f, "%s\t", m->locusId);
	if (startsWith("NM_", m->genbank))
	    fprintf(f, "%s\t\t", m->genbank);
	else
	    fprintf(f, "\t%s\t", m->genbank);
	if (treatment == 'a')
	    fprintf(f, "3 min proteinase K");
	else if (treatment == 'b')
	    fprintf(f, "30 min proteinase K");
	else if (treatment == 'd')
	    fprintf(f, "3 min proteinase K/30 min proteinase K");
	else if (treatment == 'c' || treatment == '.')
	    /* do nothing*/ ;
	else
	    errAbort("Unrecognized treatment %c in %s", treatment, in->name);
	fprintf(f, "\n");
	}
    }
carefulClose(&f);
}

void processWholeMounts(struct hash *mahoneyHash, 
	char *inFull, char *outDir)
/* Output database files for whole mounts. */
{
struct slName *imageFileList = NULL, *imageFile;
char outRa[PATH_LEN], outTab[PATH_LEN];

imageFileList = listDir(inFull, "*.jpg");
safef(outRa, sizeof(outRa), "%s/whole.ra", outDir);
wholeMountRa(outRa);
safef(outTab, sizeof(outRa), "%s/whole.tab", outDir);
wholeMountTab(imageFileList, mahoneyHash, outTab);
}

void slicesTab(struct slName *inList, struct hash *mahoneyHash,
	char *outFile)
/* Output tab-delimited info on each item in inList. */
{
struct slName *in;
FILE *f = mustOpen(outFile, "w");
char lastStage = 0;
fprintf(f, "#fileName\tsubmitId\tgene\tlocusLink\trefSeq\tgenbank\tisEmbryo\tage\tsectionSet\tsectionIx\n");
for (in = inList; in != NULL; in = in->next)
    {
    char hex[8];
    char mtf[5];
    char stage = '1', sliceNo, nextSliceNo = '1';
    int id;
    struct mahoney *m;
    strncpy(mtf, in->name+1, 4);
    mtf[4] = 0;
    id = atoi(mtf);
    safef(hex, sizeof(hex), "%x", id);
    m = hashFindVal(mahoneyHash, hex);
    if (m == NULL)
        warn("Can't find %d (%s) in hash while processing %s", id, mtf, in->name);
    else
        {
	stage = in->name[5];
	sliceNo = in->name[6];
	fprintf(f, "%s\t%d\t", in->name, id);
	fprintf(f, "%s\t", m->geneName);
	fprintf(f, "%s\t", m->locusId);
	if (startsWith("NM_", m->genbank))
	    fprintf(f, "%s\t\t", m->genbank);
	else
	    fprintf(f, "\t%s\t", m->genbank);
	if (stage == '1')
	    fprintf(f, "1\t13.5\t");
	else if (stage == '2')
	    fprintf(f, "0\t0\t");
	else
	    errAbort("Unrecognized stage %c in %s", stage, in->name);
	fprintf(f, "0\t0\n");
	}
    }
carefulClose(&f);
}



void processSlices(struct hash *mahoneyHash, 
	char *inFull, char *outDir)
/* Output database files for slices. */
{
struct slName *imageFileList = NULL, *imageFile;
char outRa[PATH_LEN], outTab[PATH_LEN];

imageFileList = listDir(inFull, "*.jpg");
safef(outRa, sizeof(outRa), "%s/slices.ra", outDir);
slicesRa(outRa);
safef(outTab, sizeof(outRa), "%s/slices.tab", outDir);
slicesTab(imageFileList, mahoneyHash, outTab);
}


struct hash *hashMahoneys(struct mahoney *list)
/* Put list of mahoneys into hash keyed by mahoney id. */
{ 
struct hash *hash = hashNew(0);
struct mahoney *el;
for (el = list; el != NULL; el = el->next)
    {
    char hex[8];
    touppers(el->genbank);
    safef(hex, sizeof(hex), "%x", el->mtf);
    hashAdd(hash, hex, el);
    }
return hash;
}

struct mahoney *loadMahoneyList(char *fileName)
/* Load data from tab-separated file (not whitespace separated) */
{
struct mahoney *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[MAHONEY_NUM_COLS];
while (lineFileNextRowTab(lf, row, ArraySize(row)))
    {
    el = mahoneyLoad(row);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void loadMahoney(char *bioImageDir, char *inTab, char *outDir)
/* loadMahoney - Convert Paul Gray/Mahoney in situs into something bioImageLoad can handle. */
{
struct mahoney *mahoneyList = loadMahoneyList(inTab);
struct hash *hash = hashMahoneys(mahoneyList);
char inFull[PATH_LEN];
char *mahoneyPath = "inSitu/Mouse/mahoney";
char inWholeFull[PATH_LEN], inSlicesFull[PATH_LEN];
safef(inFull, sizeof(inFull), "%s/full", bioImageDir);
safef(inWholeFull, sizeof(inWholeFull), "%s/%s/%s", inFull, mahoneyPath, "wholeMount");
safef(inSlicesFull, sizeof(inSlicesFull), "%s/%s/%s", inFull, mahoneyPath, "slices");
makeDir(outDir);
processWholeMounts(hash, inWholeFull, outDir);
processSlices(hash, inSlicesFull, outDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
loadMahoney(argv[1], argv[2], argv[3]);
return 0;
}
