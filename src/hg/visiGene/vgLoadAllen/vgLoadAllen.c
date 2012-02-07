/* vgLoadAllen - Create .ra and .tab files for loading Allen Brain Atlas images into VisiGene. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "portable.h"
#include "jpegSize.h"
#include "dnaseq.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgLoadAllen - Create .ra and .tab files for loading Allen Brain Atlas images\n"
  "into VisiGene\n"
  "usage:\n"
  "   vgLoadAllen sourceImageDir allen.tab probes.fa name.tab outDir\n"
  "where sourceImageDir is the full-size jpg image dir converted from the ABA image Disk\n"
  "allen.tab is the ABA input \n"
  "probes.fa contains the sequence of all the probes\n"
  "name.tab is two columns: biological name and the aba probe id (used with the gene sorter)\n"
  "outputDir is where the output .ra and tab files go.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/*  
 vgLoadAllen 
   sourceImageDir = /san/sanvol1/visiGene/gbdb/full/inSitu/Mouse/allenBrain 
     (if san is down can use /cluster/store11/visiGene/offline/allenBrain/imageDisk with .jp2)
   allen.tab     = /cluster/store11/visiGene/offline/allenBrain/probesAndData/allen20051021.tab 
   probes.fa      = /cluster/data/mm7/bed/allenBrain/allProbes.fa 
   name.tab       = /cluster/data/mm7/bed/allenBrain/allProbes.tab
   outDir         = output

to map refSeq to RP_ probe id used in track, use this:
   /cluster/data/mm7/bed/allenBrain/allProbes.tab
   NM_026675       RP_040914_02_A02
   NM_133764       RP_050317_04_H06

/cluster/data/mm7/bed/allenBrain> head allProbes.fa
>RP_040914_02_A02
TTGGAGAGCTGCCCTGTCAGACTATGGACCCCGAGGTGTCCCTGCTGCTG

Assumes that sourceImageDir has .jp2 files in subdirs,
but only one level deep.

*/   

static struct optionSpec options[] = {
   {NULL, 0},
};


struct hash *makeImageHash(char *sourceImageDir)
/* look in each subdir for .jpg files
 * but only look in subdirs, and only one level deep.
 * hash key is the gene name which is the first part of filename up to "_" 
 * and the hash value is the relative path to the file from sourceImageDir. */
{
struct hash *hash = newHash(0);
struct fileInfo *dList = NULL, *dEntry;
dList = listDirX(sourceImageDir, "*", FALSE);
for (dEntry = dList; dEntry != NULL; dEntry = dEntry->next)
    {
    if (dEntry->isDir)
	{
	char newDir[256];
	struct fileInfo *fList = NULL, *fEntry;
	safef(newDir,sizeof(newDir),"%s/%s",sourceImageDir,dEntry->name);
	fList = listDirX(newDir, "*.jpg", FALSE);   
	for (fEntry = fList; fEntry != NULL; fEntry = fEntry->next)
	    {
	    char newPath[256];
	    char *underBar=NULL;
	    safef(newPath,sizeof(newPath),"%s/%s",dEntry->name,fEntry->name);
	    underBar = strchr(fEntry->name,'_');
	    if (underBar)
		{
		char *key = cloneStringZ(fEntry->name,underBar-fEntry->name);
		char *val = cloneString(newPath);
    		hashAdd(hash, key, val);
		verbose(2, "imageHash key=%s value=%s\n", key, val);
		}
	    }
	slFreeList(&fList);
	}
    }
slFreeList(&dList);    
return hash;
}


struct hash *makeNameHash(char *fileName)
/* Parse file in format:
 *    name  probeId
 * into hash keyed by probeId with name values. */
{
struct hash *hash = newHash(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];

while (lineFileRow(lf, row))
    {
    hashAdd(hash, cloneString(row[0]), cloneString(row[1]));
    }
lineFileClose(&lf);
return hash;
}

