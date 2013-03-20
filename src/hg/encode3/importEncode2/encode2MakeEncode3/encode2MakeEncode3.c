/* encode2MakeEncode3 - Create a makefile that will reformat and copy encode2 files into
 * a parallel directory of encode3 files. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "md5.h"
#include "portable.h"
#include "obscure.h"

char *dataDir = "/scratch/kent/encValData";
char *tempDir = "/tmp";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2MakeEncode3 - Create a makefile that will reformat and copy encode2 files into\n"
  "a parallel direcgtory of encode3 files.\n"
  "   encode2MakeEncode3 sourceDir sourceManifest destDir out.make\n"
  "options:\n"
  "   -dataDir=/path/to/encode/asFilesAndChromSizesEtc\n"
  "   -tmpDir=/tmp\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"dataDir", OPTION_STRING},
   {NULL, 0},
};

#define FILEINFO_NUM_COLS 6

struct manifestInfo
/* Information on one file */
    {
    struct manifestInfo *next;  /* Next in singly linked list. */
    char *fileName;	/* Name of file with directory relative to manifest */
    char *format;	/* bam fastq etc */
    char *experiment;	/* wgEncodeXXXX */
    char *replicate;	/* 1 2 both n/a */
    char *enrichedIn;	/* promoter exon etc. */
    char *md5sum;	/* Hash of file contents or n/a */
    };

struct manifestInfo *manifestInfoLoad(char **row)
/* Load a manifestInfo from row fetched with select * from manifestInfo
 * from database.  Dispose of this with manifestInfoFree(). */
{
struct manifestInfo *ret;

AllocVar(ret);
ret->fileName = cloneString(row[0]);
ret->format = cloneString(row[1]);
ret->experiment = cloneString(row[2]);
ret->replicate = cloneString(row[3]);
ret->enrichedIn = cloneString(row[4]);
ret->md5sum = cloneString(row[5]);
return ret;
}

void manifestInfoTabOut(struct manifestInfo *mi, FILE *f)
/* Write tab-separated version of manifestInfo to f */
{
fprintf(f, "%s\t", mi->fileName);
fprintf(f, "%s\t", mi->format);
fprintf(f, "%s\t", mi->experiment);
fprintf(f, "%s\t", mi->replicate);
fprintf(f, "%s\t", mi->enrichedIn);
fprintf(f, "%s\n", mi->md5sum);
}

struct manifestInfo *manifestInfoLoadAll(char *fileName)
/* Load all manifestInfos from file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[FILEINFO_NUM_COLS];
struct manifestInfo *list = NULL, *fi;
while (lineFileRow(lf, row))
   {
   fi = manifestInfoLoad(row);
   slAddHead(&list, fi);
   }
slReverse(&list);
return list;
}

void makeDirOnlyOnce(char *dir, struct hash *hash)
/* Check if dir is already in hash.  If so we're done.  If not make dir and add it to hash. */
{
if (!hashLookup(hash, dir))
    {
    verbose(2, "make dir %s\n", dir);
    hashAdd(hash, dir, NULL);
    makeDirs(dir);
    }
}


void doGzippedBedToBigBed(char *bedFile, char *assembly,
    char *asType, char *bedType, 
    char *destDir, char *destFileName,
    struct slName **pTargetList, FILE *f)
/* Convert some bed file to a a bigBed file possibly using an as file. */
{
char outFileName[FILENAME_LEN];
safef(outFileName, sizeof(outFileName), "%s%s", destFileName, ".bigBed");
char outPath[PATH_LEN];
safef(outPath, sizeof(outPath), "%s%s%s", destDir, destFileName, ".bigBed");
fprintf(f, "%s: %s\n", outPath, bedFile);
char *tempBed = rTempName(tempDir, "b2bb", ".bed");
char tempBigBed[PATH_LEN];
safef(tempBigBed, sizeof(tempBigBed), "%s.tmp", outPath);
fprintf(f, "\tzcat %s | sort -k1,1 -k2,2n > %s\n", bedFile, tempBed);
fprintf(f, "\tbedToBigBed ");
if (bedType != NULL)
     fprintf(f, " -type=%s", bedType);
if (asType != NULL)
     fprintf(f, " -as=%s/as/%s.as", dataDir, asType);
fprintf(f, " %s %s/%s/chrom.sizes %s\n", tempBed, dataDir, assembly, tempBigBed);
fprintf(f, "\tmv %s %s\n", tempBigBed, outPath);
fprintf(f, "\trm %s\n", tempBed);
slNameAddHead(pTargetList, outPath);
}

