/* nibbImageProbes - Collect image probes for NIBB Xenopus Laevis in-situs. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "dnaseq.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "nibbImageProbes - Collect image probes for NIBB Xenopus Laevis in-situs\n"
  "usage:\n"
  "   nibbImageProbes imageDir i.fa f.fa r.fa out.fa out.map\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct imageInfo
/* Information on an info. */
    {
    struct imageInfo *next;
    char *xlName;		/* Shortened XL name */
    char *imageDir;		/* Name of associated directory. */
    char *probeName;		/* Name of probe in sequence file. */
    };

struct hash *hashImageDirs(char *rootDir, struct imageInfo **pList)
/* Make hash of image directories keyed by something
 * like XL010i18.   */
{
struct hash *hash = hashNew(0);
struct imageInfo *info;
int imageCount = 0;
struct fileInfo *outerDir, *outerDirList = listDirX(rootDir, "XL*", TRUE);

for (outerDir = outerDirList; outerDir != NULL; outerDir = outerDir->next)
    {
    struct fileInfo *innerDir, *innerDirList;
    innerDirList = listDirX(outerDir->name, "XL*", FALSE);
    for (innerDir = innerDirList; innerDir != NULL; innerDir = innerDir->next)
        {
	char *e = strchr(innerDir->name, '_');
	AllocVar(info);
	if (e == NULL)
	    errAbort("Can't find underbar in %s", innerDir->name);
	*e = 0;
	hashAddSaveName(hash, innerDir->name, info, &info->xlName);
	*e = '_';
	info->imageDir = innerDir->name;
	slAddHead(pList, info);
	imageCount += 1;
	}
    }
verbose(1, "%d image dirs in %s\n", imageCount, rootDir);
slReverse(pList);
return hash;
}

struct hashEl *findImageInfo(struct hash *hash, char *seqName)
/* Snoop around in hash for sequence name, and return
 * associated dir if found. */
{
struct hashEl *hel = NULL;
char *s = stringIn("XL", seqName);
if (s != NULL)
    {
    hel = hashLookup(hash, s);
    if (hel == NULL)
        {
	char buf[16];
	memcpy(buf, s, 8);
	hel = hashLookup(hash, s);
	}
    }
else
    {
    hel = hashLookup(hash, seqName);
    }
return hel;
}

void searchFastaForProbes(char *faFile, struct hash *imageHash, FILE *fFa)
/* Scan fasta file and where possible fill imageInfo in imageHash. */
{
struct lineFile *lf = lineFileOpen(faFile, TRUE);
DNA *dna;
int size;
char *name;

while (faSpeedReadNext(lf, &dna, &size, &name))
    {
    struct hashEl *imageEl;
    imageEl = findImageInfo(imageHash, name);
    if (imageEl != NULL)
        {
	struct imageInfo *info = imageEl->val;
	if (info->probeName == NULL)
	    faWriteNext(fFa, info->xlName, dna, size);
	for (;imageEl != NULL; imageEl = hashLookupNext(imageEl))
	    {
	    info = imageEl->val;
	    info->probeName = cloneString(name);
	    }
	}
    }
lineFileClose(&lf);
}

void nibbImageProbes(char *imageDir, char *iFile, char *fFile, char *rFile,
	char *outFa, char *outMap)
/* nibbImageProbes - Collect image probes for NIBB Xenopus Laevis in-situs. */
{
struct imageInfo *infoList = NULL, *info;
struct hash *imageHash = hashImageDirs(imageDir, &infoList);
FILE *fFa = mustOpen(outFa, "w");
FILE *fTab = mustOpen(outMap, "w");

searchFastaForProbes(iFile, imageHash, fFa);
searchFastaForProbes(fFile, imageHash, fFa);
searchFastaForProbes(rFile, imageHash, fFa);

for (info = infoList; info != NULL; info = info->next)
    {
    fprintf(fTab, "%s\t%s\t%s\n", naForNull(info->xlName), 
    	info->imageDir, naForNull(info->probeName));
    }
carefulClose(&fFa);
carefulClose(&fTab);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
nibbImageProbes(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
