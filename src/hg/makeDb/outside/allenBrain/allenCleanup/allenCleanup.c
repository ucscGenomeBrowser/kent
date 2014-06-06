/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* allenCleanup - Clean up the .tab file and image files before running allenCollectSeq and vgLoadAllen 
 *    Each image must exist, must have tiles, must have a unique gene-name (up to the first "_"),
 *    and must have a unique refseq value.  I could have also checked the prboe sequences,
 *    but we are not having problems with those.  Since this system depends heavily on refseq
 *    and nearly all have a refseq, we simply filter out and remove any that have blank or
 *    "unknown" for refseq.  We also can find and remove unused or duplicate gene-images.
 *
 *    So refseq is required and unique, the gene-name/image must uniquely exist.
 *    We are also checking that the thumbnails and tiles were properly made and are not missing.
 *
 *    Various things are reported: duplicate rows, non-unique values, unused images.
 *    
 *    The cleanup on the output tab is automatic, but the cleanup of the duplicate and unused images
 *    takes place only when -cleanup option is specified.  Unwanted .jp2 files are moved to ../dupes/
 *    and unwanted .jpg files/tiles/thumbs in sourceImageDir are deleted.
 */
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
  "allenCleanup - Clean up the .tab file and image files before running allenCollectSeq and vgLoadAllen\n"
  "usage:\n"
  "   allenCleanup imageDisk sourceImageDir allen.tab output.tab\n"
  "where imageDisk is the origianl jp2 (jpeg2000) ABA image dir\n"
  "sourceImageDir is the full-size jpg image dir converted from the ABA image Disk\n"
  "allen.tab is the input to be cleaned up, \n"
  "output.tab is the cleaned up allen output.\n"
  "options:\n"
  "   -cleanImages - causes real clean-up of images\n"
  "   -checkImages - check for all images - full, thumb, tiles\n"
  );
}

/*  
 allenCleanup
   imageDisk      = /san/sanvol1/visiGene/offline/allenBrain/imageDisk
   sourceImageDir = /san/sanvol1/visiGene/gbdb/full/inSitu/Mouse/allenBrain 
   allen.tab      = /cluster/store11/visiGene/offline/allenBrain/imageDisk/May_06/allen20061204combined.tab 
   output.tab     = /cluster/store11/visiGene/offline/allenBrain/probesAndData/allen20061204.tab 

Assumes that sourceImageDir has .jp2 files in subdirs,
but only one level deep.

*/   

bool cleanImages = FALSE;
bool checkImages = FALSE;

static struct optionSpec options[] = {
   {"cleanImages", OPTION_BOOLEAN},
   {"checkImages", OPTION_BOOLEAN},
   {NULL, 0},
};



void checkOutImages(char *sourcePath, char *relPath)
/* check for full-image/tiles/thumb from sourcePath 
 * relPath is "subdir/file.jp2" */
{
int width=0, height=0;
char outPath[256], dir[256], name[128], extension[64];
splitPath(relPath, dir, name, extension);

/* see if there is a full-size downloadable image */
safef(outPath,sizeof(outPath),"%s/%s%s%s",sourcePath,dir,name,".jpg");
boolean fe = fileExists(outPath);
if (!fe)
    {
    printf("missing full image (%s)\n",outPath);
    return;
    }
unsigned long long size = fileSize(outPath);
if (size < 256)
    {
    printf("truncated full image (%s) size=%llu\n", outPath, size);
    return;
    }
jpegSize(outPath, &width, &height);

/* see if there is a tiles dir */
safef(outPath,sizeof(outPath),"%s/%s%s",sourcePath,dir,name);
fe = fileExists(outPath);
if (!fe)
    {
    printf("missing tiles dir (%s)\n",outPath);
    return;
    }

/* see if all tiles are there */
char expected[256];
int eLevel = 0;
int eWidth = width;
int eHeight = height;
int eTiles = ((eWidth+511)/512)*((eHeight+511)/512);
int eTile = 0;
struct fileInfo *dList = NULL, *dEntry;
dList = listDirX(outPath, "*.jpg", FALSE);
for (dEntry = dList; dEntry != NULL; dEntry = dEntry->next)
    {
    safef(expected,sizeof(expected),"_%01d_%03d.jpg",eLevel,eTile);
    if (!endsWith(dEntry->name,expected))
	{
    	printf("warning: sourcePath (%s) relPath (%s)\n", sourcePath, relPath);
    	printf("warning: jpegSize width=(%d) height=(%d)\n",width,height);
	printf("warning: tile (%s) expected(%s)\n",dEntry->name, expected);
	break;
	}
    ++eTile;
    if (eTile >= eTiles)
	{
	++eLevel;
	eWidth = eWidth/2;
	eHeight = eHeight/2;
	eTiles = ((eWidth+511)/512)*((eHeight+511)/512);
	eTile = 0;
	}
    }
slFreeList(&dList);    

/* see if there is a thumbnail */
safef(outPath,sizeof(outPath),"%s/%s%s%s",sourcePath,dir,name,".jpg");
char *thumb = replaceChars(outPath,"/full/","/200/");
fe = fileExists(thumb);
if (!fe)
    {
    printf("missing thumbnail (%s)\n",thumb);
    return;
    }
freez(&thumb);

verbose(3,"\n");  

}