boolean justCopySuffix(char *fileName)
/* Return TRUE if fileName has a suffix that indicates we just copy it rather than
 * transform it. */
{
static char *copySuffixes[] = {".fastq.gz", ".bigWig", ".bigBed", ".fasta.gz", ".bam", ".spikeins"};
int i;
for (i=0; i<ArraySize(copySuffixes); ++i)
    if (endsWith(fileName, copySuffixes[i]))
        return TRUE;
return FALSE;
}

void doGzippedSomethingToBigBed(char *sourcePath, char *assembly, char *destDir, char *destFileName,
    char *bedConverter, char *tempNameRoot, struct slName **pTargetList, FILE *f)
/* Convert something that has a bed-converter program to bigBed. */
{
char bigBedName[PATH_LEN];
safef(bigBedName, sizeof(bigBedName), "%s%s%s", destDir, destFileName, ".bigBed");
char tempBigBed[PATH_LEN];
safef(tempBigBed, sizeof(tempBigBed), "%s.tmp", bigBedName);
char *tempBed = cloneString(rTempName(tempDir, tempNameRoot, ".bed"));
char *sortedTempBed = cloneString(rTempName(tempDir, tempNameRoot, ".sorted"));
fprintf(f, "%s: %s\n", bigBedName, sourcePath);
fprintf(f, "\t%s %s %s\n", bedConverter, sourcePath, tempBed);
fprintf(f, "\tsort -k1,1 -k2,2n %s > %s\n", tempBed, sortedTempBed);
fprintf(f, "\tbedToBigBed %s %s/%s/chrom.sizes %s\n", sortedTempBed, dataDir, assembly, tempBigBed);
fprintf(f, "\trm %s\n", tempBed);
fprintf(f, "\trm %s\n", sortedTempBed);
fprintf(f, "\tmv %s %s\n", tempBigBed, bigBedName);
slNameAddHead(pTargetList, bigBedName);
}

void processManifestItem(struct manifestInfo *mi, char *sourceRoot, 
    char *destRoot, struct slName **pTargetList, FILE *f)
/* Process a line from the manifest.  Write section of make file needed to transform/copy it.
 * record name of this target file in pTargetList. 
 * The transformations are:
 * o - Many files are just copied.  
 * o - Files that are bed variants are turned into bigBed variants
 * o - Files that are tgz's of multiple fastqs are split into individual fastq.gz's inside
 *     a directory named after the archive. */
{
/* Make up bunches of components for file names. */
char *fileName = mi->fileName;
char sourcePath[PATH_LEN];
safef(sourcePath, sizeof(sourcePath), "%s/%s", sourceRoot, fileName);
char destPath[PATH_LEN];
char destDir[PATH_LEN], destFileName[FILENAME_LEN], destExtension[FILEEXT_LEN];
safef(destPath, sizeof(destPath), "%s/%s", destRoot, fileName);
splitPath(destPath, destDir, destFileName, destExtension);

/* Figure out whether we are on assembly hg19, mm9, or something we don't understand */
char *assembly = NULL;
if (startsWith("hg19/", fileName))
    assembly = "hg19";
else if (startsWith("mm9/", fileName))
    assembly = "mm9";
else
    errAbort("Don't recognize assembly for %s", fileName);

verbose(2, "processing %s\t%s\n", fileName, mi->format);
if (endsWith(fileName, ".fastq.tgz"))
    {
    char outDir[PATH_LEN];
    safef(outDir, sizeof(outDir), "%s.dir", destPath);
    verbose(2, "Unpacking %s into %s\n", sourcePath, outDir);
    fprintf(f, "%s: %s\n", outDir, sourcePath);
    char tmpDir[PATH_LEN];
    safef(tmpDir, sizeof(tmpDir), "%s.tmp", destPath);
    fprintf(f, "\tmkdir %s\n", tmpDir);
    fprintf(f, "\tcd %s; tar -zxf %s\n", tmpDir, sourcePath);
    fprintf(f, "\tencode2FlattenFastqSubdirs %s\n", tmpDir);
    fprintf(f, "\tcd %s; gzip *\n", tmpDir);
    fprintf(f, "\tmv %s %s\n", tmpDir, outDir);
    slNameAddHead(pTargetList, outDir);
    }
else if (endsWith(fileName, ".narrowPeak.gz"))
    {
    doGzippedBedToBigBed(sourcePath, assembly, "narrowPeak", "bed6+4", 
	destDir, destFileName, pTargetList, f);
    }
else if (endsWith(fileName, ".broadPeak.gz"))
    {
    doGzippedBedToBigBed(sourcePath, assembly, "broadPeak", "bed6+3", 
	destDir, destFileName, pTargetList, f);
    }
else if (endsWith(fileName, ".bedRnaElements.gz"))
    {
    doGzippedBedToBigBed(sourcePath, assembly, "bedRnaElements", "bed6+3", 
	destDir, destFileName, pTargetList, f);
    }
else if (endsWith(fileName, ".bed.gz") || endsWith(fileName, ".bed9.gz"))
    {
    chopSuffix(destFileName);	// remove .bed
    doGzippedBedToBigBed(sourcePath, assembly, NULL, NULL, 
	destDir, destFileName, pTargetList, f);
    }
else if (endsWith(fileName, ".gp.gz"))
    {
    doGzippedSomethingToBigBed(sourcePath, assembly, destDir, destFileName, 
	"genePredToBed", "gp2bb", pTargetList, f);
    }
else if (endsWith(fileName, ".gtf.gz") || endsWith(fileName, ".gff.gz"))
    {
    doGzippedSomethingToBigBed(sourcePath, assembly, destDir, destFileName, 
	"gffToBed", "gff2bb", pTargetList, f);
    }
else if (justCopySuffix(fileName))
    {
    fprintf(f, "%s: %s\n", destPath, sourcePath);
    fprintf(f, "\tln -s %s %s\n", sourcePath, destPath);
    slNameAddHead(pTargetList, destPath);
    }
else
    {
    errAbort("Don't know what to do with %s in %s line %d", fileName, __FILE__, __LINE__);
    }
}


