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

boolean needsBedDoctor(char *fileName)
/* Returns TRUE if we should run encode2BedDoctor on file. */
{
if (startsWith("wgEncodeGisRnaPet", fileName))
    return TRUE;
else if (startsWith("wgEncodeCshlLongRnaSeq", fileName))
    return TRUE;
else if (startsWith("wgEncodeRikenCage", fileName))
    return TRUE;
else if (startsWith("wgEncodeUwRepliSeq", fileName))
    return TRUE;
else
    return FALSE;
}

void doGzippedBedToBigBed(char *bedFile, char *assembly,
    char *asType, char *bedType, 
    char *destDir, char *destFileName,
    struct slName **pTargetList, FILE *f)
/* Convert some bed file to a a bigBed file possibly using an as file. */
{
/* Figure out name of bigBed file we will output and write it as a make target. */
char outFileName[FILENAME_LEN];
safef(outFileName, sizeof(outFileName), "%s%s", destFileName, ".bigBed");
char outPath[PATH_LEN];
safef(outPath, sizeof(outPath), "%s%s%s", destDir, destFileName, ".bigBed");
fprintf(f, "%s: %s\n", outPath, bedFile);

/* Unpack gzipped bed and sort it. */
char *sortedBed = cloneString(rTempName(tempDir, "b2bb", ".sorted.bed"));
fprintf(f, "\tzcat %s | grep -v '^track' | sort -k1,1 -k2,2n > %s\n", bedFile, sortedBed);

/* Figure out if it's one we need to doctor up, and if so emit that code */
char *doctoredBed = NULL;
char *bigBedSource = NULL;
if (needsBedDoctor(destFileName))
    {
    doctoredBed = cloneString(rTempName(tempDir, "b2bb", ".doctored.bed"));
    if (asType == NULL)
	fprintf(f, "\tencode2BedDoctor %s %s\n", sortedBed, doctoredBed);
    else
        fprintf(f, "\tencode2BedPlusDoctor %s %s/as/%s.as %s\n", sortedBed, 
	    dataDir, asType, doctoredBed);
    fprintf(f, "\trm %s\n", sortedBed);
    bigBedSource = doctoredBed;
    }
else
    {
    bigBedSource = sortedBed;
    }

/* Write bigBed, initially to a temp name, and then moving it to real name if all went well. */
char tempBigBed[PATH_LEN];
safef(tempBigBed, sizeof(tempBigBed), "%s.tmp", outPath);
fprintf(f, "\tbedToBigBed ");
if (bedType != NULL)
     fprintf(f, " -type=%s", bedType);
if (asType != NULL)
     fprintf(f, " -as=%s/as/%s.as", dataDir, asType);
fprintf(f, " %s %s/%s/chrom.sizes %s\n", bigBedSource, dataDir, assembly, tempBigBed);
fprintf(f, "\tmv %s %s\n", tempBigBed, outPath);
fprintf(f, "\trm %s\n", bigBedSource);

/* Add to target list and go home. */
slNameAddHead(pTargetList, outPath);
}

