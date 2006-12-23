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

static char const rcsid[] = "$Id: allenCleanup.c,v 1.1 2006/12/23 04:19:24 galt Exp $";

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

static struct optionSpec options[] = {
   {"cleanImages", OPTION_BOOLEAN},
   {NULL, 0},
};

struct hash *makeImageHash(char *imageDisk, int *dupeCount)
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
char outPath[PATH_LEN];
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
struct hash *imageHash = makeImageHash(imageDisk,&imageDupe);
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
    //char *probeId = hashFindVal(nameHash, refSeq);
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
	uglyf("dupe refSeq: %s\n", refSeq);
	continue;
	}

    if (hashFindVal(geneSymbolHash, gene))
	{
	++geneSymbolDupe;
	// maybe save these to a file?
	uglyf("dupe gene symbol: %s\n", gene);
	continue;
	}

    if (hashFindVal(imageHash, gene))
	{
	/* this one is in use */
	hashRemove(imageHash, gene);  
	/* any remaining at end are unused images */
	}
    else
	{
	++imageMissing;
	// maybe save these to a file?
	uglyf("missing image: %s\n", gene);
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

//TODO:
//  Add the checking of the sourceImageDir: main jpg, thumb jpg, 7 levels of tiles jpg
//    report any that are incomplete.
//
//TODO:
//  Implement the cleanImages option
//   which would move .jp2 duplicates into ./dupes and unused into ./unused,
//   and when it did so, it would remove the corresponding jpg files in sourceImageDir

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
cleanImages = optionExists("cleanImages");
allenCleanup(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