void cleanOutImages(char *imageDisk, char *sourcePath, char *relPath, char *hideDir)
/* move to dupe or unused in imageDisk, 
 * delete image/tiles/thumb from sourcePath 
 * relPath is "subdir/file.jp2" 
 * hideDir is either "unused" or "dupes" */
{
char outPath[256], oldPath[256], dir[256], name[128], extension[64];
splitPath(relPath, dir, name, extension);
safef(outPath,sizeof(outPath),"%s/%s%s",imageDisk,dir,hideDir); 

/* move file to hideDir subdir  */
verbose(3,"moving to ./%s: outPath=[%s]\n",hideDir,outPath);
boolean fe = fileExists(outPath);
if (!fe)
    {
    makeDir(outPath);
    }
safef(oldPath,sizeof(oldPath),"%s/%s%s%s",imageDisk,dir,name,extension);
safef(outPath,sizeof(outPath),"%s/%s%s/%s%s",imageDisk,dir,hideDir,name,extension);
if (link(oldPath,outPath)==0)
    {
    unlink(oldPath);
    }

/* see if there are any downloadable images to cleanup */
safef(outPath,sizeof(outPath),"%s/%s%s%s",sourcePath,dir,name,".jpg");
fe = fileExists(outPath);
if (fe)
    {
    unlink(outPath);
    }

/* see if there are any tiles to cleanup */
safef(outPath,sizeof(outPath),"%s/%s%s",sourcePath,dir,name);
fe = fileExists(outPath);
if (fe)
    {
    struct fileInfo *dList = NULL, *dEntry;
    dList = listDirX(outPath, "*.jpg", TRUE);
    for (dEntry = dList; dEntry != NULL; dEntry = dEntry->next)
	{
	unlink(dEntry->name);
	}
    slFreeList(&dList);    
    rmdir(outPath);
    }

/* see if there is a thumbnail to cleanup */
safef(outPath,sizeof(outPath),"%s/%s%s%s",sourcePath,dir,name,".jpg");
char *thumb = replaceChars(outPath,"/full/","/200/");
verbose(3,"checking if thumb exists: thumb=[%s]\n",thumb);
fe = fileExists(thumb);
if (fe)
    {
    unlink(thumb);
    }
freez(&thumb);

}

struct hash *makeImageHash(char *imageDisk, int *dupeCount, char *sourcePath)
/* look in each subdir for .jp2 files
 * but only look in subdirs, and only one level deep.
 * hash key is the gene name which is the first part of filename up to "_" 
 * and the hash value is the relative path to the file from imageDisk. */
{
struct hash *hash = newHash(0);
struct fileInfo *dList = NULL, *dEntry;
dList = listDirX(imageDisk, "*", FALSE);
for (dEntry = dList; dEntry != NULL; dEntry = dEntry->next)
    {
    if (dEntry->isDir)
	{
	char newDir[256];
	struct fileInfo *fList = NULL, *fEntry;
	safef(newDir,sizeof(newDir),"%s/%s",imageDisk,dEntry->name);
	fList = listDirX(newDir, "*.jp2", FALSE);   
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
		char *valExisting = hashFindVal(hash, key);
		if (valExisting)
		    {
		    printf("duplicate image: key=%s value=%s (existing=%s)\n", key, val, valExisting);
		    if (cleanImages)
	    		cleanOutImages(imageDisk,sourcePath,val,"dupes");
		    (*dupeCount)++;
		    }
		else
		    {
    		    hashAdd(hash, key, val);
		    verbose(2, "imageHash key=%s value=%s\n", key, val);
		    }
		}
	    }
	slFreeList(&fList);
	}
    }
