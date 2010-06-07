/* vgLoadNibb - Create .ra and .tab files for loading Xenopus images from 
 * NIBB into VisiGene. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "portable.h"
#include "jpegSize.h"
#include "dnaseq.h"
#include "fa.h"

static char const rcsid[] = "$Id: vgLoadNibb.c,v 1.9 2008/04/04 22:57:02 galt Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgLoadNibb - Create .ra and .tab files for loading Xenopus images from\n"
  "NIBB into VisiGene\n"
  "usage:\n"
  "   vgLoadNibb sourceImageDir parsed.tab probes.fa name.tab stage.tab outDir\n"
  "where sourceImageDir is the image dir originally loaded from the NIBB\n"
  "CDs,  parsedTab is the output of nibbParseImageDir, \n"
  "probes.fa contains the sequence of all the probes\n"
  "name.tab is two columns with a biological name in the first column\n"
  "and the nibb probe id in the second column (created with the gene sorter)\n"
  "stage.tab is two columns with the stage name in the first and the age of\n"
  "in hours in the second column, and outputDir is where the output .ra and tab\n"
  "files go.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *makeNameHash(char *fileName)
/* Parse file in format:
 *    name  probeId,probeId,
 * into hash keyed by probeId with name values. */
{
struct hash *hash = newHash(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];

while (lineFileRow(lf, row))
    {
    struct slName *probe, *probeList = commaSepToSlNames(row[1]);
    for (probe = probeList; probe != NULL; probe = probe->next)
	hashAdd(hash, probe->name, cloneString(row[0]));
    slFreeList(&probeList);
    }
lineFileClose(&lf);
return hash;
}

struct hash *makeStageHash(char *fileName)
/* Return hash with keys that are stage names (st12 and the like)
 * and values are ascii strings describing age in days. 
 * The input is two columns - stage name, and age in hours. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
struct hash *hash = hashNew(0);

while (lineFileRow(lf, row))
    {
    char *stage = row[0];
    double hours = atof(row[1]);
    char days[16];
    safef(days, sizeof(days), "%f", hours/24);
    hashAdd(hash, stage, cloneString(days));
    }
lineFileClose(&lf);
return hash;
}

void writeRa(char *fileName)
/* Write our .ra file with information common to all NIBB images. */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "submitSet nibbXenopusLaevis3\n");
fprintf(f, "fullDir ../visiGene/full/inSitu/XenopusLaevis/nibb\n");
fprintf(f, "thumbDir ../visiGene/200/inSitu/XenopusLaevis/nibb\n");
fprintf(f, "priority 1200\n");
fprintf(f, "sliceType whole mount\n");
fprintf(f, "submissionSource National Institute of Basic Biology (NIBB) XDB\n");
fprintf(f, "taxon 8355\n");
fprintf(f, "genotype wild type\n");
fprintf(f, "acknowledgement Thanks to Naoto Ueno and colleagues at NIBB for helping make these images available in VisiGene\n");

/* Still need to fill in contributor, publication, journal, journalUrl, itemUrl */
fprintf(f, "contributor Ueno N., Kitayama A., Terasaka C., Nomoto K., Shibamoto K., Nishide H.\n");
fprintf(f, "year 2005\n");
fprintf(f, "setUrl http://xenopus.nibb.ac.jp\n");
fprintf(f, "itemUrl http://xenopus.nibb.ac.jp/cgi-bin/search?query=%%s&name=clone\n");
fprintf(f, "probeColor purple\n");
carefulClose(&f);
}



void writeTab(struct hash *stageHash, struct hash *seqHash, 
	char *sourceImageDir, char *parsedTab, 
	struct hash *nameHash, char *outName)
/* Synthasize data and write out tab-separated file with one line for
 * each image. */
{
char sourceImage[PATH_LEN];
FILE *f = mustOpen(outName, "w");
struct lineFile *lf = lineFileOpen(parsedTab, TRUE);
char *row[6];

/* Write header. */
fprintf(f, "#");
fprintf(f, "gene\t");
fprintf(f, "submitId\t");
fprintf(f, "fileName\t");
fprintf(f, "imageWidth\t");
fprintf(f, "imageHeight\t");
fprintf(f, "bodyPart\t");
fprintf(f, "age\t");
fprintf(f, "minAge\t");
fprintf(f, "maxAge\t");
fprintf(f, "seq\n");

while (lineFileRowTab(lf, row))
    {
    char *clone = row[0];
    char *stage = row[1];
    char *part = row[2];
    char *dir = row[3];
    char *subdir = row[4];
    char *file = row[5];
    char *gene = hashFindVal(nameHash, clone);
    struct dnaSeq *seq = hashFindVal(seqHash, clone);
    int width, height;

    safef(sourceImage, sizeof(sourceImage), "%s/%s/%s/%s",
    	sourceImageDir, dir, subdir, file);
    jpegSize(sourceImage, &width, &height);

    if (gene == NULL)
        gene = clone;
    fprintf(f, "%s\t", gene);
    fprintf(f, "%s\t", clone);
    fprintf(f, "%s/%s\t", subdir, file);
    fprintf(f, "%d\t", width);
    fprintf(f, "%d\t", height);
    fprintf(f, "%s\t", part);
    if (sameString(stage, "mixed"))
	fprintf(f, "1\t0\t3\t");
    else
	{
	char *age = hashMustFindVal(stageHash, stage);
	fprintf(f, "%s\t", age);
	fprintf(f, "%s\t", age);
	fprintf(f, "%s\t", age);
	}
    if (seq != NULL)
	fprintf(f, "%s\n", seq->dna);
    else
        fprintf(f, "\n");
    }

carefulClose(&f);
}

void vgLoadNibb(char *sourceImageDir, char *parsedTab, char *probesFa,
	char *nameTab, char *stageTab, char *outDir)
/* vgLoadNibb - Create .ra and .tab files for loading Xenopus images from NIBB 
 * into VisiGene. */
{
struct hash *nameHash = makeNameHash(nameTab);
struct hash *seqHash = dnaSeqHash(faReadAllDna(probesFa));
struct hash *stageHash = makeStageHash(stageTab);
char outPath[PATH_LEN];

verbose(1, "Got %d named probes\n", nameHash->elCount);
verbose(1, "Got %d probe sequences\n", seqHash->elCount);
verbose(1, "Got %d stages\n", stageHash->elCount);

/* Make output directory and ra file. */
makeDir(outDir);
safef(outPath, sizeof(outPath), "%s/%s", outDir, "nibb.ra");
writeRa(outPath);

/* Make tab separated file. */
safef(outPath, sizeof(outPath), "%s/%s", outDir, "nibb.tab");
writeTab(stageHash, seqHash, sourceImageDir, parsedTab, nameHash, outPath);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
vgLoadNibb(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