void encode2MakeEncode3(char *sourceDir, char *sourceManifest, char *destDir, char *outMake)
/* encode2MakeEncode3 - Copy files in encode2 manifest and in case of tar'd files rezip them 
 * independently. */
{
struct manifestInfo *fileList = manifestInfoLoadAll(sourceManifest);
verbose(1, "Loaded information on %d files from %s\n", slCount(fileList), sourceManifest);
verboseTimeInit();
FILE *f = mustOpen(outMake, "w");
struct manifestInfo *mi;
struct hash *destDirHash = hashNew(0);
makeDirOnlyOnce(destDir, destDirHash);

/* Print first dependency in makefile - the one that causes all files to be made. */
fprintf(f, "startHere:  all\n\techo all done\n\n");

/* Write out each file target, and save also list of all targets. */
struct slName *targetList = NULL;
for (mi = fileList; mi != NULL; mi = mi->next)
    {
    /* Make path to source file. */
    char sourcePath[PATH_LEN];
    safef(sourcePath, sizeof(sourcePath), "%s/%s", sourceDir, mi->fileName);

    /* Make destination dir */
    char localDir[PATH_LEN];
    splitPath(mi->fileName, localDir, NULL, NULL);
    char destSubDir[PATH_LEN];
    safef(destSubDir, sizeof(destSubDir), "%s/%s", destDir, localDir);
    makeDirOnlyOnce(destSubDir, destDirHash);

    char destPath[PATH_LEN];
    safef(destPath, sizeof(destPath), "%s/%s", destDir, mi->fileName);

    processManifestItem(mi, sourceDir, destDir, &targetList, f);

#ifdef OLD
    /* If md5sum available check it. */
    if (!sameString(mi->md5sum, "n/a"))
        {
	char *md5sum = md5HexForFile(sourcePath);
	verboseTime(1, "md5Summed %s (%lld bytes)", mi->fileName, (long long)fileSize(sourcePath));
	if (!sameString(md5sum, mi->md5sum))
	    {
	    warn("md5sum mismatch on %s, %s in metaDb vs %s in file", sourcePath, mi->md5sum, md5sum);
	    fprintf(f, "md5sum mismatch on %s, %s in metaDb vs %s in file\n", sourcePath, mi->md5sum, md5sum);
	    ++mismatch;
	    verbose(2, "%d md5sum matches, %d mismatches\n", match, mismatch);
	    }
	else 
	    ++match;
	}
#endif /* OLD */
    }

slReverse(&targetList);
fprintf(f, "all:");
struct slName *target;
for (target = targetList; target != NULL; target = target->next)
    fprintf(f, " %s", target->name);
fprintf(f, "\n");

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
dataDir = optionVal("dataDir", dataDir);
if (argc != 5)
    usage();
encode2MakeEncode3(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
