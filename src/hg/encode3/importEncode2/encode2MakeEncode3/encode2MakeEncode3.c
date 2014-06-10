/* encode2MakeEncode3 - Create a makefile that will reformat and copy encode2 files into
 * a parallel directory of encode3 files. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "md5.h"
#include "portable.h"
#include "obscure.h"
#include "sqlNum.h"
#include "encode3/encode2Manifest.h"

char *dataDir = "/scratch/kent/encValData";
char *tempDir = "/tmp";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2MakeEncode3 - Create a makefile that will reformat and copy encode2 files into\n"
  "a parallel directory of encode3 files. Should be run in San Diego after encode2 files are\n"
  "already transfered.  In addition to the makefile it creates all the destination subdirs\n"
  "and a remap file that maps source files to destination files.\n"
  "The usage is:\n"
  "   encode2MakeEncode3 sourceDir sourceManifest destDir out.make destManifest remapNameFile\n"
  "When the makefile is run it will transform most element type (GFF and Bed-variants) into\n"
  "bigBed.  It also unpacks .fastq.tzg files into directories full of .fastq.gz\n"
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

char *veryTempName(char *dir, char *base, char *suffix)
/* Make a temp name that should be uniq on file system */
{
static int id = 0;
char rebase[128];
safef(rebase, sizeof(rebase), "%d_%s", ++id, base);
return cloneString(rTempName(dir, rebase, suffix));
}


boolean needsBedDoctor(char *fileName)
/* Returns TRUE if we should run encode2BedDoctor on file. */
{
if (startsWith("wgEncodeGisRnaPet", fileName))
    return TRUE;
else if (startsWith("wgEncodeCshlLongRnaSeq", fileName))
    return TRUE;
else if (startsWith("wgEncodeCshlShortRnaSeq", fileName))
    return TRUE;
else if (startsWith("wgEncodeRikenCage", fileName))
    return TRUE;
else if (startsWith("wgEncodeUwRepliSeq", fileName))
    return TRUE;
else
    return FALSE;
}

void doGzippedBedToBigBed(struct encode2Manifest *mi, char *bedFile, char *destPath,
    char *assembly, char *asType, char *bedType, char *midFix,
    char *destDir, char *destFileName,
    struct slName **pTargetList, FILE *f, FILE *manF)