slFreeList(&dList);    
return hash;
}


void allenCleanup(char *imageDisk, char *sourcePath,
 char *allenTab, char *output)
/* clean up input allen.tab */
{
FILE *f = mustOpen(output, "w");
struct lineFile *lf = lineFileOpen(allenTab, TRUE);
char *row[5];
int refSeqEmpty = 0;
int refSeqUnknown = 0;
struct hash *refSeqHash = newHash(0);
int refSeqDupe = 0;
struct hash *geneSymbolHash = newHash(0); /* actually initial part of image name */
int geneSymbolDupe = 0;
int imageDupe = 0;
struct hash *imageHash = makeImageHash(imageDisk,&imageDupe,sourcePath);
int imageCount = imageHash->elCount;
int imageMissing = 0;
int count = 0;


/* Write header. */
fprintf(f, "#");
fprintf(f, "geneSymbol\t");
fprintf(f, "geneName\t");
fprintf(f, "entrezGeneId\t");
fprintf(f, "refSeqAccessionNumber\t");
fprintf(f, "urlToAtlas\n"); 

while (lineFileRowTab(lf, row))
    {
    char *gene = row[0];
    char *geneName = row[1];
    char *entrez = row[2];
    char *refSeq = row[3];
    char *url = row[4];
    if ((refSeq==NULL)||sameOk(refSeq,""))
	{
	++refSeqEmpty;
	continue;
	}
    if (sameOk(refSeq,"unknown"))
	{
	++refSeqUnknown;
	continue;
	}

    if (hashFindVal(refSeqHash, refSeq))
	{
	++refSeqDupe;
	// maybe save these to a file?
	verbose(2,"dupe refSeq: %s\n", refSeq);
	continue;
	}

    if (hashFindVal(geneSymbolHash, gene))
	{
	++geneSymbolDupe;
	// maybe save these to a file?
	verbose(2,"dupe gene symbol: %s\n", gene);
	continue;
	}

    char *existing = hashFindVal(imageHash, gene);
    if (existing)
	{
	/* this one is in use */
	if (checkImages)
	    checkOutImages(sourcePath, existing);
	hashRemove(imageHash, gene);  
	/* any remaining at end are unused images */
	}
    else
	{
	++imageMissing;
	// maybe save these to a file?
	verbose(2,"missing image: %s\n", gene);
	continue;
	}

    hashAdd(refSeqHash, refSeq, "");
    hashAdd(geneSymbolHash, gene, "");

    fprintf(f, "%s\t", gene);
    fprintf(f, "%s\t", geneName);
    fprintf(f, "%s\t", entrez);
    fprintf(f, "%s\t", refSeq);
    fprintf(f, "%s\n", url);

    ++count;

    }

lineFileClose(&lf);    
carefulClose(&f);

/* dump unused images */
int i;
struct hashEl *hel = NULL;
for (i=0; i<imageHash->size; ++i)
    {
    for (hel = imageHash->table[i]; hel != NULL; hel = hel->next)
	{
	char *val = (char *)hel->val;
	printf("unused image: %s\n", val);
	if (cleanImages)
	    cleanOutImages(imageDisk,sourcePath,val,"unused");
	}
    }


printf("# with empty refSeq: %d\n",refSeqEmpty);
printf("# with refSeq=\"unknown\": %d\n",refSeqUnknown);
printf("# of rows with duplicate refSeq: %d\n",refSeqDupe);
printf("# of rows with duplicate geneSymbol: %d\n",geneSymbolDupe);
printf("Got %d images\n", imageCount);
printf("# of duplicate images: %d\n",imageDupe);
printf("# of missing images: %d\n",imageMissing);
printf("# of unused images: %d\n", imageHash->elCount);
printf("final count of unique existing gene/images: %d\n",count);

}

/*
   checkImages option
  Add the checking of the sourceImageDir: main jpg, thumb jpg, 7 levels of tiles jpg
    report any that are incomplete.
*/

/* cleanImages option
  moves .jp2 duplicates into ./dupes and unused into ./unused,
   and removes the corresponding jpg files (full-download-image,tiles,thumbnail) in sourceImageDir
*/


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
cleanImages = optionExists("cleanImages");
checkImages = optionExists("checkImages");
allenCleanup(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