boolean justCopySuffix(char *fileName)
/* Return TRUE if fileName has a suffix that indicates we just copy it rather than
 * transform it. */
{
static char *copySuffixes[] = {".fastq.gz", ".bigWig", ".bigBed", ".fasta.gz", ".bam", ".spikeins",
    ".pdf.gz", ".pdf", ".pair.tar.gz", ".tar.gz", ".tab.gz", ".csfasta.gz", ".csfastq.gz",
    ".csqual.gz", "Validation.tgz", ".doc.tgz", ".matrix.gz", "PrimerPeaks.peaks.gz",
    ".matrix.tgz", ".CEL.gz", ".spikeins.CL.bam.gz", ".document.tgz"
    };
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

void doGzippedGffToBigBed(char *sourcePath, char *destPath, 
    char *assembly, char *destDir, char *destFileName, 
    struct slName **pTargetList, FILE *f)
/* Do both copy and conversion to bigBed.  Also do some doctoring. */
{
/* First handle the straight up copy. */
fprintf(f, "%s: %s\n", destPath, sourcePath);
fprintf(f, "\tln -s %s %s\n", sourcePath, destPath);
slNameAddHead(pTargetList, destPath);

/* Now convert to big bed. */
char *tempNameRoot = "gff2bb";
char bigBedName[PATH_LEN];
safef(bigBedName, sizeof(bigBedName), "%s%s%s", destDir, destFileName, ".bigBed");
char tempBigBed[PATH_LEN];
safef(tempBigBed, sizeof(tempBigBed), "%s.tmp", bigBedName);
char *fixedGff = cloneString(rTempName(tempDir, tempNameRoot, ".gff"));
char *tempBed = cloneString(rTempName(tempDir, tempNameRoot, ".bed"));
char *sortedTempBed = cloneString(rTempName(tempDir, tempNameRoot, ".sorted"));
char *clippedTempBed = cloneString(rTempName(tempDir, tempNameRoot, ".clipped"));
fprintf(f, "%s: %s\n", bigBedName, sourcePath);
fprintf(f, "\tencode2GffDoctor %s %s\n", sourcePath, fixedGff);
fprintf(f, "\tgffToBed %s %s\n", fixedGff, tempBed);
fprintf(f, "\trm %s\n", fixedGff);
fprintf(f, "\tsort -k1,1 -k2,2n %s > %s\n", tempBed, sortedTempBed);
fprintf(f, "\trm %s\n", tempBed);
fprintf(f, "\tbedClip %s %s/%s/chrom.sizes %s\n", sortedTempBed, dataDir, assembly, clippedTempBed);
fprintf(f, "\trm %s\n", sortedTempBed);
fprintf(f, "\tbedToBigBed %s %s/%s/chrom.sizes %s\n", 
    clippedTempBed, dataDir, assembly, tempBigBed);
fprintf(f, "\trm %s\n", clippedTempBed);
fprintf(f, "\tmv %s %s\n", tempBigBed, bigBedName);
slNameAddHead(pTargetList, bigBedName);
}

void processManifestItem(int itemNo, struct manifestInfo *mi, char *sourceRoot, 
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

/* See if source file exists.  If not warn and skip. */
if (!fileExists(sourcePath))
    {
    warn("%s doesn't exist", sourcePath);
    return;
    }

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
else if (endsWith(fileName, ".bedLogR.gz"))
    {
    doGzippedBedToBigBed(sourcePath, assembly, "bedLogR", "bed9+1", 
	destDir, destFileName, pTargetList, f);
    }
else if (endsWith(fileName, "bedRrbs.gz"))
    {
    doGzippedBedToBigBed(sourcePath, assembly, "bedRrbs", "bed9+2", 
	destDir, destFileName, pTargetList, f);
    }
else if (endsWith(fileName, ".peptideMapping.gz"))
    {
    doGzippedBedToBigBed(sourcePath, assembly, "peptideMapping", "bed9+1", 
	destDir, destFileName, pTargetList, f);
    }
else if (endsWith(fileName, ".shortFrags.gz"))
    {
    doGzippedBedToBigBed(sourcePath, assembly, NULL, "bed6+21", 
	destDir, destFileName, pTargetList, f);
    }
else if (endsWith(fileName, ".bedClusters.gz") || endsWith(fileName, ".bedCluster.gz"))
    {
    doGzippedBedToBigBed(sourcePath, assembly, NULL, NULL, 
	destDir, destFileName, pTargetList, f);
    }
else if (endsWith(fileName, ".bed.gz") || endsWith(fileName, ".bed9.gz"))
    {
    if (stringIn("wgEncodeHaibMethylRrbs/", fileName))
	{
	doGzippedBedToBigBed(sourcePath, assembly, "bedRrbs", "bed9+2", 
	    destDir, destFileName, pTargetList, f);
	}
    else if (stringIn("wgEncodeOpenChromSynth/", fileName))
        {
	doGzippedBedToBigBed(sourcePath, assembly, "openChromCombinedPeaks", "bed9+12", 
	    destDir, destFileName, pTargetList, f);
	}
    else
	{
	chopSuffix(destFileName);	// remove .bed
	doGzippedBedToBigBed(sourcePath, assembly, NULL, NULL, 
	    destDir, destFileName, pTargetList, f);
	}
    }
else if (endsWith(fileName, ".gp.gz"))
    {
    doGzippedSomethingToBigBed(sourcePath, assembly, destDir, destFileName, 
	"genePredToBed", "gp2bb", pTargetList, f);
    }
else if (endsWith(fileName, ".gtf.gz") || endsWith(fileName, ".gff.gz"))
    {
    doGzippedGffToBigBed(sourcePath, destPath, assembly, destDir, destFileName, pTargetList, f);
    }
else if (justCopySuffix(fileName))
    {
    fprintf(f, "%s: %s\n", destPath, sourcePath);
    fprintf(f, "\tln -s %s %s\n", sourcePath, destPath);
    slNameAddHead(pTargetList, destPath);
    }
else
    {
    errAbort("Don't know what to do with item %d %s in %s line %d", 
             itemNo, fileName, __FILE__, __LINE__);
    }
}


void encode2MakeEncode3(char *sourceDir, char *sourceManifest, char *destDir, char *outMake)
/* encode2MakeEncode3 - Copy files in encode2 manifest and in case of tar'd files rezip them 
 * independently. */
{
struct manifestInfo *fileList = manifestInfoLoadAll(sourceManifest);
verbose(2, "Loaded information on %d files from %s\n", slCount(fileList), sourceManifest);
verboseTimeInit();
FILE *f = mustOpen(outMake, "w");
struct manifestInfo *mi;
struct hash *destDirHash = hashNew(0);
makeDirOnlyOnce(destDir, destDirHash);

/* Print first dependency in makefile - the one that causes all files to be made. */
fprintf(f, "startHere:  all\n\techo all done\n\n");

/* Write out each file target, and save also list of all targets. */
struct slName *targetList = NULL;
int itemNo = 0;
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

    processManifestItem(++itemNo, mi, sourceDir, destDir, &targetList, f);
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