void writeRa(char *fileName)
/* Write our .ra file with information common to all NIBB images. */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "submitSet abaMouse1\n");
fprintf(f, "fullDir http://hgwdev.cse.ucsc.edu/visiGene/full/inSitu/Mouse/allenBrain\n");
fprintf(f, "thumbDir http://hgwdev.cse.ucsc.edu/visiGene/200/inSitu/Mouse/allenBrain\n");
fprintf(f, "itemUrl http://www.brain-map.org/search.do?queryText=%%s\n");
fprintf(f, "priority 1200\n");
fprintf(f, "sliceType sagittal\n");
fprintf(f, "submissionSource Allen Brain Atlas (ABA)\n");
fprintf(f, "taxon 10090\n");
fprintf(f, "genotype wild type\n");
fprintf(f, "strain C57BL/6\n");
fprintf(f, "age 56\n");
fprintf(f, "sex male\n");
fprintf(f, "bodyPart brain\n");
fprintf(f, "contributor Boe A.,Dang C.,Jeung D.,Luong L.,Sunkin S.,Sutram M.,Youngstrom B.,Lein E.,Jones A.,ABA ABA,ABI ABI,Allen Institute for Brain Science,Allen Brain Atlas,\n");
fprintf(f, "acknowledgement "
    "Thanks to the Allen Institute for Brain "
    "Science, particularly to Andrew Boe, Chinh Dang, Darren Jeung, Lon "
    "Luong, Susan Sunkin, Madhavi Sutram, and Brian Youngstrom for helping "
    "make these images available in VisiGene."
);
/* Still need to fill in contributor, publication, journal, journalUrl */
fprintf(f, "contributor Allen Institute for Brain Science\n");
fprintf(f, "year 2006\n");
fprintf(f, "setUrl http://www.brainatlas.org/\n");
fprintf(f, "copyright &copy; 2004-2006 Allen Institute for Brain Science\n");
fprintf(f, "probeColor blue/purple\n");
carefulClose(&f);
}



void writeTab(
	struct hash *imageHash, 
	struct hash *seqHash, 
	char *sourceImageDir, char *allenTab, 
	struct hash *nameHash, char *outName)
/* Synthesize data and write out tab-separated file with one line for
 * each image. */
{
char sourceImage[PATH_LEN];
FILE *f = mustOpen(outName, "w");
struct lineFile *lf = lineFileOpen(allenTab, TRUE);
char *row[5];

/* Write header. */
fprintf(f, "#");
fprintf(f, "gene\t");
fprintf(f, "refSeq\t");
fprintf(f, "locusLink\t"); 
fprintf(f, "submitId\t");    /* egeneid=68323 or genesym=1110003F05Rik */
fprintf(f, "fileName\t");
fprintf(f, "imageWidth\t");
fprintf(f, "imageHeight\t");
fprintf(f, "probeId\t");     /* actually, this not supported yet but would be great. */
fprintf(f, "seq\n");

while (lineFileRowTab(lf, row))
    {
    char *gene = row[0];
    /* char *geneName = row[1]; */
    char *entrez = row[2];
    char *refSeq = row[3];
    char *url = row[4];
    char *probeId = hashFindVal(nameHash, refSeq);
    struct dnaSeq *seq = NULL; 
    int width=0, height=0;
    char *relPath = hashFindVal(imageHash, gene);
    char *submitId = strchr(url,'='); 
    if (probeId)
    	seq = hashFindVal(seqHash, probeId);
    if (submitId)
	++submitId;  /* we want the string following first '=' */
    if (sameString(entrez,"0"))
	entrez = NULL;

    if (relPath)
	{
	safef(sourceImage, sizeof(sourceImage), "%s/%s", 
	    sourceImageDir, relPath);
	jpegSize(sourceImage, &width, &height);

	fprintf(f, "%s\t", gene);
	fprintf(f, "%s\t", refSeq);
	fprintf(f, "%s\t", entrez?entrez:"");
	fprintf(f, "%s\t", submitId);
	fprintf(f, "%s\t", relPath);
	fprintf(f, "%d\t", width);
	fprintf(f, "%d\t", height);
	fprintf(f, "%s\t", probeId?probeId:"");
	if (seq != NULL)
	    fprintf(f, "%s\n", seq->dna);
	else
	    fprintf(f, "\n");
	}
    }
lineFileClose(&lf);    
carefulClose(&f);
}

void vgLoadAllen(char *sourceImageDir, char *allenTab, char *probesFa,
	char *nameTab, char *outDir)
/* vgLoadAllen - Create .ra and .tab files for loading Xenopus images from NIBB 
 * into VisiGene. */
{

struct hash *imageHash = makeImageHash(sourceImageDir);
struct hash *nameHash =  makeNameHash(nameTab);
struct hash *seqHash = dnaSeqHash(faReadAllDna(probesFa));
char outPath[PATH_LEN];

verbose(1, "Got %d images\n", imageHash->elCount);
verbose(1, "Got %d named probes\n", nameHash->elCount);
verbose(1, "Got %d probe sequences\n", seqHash->elCount);

/* Make output directory and ra file. */
makeDir(outDir);
safef(outPath, sizeof(outPath), "%s/%s", outDir, "aba.ra");
writeRa(outPath);

/* Make tab separated file. */
safef(outPath, sizeof(outPath), "%s/%s", outDir, "aba.tab");
writeTab(imageHash, seqHash, sourceImageDir, allenTab, nameHash, outPath);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
vgLoadAllen(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;

}