/* Convert some bed file to a a bigBed file possibly using an as file. */
{
/* First handle the straight up copy - even though minimal changes to bigBed,
 * for archival purposes Eurie and Cricket want original bed too. */
fprintf(f, "%s: %s\n", destPath, bedFile);
fprintf(f, "\tln -s %s %s\n", bedFile, destPath);
slNameAddHead(pTargetList, destPath);
encode2ManifestShortTabOut(mi, manF);

/* Figure out name of bigBed file we will output and write it as a make target. */
char outFileName[FILENAME_LEN];
safef(outFileName, sizeof(outFileName), "%s%s", destFileName, ".bigBed");
char outPath[PATH_LEN];
safef(outPath, sizeof(outPath), "%s%s%s", destDir, destFileName, ".bigBed");
fprintf(f, "%s: %s\n", outPath, bedFile);


/* Unpack gzipped bed and sort it. */
char *tempNameRoot = "b2bb";
char *clippedBed = veryTempName(tempDir, tempNameRoot, ".clipped");
if (sameOk(asType, "peptideMapping"))  // special cleanups...
    fprintf(f, "\tzcat %s | tr '\\r' '\\n' | grep -v '^track' | cut -f 1-10 "
	       "| bedClip stdin %s/%s/chrom.sizes %s\n",
	       bedFile, dataDir, assembly, clippedBed);
else
    fprintf(f, "\tzcat %s | grep -v '^track' | bedClip stdin %s/%s/chrom.sizes %s\n",
	bedFile, dataDir, assembly, clippedBed);
char *sortedBed = veryTempName(tempDir, tempNameRoot, ".sorted.bed");
fprintf(f, "\tsort -k1,1 -k2,2n %s > %s\n", clippedBed, sortedBed);
fprintf(f, "\trm %s\n", clippedBed);

/* Figure out if it's one we need to doctor up, and if so emit that code */
char *doctoredBed = NULL;
char *bigBedSource = NULL;
if (needsBedDoctor(destFileName))
    {
    doctoredBed = veryTempName(tempDir, "b2bb", ".doctored.bed");
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

/* Add to target list. */
slNameAddHead(pTargetList, outPath);

/* Print out info about bigBed we made to new manifest files. */
char localFileName[PATH_LEN+8];	// a little extra for .bigBed
safef(localFileName, PATH_LEN, "%s", mi->fileName);
chopSuffix(localFileName);  // Chop off .gz
if (midFix != NULL) // If midfix, replace .narrowPeak etc with midFix.  Used to get rid of .bed
     {
     chopSuffix(localFileName);
     strcat(localFileName, midFix);
     }
strcat(localFileName, ".bigBed");
mi->fileName = localFileName;
mi->format = asType;
encode2ManifestShortTabOut(mi, manF);
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

void doGzippedSomethingToBigBed(struct encode2Manifest *mi, char *sourcePath, char *assembly, 
    char *destDir, char *destFileName,
    char *bedConverter, char *tempNameRoot, struct slName **pTargetList, FILE *f, FILE *manF)
/* Convert something that has a bed-converter program to bigBed. */
{
char bigBedName[PATH_LEN];
safef(bigBedName, sizeof(bigBedName), "%s%s%s", destDir, destFileName, ".bigBed");
char tempBigBed[PATH_LEN];
safef(tempBigBed, sizeof(tempBigBed), "%s.tmp", bigBedName);
char *tempBed = veryTempName(tempDir, tempNameRoot, ".bed");
char *sortedTempBed = veryTempName(tempDir, tempNameRoot, ".sorted");
fprintf(f, "%s: %s\n", bigBedName, sourcePath);
fprintf(f, "\t%s %s %s\n", bedConverter, sourcePath, tempBed);
fprintf(f, "\tsort -k1,1 -k2,2n %s > %s\n", tempBed, sortedTempBed);
fprintf(f, "\trm %s\n", tempBed);
char *clippedTempBed = veryTempName(tempDir, tempNameRoot, ".clipped");
fprintf(f, "\tbedClip %s %s/%s/chrom.sizes %s\n", sortedTempBed, dataDir, assembly, clippedTempBed);
fprintf(f, "\trm %s\n", sortedTempBed);
fprintf(f, "\tbedToBigBed %s %s/%s/chrom.sizes %s\n", 
    clippedTempBed, dataDir, assembly, tempBigBed);
fprintf(f, "\trm %s\n", clippedTempBed);
fprintf(f, "\tmv %s %s\n", tempBigBed, bigBedName);
slNameAddHead(pTargetList, bigBedName);

/* Print out info about bigBed we made to new manifest files. */
char localFileName[PATH_LEN+8];	// a little extra for .bigBed
safef(localFileName, PATH_LEN, "%s", mi->fileName);
chopSuffix(localFileName);
strcat(localFileName, ".bigBed");
mi->fileName = localFileName;
encode2ManifestShortTabOut(mi, manF);
}

void doGzippedGffToBigBed(struct encode2Manifest *mi, char *sourcePath, char *destPath, 
    char *assembly, char *destDir, char *destFileName, 
    struct slName **pTargetList, FILE *f, FILE *manF)
/* Do both copy and conversion to bigBed.  Also do some doctoring. */
{
/* First handle the straight up copy. */
fprintf(f, "%s: %s\n", destPath, sourcePath);
fprintf(f, "\tln -s %s %s\n", sourcePath, destPath);
slNameAddHead(pTargetList, destPath);
encode2ManifestShortTabOut(mi, manF);

/* Now convert to big bed. */
char *tempNameRoot = "gff2bb";
char bigBedName[PATH_LEN];
safef(bigBedName, sizeof(bigBedName), "%s%s%s", destDir, destFileName, ".bigBed");
char tempBigBed[PATH_LEN];
safef(tempBigBed, sizeof(tempBigBed), "%s.tmp", bigBedName);
char *fixedGff = veryTempName(tempDir, tempNameRoot, ".gff");
char *tempBed = veryTempName(tempDir, tempNameRoot, ".bed");
char *sortedTempBed = veryTempName(tempDir, tempNameRoot, ".sorted");
char *clippedTempBed = veryTempName(tempDir, tempNameRoot, ".clipped");
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

/* Print out info about bigBed we made to new manifest files. */
char localFileName[PATH_LEN+8];	// a little extra for .bigBed
safef(localFileName, PATH_LEN, "%s", mi->fileName);
chopSuffix(localFileName);
strcat(localFileName, ".bigBed");
mi->fileName = localFileName;
mi->format = "bigBed";
encode2ManifestShortTabOut(mi, manF);
}

void processManifestItem(int itemNo, struct encode2Manifest *mi, char *sourceRoot, 
    char *destRoot, struct slName **pTargetList, FILE *f, FILE *manF)
/* Process a line from the manifest.  Write section of make file needed to transform/copy it.
 * record name of this target file in pTargetList. 
 * The transformations are:
 * o - Many files are just copied.  
 * o - Files that are bed variants are turned into bigBed variants
 * o - Files that are tgz's of multiple fastqs are split into individual fastq.gz's inside
 *     a directory named after the archive. */
{
fprintf(manF, "# from %s:\n", mi->fileName);
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
    fprintf(f, "\tcd %s; gzip -4 *\n", tmpDir);
    fprintf(f, "\tmv %s %s\n", tmpDir, outDir);
    slNameAddHead(pTargetList, outDir);

    /* Write out revised manifest info */
    char localFileName[PATH_LEN+4];	// a little extra for .dir
    safef(localFileName, PATH_LEN, "%s", mi->fileName);
    strcat(localFileName, ".dir");
    mi->fileName = localFileName;
    encode2ManifestShortTabOut(mi, manF);
    }
else if (endsWith(fileName, ".narrowPeak.gz"))
    {
    doGzippedBedToBigBed(mi, sourcePath, destPath, assembly, "narrowPeak", "bed6+4", 
	NULL, destDir, destFileName, pTargetList, f, manF);
    }
else if (endsWith(fileName, ".broadPeak.gz"))
    {
    doGzippedBedToBigBed(mi, sourcePath, destPath, assembly, "broadPeak", "bed6+3", 
	NULL, destDir, destFileName, pTargetList, f, manF);
    }
else if (endsWith(fileName, ".bedRnaElements.gz"))
    {
    doGzippedBedToBigBed(mi, sourcePath, destPath, assembly, "bedRnaElements", "bed6+3", 
	NULL, destDir, destFileName, pTargetList, f, manF);
    }
else if (endsWith(fileName, ".bedLogR.gz"))
    {
    doGzippedBedToBigBed(mi, sourcePath, destPath, assembly, "bedLogR", "bed9+1", 
	NULL, destDir, destFileName, pTargetList, f, manF);
    }
else if (endsWith(fileName, "bedRrbs.gz"))
    {
    doGzippedBedToBigBed(mi, sourcePath, destPath, assembly, "bedRrbs", "bed9+2", 
	NULL, destDir, destFileName, pTargetList, f, manF);
    }
else if (endsWith(fileName, ".peptideMapping.gz"))
    {
    doGzippedBedToBigBed(mi, sourcePath, destPath, assembly, "peptideMapping", "bed6+4", 
	NULL, destDir, destFileName, pTargetList, f, manF);
    }
else if (endsWith(fileName, ".shortFrags.gz"))
    {
    doGzippedBedToBigBed(mi, sourcePath, destPath, assembly, "shortFrags", "bed6+21", 
	NULL, destDir, destFileName, pTargetList, f, manF);
    }
else if (endsWith(fileName, ".bedClusters.gz") || endsWith(fileName, ".bedCluster.gz"))
    {
    doGzippedBedToBigBed(mi, sourcePath, destPath, assembly, NULL, NULL, 
	NULL, destDir, destFileName, pTargetList, f, manF);
    }
else if (endsWith(fileName, ".bed.gz") || endsWith(fileName, ".bed9.gz"))
    {
    if (stringIn("wgEncodeHaibMethylRrbs/", fileName))
	{
	doGzippedBedToBigBed(mi, sourcePath, destPath, assembly, "bedRrbs", "bed9+2", 
	    NULL, destDir, destFileName, pTargetList, f, manF);
	}
    else if (stringIn("wgEncodeOpenChromSynth/", fileName))
        {
	doGzippedBedToBigBed(mi, sourcePath, destPath, assembly, "openChromCombinedPeaks", 
	    "bed9+12", NULL, destDir, destFileName, pTargetList, f, manF);
	}
    else
	{
	char localName[PATH_LEN];
	safef(localName, sizeof(localName), "%s", destFileName);
	chopSuffix(localName);	// remove .bed
	doGzippedBedToBigBed(mi, sourcePath, destPath, assembly, NULL, NULL, 
	    "", destDir, localName, pTargetList, f, manF);
	}
    }
else if (endsWith(fileName, ".gp.gz"))
    {
    doGzippedSomethingToBigBed(mi, sourcePath, assembly, destDir, destFileName, 
	"genePredToBed", "gp2bb", pTargetList, f, manF);
    }
else if (endsWith(fileName, ".gtf.gz") || endsWith(fileName, ".gff.gz"))
    {
    doGzippedGffToBigBed(mi, sourcePath, destPath, assembly, destDir, destFileName, pTargetList, 
	f, manF);
    }
else if (justCopySuffix(fileName))
    {
    fprintf(f, "%s: %s\n", destPath, sourcePath);
    fprintf(f, "\tln -s %s %s\n", sourcePath, destPath);
    slNameAddHead(pTargetList, destPath);
    encode2ManifestShortTabOut(mi, manF);
    }
else
    {
    errAbort("Don't know what to do with item %d %s in %s line %d", 
             itemNo, fileName, __FILE__, __LINE__);
    }
}


void encode2MakeEncode3(char *sourceDir, char *sourceManifest, char *destDir, char *outMake,
    char *outManifest, char *outRename)
/* encode2MakeEncode3 - Copy files in encode2 manifest and in case of tar'd files rezip them 
 * independently. */
{
struct encode2Manifest *fileList = encode2ManifestShortLoadAll(sourceManifest);
verbose(2, "Loaded information on %d files from %s\n", slCount(fileList), sourceManifest);
verboseTimeInit();
FILE *f = mustOpen(outMake, "w");
FILE *manF = mustOpen(outManifest, "w");
FILE *renameF = mustOpen(outRename, "w");
struct encode2Manifest *mi;
int destDirPrefixSize = strlen(destDir)+1;  /* +1 for / */
struct hash *destDirHash = hashNew(0);
makeDirOnlyOnce(destDir, destDirHash);

/* Print first dependency in makefile - the one that causes all files to be made. */
fprintf(f, "startHere:  all\n\techo all done\n\n");

/* Write out each file target, and save also list of all targets. */
struct slName *targetList = NULL;
int itemNo = 0;
for (mi = fileList; mi != NULL; mi = mi->next)
    {
    char *origFileName = cloneString(mi->fileName);

    /* Keep a list of what all it goes to. */
    struct slName *oneTargetList = NULL;

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

    processManifestItem(++itemNo, mi, sourceDir, destDir, &oneTargetList, f, manF);

    /* Write out rename info. */
    if (oneTargetList != NULL)
	{
	if (oneTargetList->next != NULL || !sameString(origFileName, oneTargetList->name + destDirPrefixSize))
	    {
	    fprintf(renameF, "%s", origFileName);
	    struct slName *target;
	    for (target = oneTargetList; target != NULL; target = target->next)
		{
		fprintf(renameF, " %s", target->name + destDirPrefixSize);
		}
	    fprintf(renameF, "\n");
	    }
	}

    /* Save local target list to big one. */
    targetList = slCat(oneTargetList, targetList);
    }

slReverse(&targetList);
fprintf(f, "all:");
struct slName *target;
for (target = targetList; target != NULL; target = target->next)
    fprintf(f, " %s", target->name);
fprintf(f, "\n");

carefulClose(&renameF);
carefulClose(&manF);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
dataDir = optionVal("dataDir", dataDir);
if (argc != 7)
    usage();
encode2MakeEncode3(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
