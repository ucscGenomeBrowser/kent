/* validate ENCODE3 manifest.txt creating output validated.txt */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "md5.h"
#include "hex.h"
#include "sqlNum.h"
#include "encode3/encode3Valid.h"
#include "gff.h"

char *version = "1.9";
char *workingDir = ".";
char *encValData = "encValData";
char *ucscDb = NULL;
char *validateFilesPath = "";

boolean quickMd5sum = FALSE;  // Just for development testing, do not use

void usage()
/* Explain usage and exit. */
{
errAbort(
    "validateManifest v%s - Validates the ENCODE3 manifest.txt file.\n"
    "                Calls validateFiles on each file in the manifest.\n"
    "                Exits with a zero status for no errors detected and non-zero for errors.\n"
    "                Writes Error messages to stderr\n"
    "usage:\n"
    "   validateManifest\n"
    "\n"
    "   -dir=workingDir, defaults to the current directory.\n"
    "   -encValData=encValDataDir, relative to workingDir, defaults to %s.\n"
    "\n"
    "   Input files in the working directory: \n"
    "     manifest.txt - current input manifest file\n"
    "     validated.txt - input from previous run of validateManifest\n" 
    "\n"
    "   Output file in the working directory: \n"
    "     validated.txt - results of validated input\n"
    "\n"
    , version, encValData
    );
}

static struct optionSpec options[] = {
    {"dir", OPTION_STRING},
    {"encValData", OPTION_STRING},
    {"quickMd5sum", OPTION_BOOLEAN},     // Testing option, user should not use
    {"-help", OPTION_BOOLEAN},
    {NULL, 0},
};


struct slRecord
/* List of tab-parsed records. */
    {
    struct slRecord *next;	/* Next in list. */
    char *row;               /* Allocated at run time to length of string. */
    char **words;            /* Array allocated dynamically */
    };



int readManifest(char *fileName, 
    struct slRecord **pFields, 
    struct slRecord **pAllRecs )
/* Read in the manifest file format into memory structures */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);

struct slRecord *allRecs = NULL;

char *row;
char **fields = NULL;
char **words = NULL;
int fieldCount = 0;

////verbose(2,"[%s %3d] file(%s)\n", __func__, __LINE__, lf->fileName);

int fieldNameRowsCount = 0;

while (lineFileNext(lf, &row, NULL))
    {
    //uglyf("%s\n", row); // DEBUG REMOVE
    if ( startsWith("#file_name", row) || 
	 startsWith("#file"     , row) ||  // catch some misspellings like #filename
	 startsWith("#ucsc_db"  , row))
	{
	if ( fieldNameRowsCount == 0)
	    {
	    ++fieldNameRowsCount;
	    // grab fieldnames from metadata
	    char *metaLine = cloneString(row);
	    //uglyf("%s\n", metaLine); // DEBUG REMOVE
	    ++metaLine;  // skip over the leading # char
	    fieldCount = chopByChar(metaLine, '\t', NULL, 0);
	    AllocArray(fields,fieldCount);
	    fieldCount = chopByChar(metaLine, '\t', fields, fieldCount);
	    /* DEBUG
	    for (i=0; i<fieldCount; ++i)
		{
		uglyf("field #%d = [%s]\n", i, fields[i]); // DEBUG REMOVE
		}
	    */
	    struct slRecord *meta = NULL;
	    AllocVar(meta);
	    meta->row = metaLine;
	    meta->words = fields;
	    if (pFields)
		*pFields = meta;	    
	    }
	else
	    {
	    errAbort("Found comment line listing field names more than once.");
	    }
	}
    else if (startsWith("#",row))
	{
	// ignore other comment lines?
	}
    else
	{

	char *line = cloneString(row);

	int n = 0;
	AllocArray(words,fieldCount+1);
	n = chopByChar(line, '\t', words, fieldCount+1);
	if (n != fieldCount)
	    {
	    errAbort("Error [file=%s, line=%d]: found %d columns, expected %d [%s]"
		, lf->fileName, lf->lineIx, n, fieldCount, row);
	    }

	struct slRecord *rec = NULL;
	AllocVar(rec);
	rec->row = line;
	rec->words = words;

	slAddHead(&allRecs, rec);	
	}

    }

slReverse(&allRecs);
if (pAllRecs)
    *pAllRecs = allRecs;	    

if (fieldNameRowsCount == 0)
    errAbort("Expected 1st line to contain a comment line listing field names.");

lineFileClose(&lf);

return fieldCount;

}


struct hash *makeFileNameHash(struct slRecord *recs, int fileNameIndex)
/* make a hash of all records by fileName */
{
struct hash *hash = newHash(12);
struct slRecord *rec = NULL;
for(rec = recs; rec; rec = rec->next)
    {
    if (hashLookup(hash, rec->words[fileNameIndex]))
	errAbort("duplicate file_name found: %s", rec->words[fileNameIndex]);
    hashAdd(hash, rec->words[fileNameIndex], rec);
    }
return hash;
}


char *getAs(char *asFileName)
/* Get full .as path */
{
char asPath[256];
safef(asPath, sizeof asPath, "%s/as/%s", encValData, asFileName);
return cloneString(asPath);
}

char *getGenome(char *fileName)
/* Get genome, e.g. hg19 */
{  
// TODO this could use some more development
// but start with something very simple for now
// such as assuming that the genome is found 
// as the prefix in the fileName path.
// Maybe in future can pull this from the hub.txt?
// ucscDb will be set to the value in the optional column "ucsc_db"
char genome[256] = "";
if (ucscDb)
    {
    safef(genome, sizeof genome, "%s", ucscDb);
    }
else
    {
    char *slash = strchr(fileName, '/');
    if (!slash)
	errAbort("Expected to find genome in file_name prefix.");
    safencat(genome, sizeof genome, fileName, slash - fileName);
    }
if (
    !sameString(genome, "dm3") &&
    !sameString(genome, "dm6") &&
    !sameString(genome, "ce10") &&
    !sameString(genome, "hg19") &&
    !sameString(genome, "hg20") &&
    !sameString(genome, "hg38") &&
    !sameString(genome, "mm9") &&
    !sameString(genome, "mm10") 
    )
    errAbort("unknown genome %s", genome);
return cloneString(genome);
}

char *getChromInfo(char *fileName)
/* Get path to chromInfo file for fileName */
{
char *genome = getGenome(fileName);
char chromInfo[256];
safef(chromInfo, sizeof chromInfo, "%s/%s/chrom.sizes", encValData, genome);
return cloneString(chromInfo);
}

char *getTwoBit(char *fileName)
/* Get path to twoBit file for fileName */
{  
// TODO this could use some more development
// Maybe in future can download this from one of our servers?
char *genome = getGenome(fileName);
char twoBit[256];
safef(twoBit, sizeof twoBit, "%s/%s/%s.2bit", encValData, genome, genome);
return cloneString(twoBit);
}

char *getBamBai(char *fileName)
/* Get path to bam index for fileName */
{  
char bamBai[256];
safef(bamBai, sizeof bamBai, "%s.bai", fileName);
return cloneString(bamBai);
}


boolean runCmdLine(char *cmdLine)
/* Run command line */
{
// TODO this should be substantially more complex
//   with exec with timeout, might want to just translate
//   some of the exec with wait code from the old ENCODE2 pipeline
//   Maybe the default timeout should be 8 hours.
//   I am sure that is more than generous enough for validating a single big file.
verbose(1, "cmdLine=[%s]\n",cmdLine);
int retCode = system(cmdLine); 
verbose(2, "retCode=%d\n", retCode);   // note 0 = success, 65280 = exit(255) or exit(-1) which is usually errAbort.
sleep(1); // give stupid gzip broken pipe errors a chance to happen and print out to stderr
return (retCode == 0);
}

boolean validateBam(char *fileName)
/* Validate BAM file */
{
char *twoBit = getTwoBit(fileName);
char *chromInfo = getChromInfo(fileName);
char cmdLine[1024];
int mismatches = 7;  // TODO this is totally arbitrary right now

// run validator on BAM even if the twoBit is not available.
boolean hasTwoBit = fileExists(twoBit); 
if (hasTwoBit)
    {
    safef(cmdLine, sizeof cmdLine, "%svalidateFiles -type=bam -mismatches=%d -chromInfo=%s -genome=%s %s", 
	validateFilesPath, mismatches, chromInfo, twoBit, fileName);
    }
else
    {
    // simple existence check for the corresponding .bam.bai since without -genome=, 
    //  vf will not even open the bam index.
    /* I thought this was good to check anyway since labs should be using bam with index in a hub, 
	but Eurie seems to think not, so commenting out for now.
    char *bamBai = getBamBai(fileName);
    if (!fileExists(bamBai))
	{
	warn("Bam Index file missing: %s. Use SAM Tools to create.", bamBai);
	return FALSE;
	}
    */
    // QUICK-run by removing -genome and mismatches and stuff.
    safef(cmdLine, sizeof cmdLine, "%svalidateFiles -type=bam -chromInfo=%s %s", 
	validateFilesPath, chromInfo, fileName);
    }
return runCmdLine(cmdLine);
}


boolean validateBedNP(char *fileName, char *format, boolean plainBed, int bedN, int bedP)
/* Validate bedN+P file */
{
char *chromInfo = getChromInfo(fileName);
char asFileName[256];
safef(asFileName, sizeof asFileName, "%s.as", format);
char *asFile = getAs(asFileName);
char cmdLine[1024];
char *bedType = "bigBed";
if (plainBed)
    bedType = "bed";
safef(cmdLine, sizeof cmdLine, "%svalidateFiles -type=%s%d+%d -as=%s -chromInfo=%s %s", validateFilesPath, bedType, bedN, bedP, asFile, chromInfo, fileName);
return runCmdLine(cmdLine);
}

boolean validateBigBed(char *fileName)
/* Validate bigBed file */
{

warn("error FILENAME %s: generic format bigBed is not allowed. Please use a specific format for the kind of bed data.", fileName);
return FALSE;

char *asFile = getAs("modPepMap-std.as");  // TODO this wrong but how do we know what to put here?
char *chromInfo = getChromInfo(fileName);
char cmdLine[1024];
// TODO probably need to do more work to define what the right type= and .as is
//  going to be, and how to get it.
// The following line is nothing but pure hack taken from the first example found in the manifest,
//  and probably will fail miserably on other lines of the manifest, as this approach is too simple to work still
safef(cmdLine, sizeof cmdLine, "%svalidateFiles -type=bigBed12+4 -as=%s -chromInfo=%s %s", validateFilesPath, asFile, chromInfo, fileName);
// TODO actually run the validator
return runCmdLine(cmdLine);
}

boolean validateBigWig(char *fileName)
/* Validate bigWig file */
{
char *chromInfo = getChromInfo(fileName);
char cmdLine[1024];
safef(cmdLine, sizeof cmdLine, "%svalidateFiles -type=bigWig -chromInfo=%s %s", validateFilesPath, chromInfo, fileName);
return runCmdLine(cmdLine);
}

boolean validateFastq(char *fileName)
/* Validate fastq file */
{
// check if fastq fileName ends in .gz extension
if (!endsWith(fileName, ".gz"))
    {
    warn("FILENAME %s must be compressed only with gzip and have the extension .gz", fileName);
    return FALSE;
    }
char cmdLine[1024];
safef(cmdLine, sizeof cmdLine, "%svalidateFiles -type=fastq %s", validateFilesPath, fileName);
return runCmdLine(cmdLine);
}

boolean validateGtf(char *fileName)
/* Validate gtf file */
{
uglyf("GTF: very basic checking only performed.\n");
/* Open and read file with generic GFF reader and check it is GTF */
struct gffFile *gff = gffRead(fileName);
if (!gff->isGtf)
    {
    warn("file (%s) is not in GTF format - check it has gene_id and transcript_id", fileName);
    return FALSE;
    }
// TODO actually run a more complete check
return TRUE;
}

boolean validateRcc(char *fileName)
/* Validate RCC file */
{
char cmdLine[1024];
safef(cmdLine, sizeof cmdLine, "%svalidateFiles -type=rcc %s", validateFilesPath, fileName);
return runCmdLine(cmdLine);
}
 
boolean validateIdat(char *fileName)
/* Validate IDAT file */
{
char cmdLine[1024];
safef(cmdLine, sizeof cmdLine, "%svalidateFiles -type=idat %s", validateFilesPath, fileName);
return runCmdLine(cmdLine);
}
 
boolean validateFasta(char *fileName)
/* Validate fasta file */
{
char cmdLine[1024];
safef(cmdLine, sizeof cmdLine, "%svalidateFiles -type=fasta %s", validateFilesPath, fileName);
return runCmdLine(cmdLine);
}
 
boolean validateUnknown(char *fileName)
/* Validate Unknown type file */
{
warn("File %s has type unknown", fileName);
return TRUE;
}
 

boolean validateFile(char *fileName, char *format)
/* call validateFiles for the file and format */
{
boolean result = FALSE;
if (endsWith(fileName, ".tgz"))  // TODO how to handle .tgz tar'd fasta files.
    { // will encode3 really even need to support these at all?
      // and if it does, would we have vf support tar archive natively,
      // or have vm (this program) unpack it and call vf for each file found inside?
    warn(".tgz format not currently supported by validateManifest");
    return FALSE;
    }

// Handle support for plain BEDs that are not bigBeds.
boolean plainBed = FALSE;
if (startsWith("bed_", format))
    {
    format += strlen("bed_");  // skip the prefix.
    plainBed = TRUE;
    }

// Call the handler based on format
if (sameString(format,"bam"))
    result = validateBam(fileName);
else if (startsWith(format,"bigBed")) // Actually this generic type will be rejected.
    result = validateBigBed(fileName);
else if (startsWith(format,"bigWig"))
    result = validateBigWig(fileName);
else if (startsWith(format,"fastq"))
    result = validateFastq(fileName);
else if (startsWith(format,"gtf"))
    result = validateGtf(fileName);
else if (startsWith(format,"rcc"))
    result = validateRcc(fileName);
else if (startsWith(format,"idat"))
    result = validateIdat(fileName);
else if (startsWith(format,"fasta"))
    result = validateFasta(fileName);
else if (startsWith(format,"unknown"))
    result = validateUnknown(fileName);
else 
    {
    struct encode3BedType *bedType = encode3BedTypeMayFind(format);
    if (bedType != NULL)
        {
	result = validateBedNP(fileName, format, plainBed, 
	    bedType->bedFields, bedType->extraFields);
	}
    else
        {
	warn("Unrecognized format: %s", format);
	result = FALSE;
	}
    }
return result;
}


boolean disallowedCompressionExtension(char *fileName, char *format)
/* return TRUE if fileName ends in a disallowed extension */
{
if (endsWith(fileName, ".tgz"))
    return TRUE;
if (endsWith(fileName, ".tar.gz"))
    return TRUE;
if (endsWith(fileName, ".tar"))
    return TRUE;
if (endsWith(fileName, ".zip"))
    return TRUE;
if (endsWith(fileName, ".bz2"))
    return TRUE;
if (endsWith(fileName, ".gz") && !sameString(format,"fastq"))
    return TRUE;
return FALSE;
}


int validateManifest(char *workingDir)
/* Validate the manifest.txt input file creating validated.txt output */
{

chdir(workingDir);
if (!fileExists("manifest.txt"))
    {
    warn("manifest.txt not found in workingDir %s", workingDir);
    usage();
    }

uglyf("workingDir=%s\n\n", workingDir);

if (fileExists("validateFiles"))
    validateFilesPath = "./";


char *fakeMd5sum = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
if (quickMd5sum)
    uglyf("DEBUG: because md5sum calculations are slow for big files, for testing purposes big files will be assigned md5sum=%s\n", fakeMd5sum);


struct slRecord *manifestFields = NULL;
struct slRecord *manifestRecs = NULL;

uglyf("reading manifest.txt\n\n");
int mFieldCount = readManifest("manifest.txt", &manifestFields, &manifestRecs);

struct slRecord *vFields = NULL;
struct slRecord *vRecs = NULL;

boolean haveVal = FALSE;
int vFieldCount = -1;

if (fileExists("validated.txt"))  // read in the old validated.txt file to save time
    {
    uglyf("reading validated.txt\n\n");
    vFieldCount = readManifest("validated.txt", &vFields, &vRecs);
    if (vFieldCount != mFieldCount + 4) // TODO this might be allowed someday if good case exists for it.
	errAbort("ERROR: the number of fields in validated.txt %d does not match the number of fields %d in manifest.txt", vFieldCount, mFieldCount);
    haveVal = TRUE;
    }



int mUcscDbIdx = -1;    // optional field ucsc_db
int mFileNameIdx = -1;
int mFormatIdx = -1;
int mOutputTypeIdx = -1;
int mExperimentIdx = -1;
int mReplicateIdx = -1;
int mTechnicalReplicateIdx = -1;
int mEnrichedInIdx = -1;
int mPairedEndIdx = -1;
int i = 0;
// find field numbers needed for required fields.
for (i=0; i<mFieldCount; ++i)
    {
    if (sameString(manifestFields->words[i], "ucsc_db"))
	mUcscDbIdx = i;
    if (sameString(manifestFields->words[i], "file_name"))
	mFileNameIdx = i;
    if (sameString(manifestFields->words[i], "format"))
	mFormatIdx = i;
    if (sameString(manifestFields->words[i], "output_type"))
	mOutputTypeIdx = i;
    if (sameString(manifestFields->words[i], "experiment"))
	mExperimentIdx = i;
    if (sameString(manifestFields->words[i], "replicate"))
	mReplicateIdx = i;
    if (sameString(manifestFields->words[i], "technical_replicate"))
	mTechnicalReplicateIdx = i;
    if (sameString(manifestFields->words[i], "enriched_in"))
	mEnrichedInIdx = i;
    if (sameString(manifestFields->words[i], "paired_end"))
	mPairedEndIdx = i;
    }
if (mFileNameIdx == -1)
    errAbort("field file_name not found in manifest.txt");
if (mFormatIdx == -1)
    errAbort("field format not found in manifest.txt");
if (mOutputTypeIdx == -1)
    errAbort("field output_type not found in manifest.txt");
if (mExperimentIdx == -1)
    errAbort("field experiment not found in manifest.txt");
if (mReplicateIdx == -1)
    errAbort("field replicate not found in manifest.txt");
// technical_replicate is probably optional
//if (mTechnicalReplicateIdx == -1)
//    errAbort("field technical_replicate not found in manifest.txt");
if (mEnrichedInIdx == -1)
    errAbort("field enriched_in not found in manifest.txt");
// paired_end is probably optional
//if (mPairedEndIdx == -1)
//    errAbort("field paired_end not found in manifest.txt");

// check if the fieldnames in old validated appear in the same order in manifest.txt
//  although this is currently a minor limitation, it could be removed 
//  with just a little work in future if needed.
if (haveVal)
    for (i = 0; i < mFieldCount; ++i)
	{
	if (!sameString(manifestFields->words[i], vFields->words[i]))
	    errAbort("field names in old validated.txt do not match those in manifest.txt");
	}
// get indexes for old val extra fields
int vMd5SumIdx = -1;
int vSizeIdx = -1;
int vModifiedIdx = -1;
int vValidKeyIdx = -1;
if (haveVal)
    {
    for (i = mFieldCount; i < vFieldCount; ++i)
	{
	if (sameString(vFields->words[i], "md5_sum"))
	    vMd5SumIdx = i;
	if (sameString(vFields->words[i], "size"))
	    vSizeIdx = i;
	if (sameString(vFields->words[i], "modified"))
	    vModifiedIdx = i;
	if (sameString(vFields->words[i], "valid_key"))
	    vValidKeyIdx = i;
	}
    if ( vMd5SumIdx   == -1) errAbort("field "  "md5_sum not found in old validated.txt");
    if ( vSizeIdx      == -1) errAbort("field "     "size not found in old validated.txt");
    if ( vModifiedIdx  == -1) errAbort("field " "modified not found in old validated.txt");
    if ( vValidKeyIdx == -1) errAbort("field ""valid_key not found in old validated.txt");
    }

// calling for the side-effect of checking for duplicate file_names.
// ignore return code
(void) makeFileNameHash(manifestRecs, mFileNameIdx);

// hash old validated records by file_name for quick lookup.
struct hash *valHash = NULL;
if (haveVal)
    valHash = makeFileNameHash(vRecs, mFileNameIdx);


// open output
// write to a different temp filename so that the old validated.txt is not lost if this program not complete
FILE *f = mustOpen("validated.tmp", "w"); 

char *tabSep = "";
// write fieldnames to output
fprintf(f,"#");  // write leading comment character #
for (i = 0; i < mFieldCount; ++i)
    {
    fprintf(f, "%s%s", tabSep, manifestFields->words[i]);
    tabSep = "\t";
    }
// include additional fieldnames
fprintf(f,"\tmd5_sum\tsize\tmodified\tvalid_key");
fprintf(f,"\n");

fprintf(f,"##validateManifest version %s\n", version);  // write vm version as a comment

// hash for output_type checking for unique format 
// catches problem some users were using the same output_type on the wrong format.
struct hash *outputTypeHash = newHash(12);

// loop through manifest recs
boolean errsFound = FALSE;
struct slRecord *rec = NULL;
int recNo = 1;
for(rec = manifestRecs; rec; rec = rec->next)
    {
    /* DEBUG
    uglyf("rec #%d = [", recNo);
    int i = 0;
    for (i = 0; i < mFieldCount; ++i)
	{
	uglyf("\t%s", rec->words[i]);
	}
    uglyf("]\n");
    */

    boolean fileIsValid = TRUE; // Default to valid until we know otherwise.

    // get file_name, size, datetime
    char *mFileName = rec->words[mFileNameIdx];
    if (mUcscDbIdx != -1)
	ucscDb = rec->words[mUcscDbIdx];

    off_t mFileSize = 0;
    time_t mFileTime = 0;
    char *mMd5Hex = "0";
    char *mValidKey = "ERROR";

    // check that ucsc db, if used, is not blank
    if (fileIsValid && mUcscDbIdx != -1 && ucscDb[0] == 0)
	{
	fileIsValid = FALSE;
	printf("ERROR: ucsc_db must not be blank.\n");
	}		    

    // check that the file exists
    if (fileIsValid && !fileExists(mFileName))
	{
	fileIsValid = FALSE;
	printf("ERROR: '%s' FILE NOT FOUND !!!\n", mFileName);
	}

    char *mFormat = rec->words[mFormatIdx];

    // check that the format is not blank
    if (fileIsValid && sameString(mFormat,""))
	{
	fileIsValid = FALSE;
	printf("ERROR: format must not be blank.\n");
	}

    // check that the file extension is not disallowed
    if (fileIsValid && disallowedCompressionExtension(mFileName, mFormat))
	{
	fileIsValid = FALSE;
	printf("ERROR: %s FILE COMPRESSION TYPE NOT ALLOWED with format %s !!!\n", mFileName, mFormat);
	}


    // check that each output_type is not blank and only used with one format
    char *mOutputType = rec->words[mOutputTypeIdx];
    if (fileIsValid && mOutputType[0] == 0)
	{
	fileIsValid = FALSE;
	printf("ERROR: output_type must not be blank.\n");
	}		    
    if (fileIsValid)
	{
	struct hashEl *el = hashLookup(outputTypeHash, mOutputType);
	if (el)
	    {
	    char *existingFormat = (char *) el->val;
	    if (!sameString(mFormat, existingFormat))
		{
		fileIsValid = FALSE;
		printf("ERROR: Each output_type can only be used with one format.  output_type %s is being used with both format %s and %s.\n",
		    mOutputType, mFormat, existingFormat);
		}		    
	    }
	else
	    hashAdd(outputTypeHash, mOutputType, cloneString(mFormat));
	}

    // check experiment field
    char *mExperiment = rec->words[mExperimentIdx];
    if (fileIsValid)
	{
	if (!startsWith("ENCSR", mExperiment))
	    {
	    fileIsValid = FALSE;
	    printf("ERROR: %s is not a valid value for the experiment field.  Must start with ENCSR.\n", mExperiment);
	    }
	}

    // check enriched_in field
    char *mEnrichedIn = rec->words[mEnrichedInIdx];
    if (fileIsValid)
	{
	if (!encode3CheckEnrichedIn(mEnrichedIn))
	    {
	    fileIsValid = FALSE;
	    printf("ERROR: %s is not a valid value for the enriched_in field.\n", mEnrichedIn);
	    }
	}

    
    // check replicate field
    char *mReplicate = rec->words[mReplicateIdx];
    if (fileIsValid)
	{
	boolean smallNumber = FALSE;
	int sl = strlen(mReplicate);
	int sn = 0;
	if (countLeadingDigits(mReplicate) == sl && sl <= 2 && sl > 0)
	    {
	    smallNumber = TRUE;
	    sn = atoi(mReplicate);
	    }
       	if (!(startsWith("pooled", mReplicate) || startsWith("n/a", mReplicate) || (smallNumber && sn >=1 && sn <=10)))
	    {
	    fileIsValid = FALSE;
    	    printf("ERROR: %s is not a valid value for the replicate field.  "
		"Must be pooled or n/a or a small unsigned number 1 <= N <=10.\n", mReplicate);
	    }
	}
    
    // check technical_replicate field
    if (fileIsValid)
	{
	if (mTechnicalReplicateIdx != -1)  // The technical_replicate field is optional
	    {
	    char *mTechnicalReplicate = rec->words[mTechnicalReplicateIdx];
	    boolean smallNumber = FALSE;
	    int sl = strlen(mTechnicalReplicate);
	    int sn = 0;
	    if (countLeadingDigits(mTechnicalReplicate) == sl && sl <= 2 && sl > 0)
		{
		smallNumber = TRUE;
		sn = atoi(mTechnicalReplicate);
		}
	    if (!(startsWith("pooled", mTechnicalReplicate) || startsWith("n/a", mTechnicalReplicate) || (smallNumber && sn >=1 && sn <=10)))
		{
		fileIsValid = FALSE;
		printf("ERROR: %s is not a valid value for the technical_replicate field.  "
		    "Must be pooled or n/a or a small unsigned number 1 <= N <=10.\n", mTechnicalReplicate);
		}
	    }
	}
    
    // check paired_end field
    if (fileIsValid)
	{
	if (mPairedEndIdx != -1)  // The check paired_end field is optional
	    {
	    char *mPairedEnd = rec->words[mPairedEndIdx];
	    boolean smallNumber = FALSE;
	    int sl = strlen(mPairedEnd);
	    int sn = 0;
	    if (countLeadingDigits(mPairedEnd) == sl && sl < 2 && sl > 0)
		{
		smallNumber = TRUE;
		sn = atoi(mPairedEnd);
		}
	    if (!(startsWith("pooled", mPairedEnd) || startsWith("n/a", mPairedEnd) || (smallNumber && (sn==1 || sn ==2))))
		{
		fileIsValid = FALSE;
		printf("ERROR: %s is not a valid value for the paired_end field.  Must be 1 (forward), 2 (reverse) or \"n/a\".\n", mPairedEnd);
		}
	    }
	else
	    {
	    if (sameString(mFormat, "fastq"))  // The check paired_end field is required for fastq
		{
		fileIsValid = FALSE;
		printf("ERROR: For format fastq the paired_end field is required.  Must be 1 (forward), 2 (reverse) or \"n/a\".\n");
		}
	    }
	}
    


    if (fileIsValid)
	{

	mFileSize = fileSize(mFileName);
	mFileTime = fileModTime(mFileName);

	char *vMd5Hex = NULL;
	char *vValidKey = NULL;
	
	boolean dataMatches = FALSE;
	// look for a matching record in old validated
	if (haveVal)
	    {
	    off_t vFileSize = 0;
	    time_t vFileTime = 0;
	    struct slRecord *vRec = (struct slRecord *) hashFindVal(valHash, rec->words[mFileNameIdx]);
	    // check if all fields match between manifest and old validated
	    if (vRec)
		{
		dataMatches = TRUE;
		// check that the fields values match
		for (i = 0; i < mFieldCount; ++i)
		    {
		    if (!sameString(rec->words[i], vRec->words[i]))
			dataMatches = FALSE;
		    }
		// check that the record correctly matches the actual file sizes.
		if (dataMatches)
		    {
		    vFileSize = sqlLongLong(vRec->words[vSizeIdx]);  // TODO maybe use my special functions from the validator
		    if (vFileSize != mFileSize) dataMatches = FALSE;
		    }
		// check that the record correctly matches the actual file timestamp.
		if (dataMatches)
		    {
		    vFileTime = sqlLongLong(vRec->words[vModifiedIdx]);  // There is no sqlLong function, but there should be!
		    if (vFileTime != mFileTime) dataMatches = FALSE;
		    }
		// verify vValidKey against vMd5Hex.
		if (dataMatches)
		    {
		    vMd5Hex   = vRec->words[vMd5SumIdx];
		    vValidKey = vRec->words[vValidKeyIdx];
		    char *checkValidKey = encode3CalcValidationKey(vMd5Hex, vFileSize);
		    if (sameString(vValidKey,"ERROR")) 
			{
			dataMatches = FALSE;
			}
		    else if (!sameString(vValidKey,checkValidKey)) 
			{
			warn("invalid key %s in old validated.txt",vValidKey);  // TODO add line# or filename etc?
			dataMatches = FALSE;
			}
		    }
		
		}
	    }

	if (dataMatches)
	    {
	    mMd5Hex = vMd5Hex;
	    mValidKey = vValidKey;
	    }
	else
	    {
	    // get md5_sum
	    if (quickMd5sum && mFileSize > 100 * 1024 * 1024)
		mMd5Hex = fakeMd5sum;  // "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	    else
		{
		verbose(1, "Running md5 checksum on %s\n", mFileName);
		mMd5Hex = md5HexForFile(mFileName);
		}

	    mValidKey = encode3CalcValidationKey(mMd5Hex, mFileSize);

	    fileIsValid = validateFile(mFileName, mFormat); // Call the validator on the file and format.
	    if (!fileIsValid)
		mValidKey = "ERROR";

	    }

	}


    printf("mFileName = %s size=%lld time=%ld md5=%s validKey=%s\n", 
	mFileName, (long long)mFileSize, (long)mFileTime, mMd5Hex, mValidKey);
    printf("\n");

    // write to output validated.tmp
    tabSep = "";
    for (i = 0; i < mFieldCount; ++i)
	{
	fprintf(f, "%s%s", tabSep, rec->words[i]);
	tabSep = "\t";
	}
    // include additional fields
    fprintf(f,"\t%s\t%lld\t%ld\t%s", mMd5Hex, (long long)mFileSize, (long)mFileTime, mValidKey);
    fprintf(f,"\n");
    fflush(f);

    if (sameString(mValidKey,"ERROR"))
	errsFound = TRUE;

    ++recNo;
    }


carefulClose(&f);
rename("validated.tmp", "validated.txt"); // replace the old validated file with the new one

if (errsFound)
    return 1;

// #file_name      format  experiment      replicate       output_type     biosample       target  localization    update
// ucsc_db   (this is optional but overrides attempts to get db from file_name path)
// md5_sum size modified valid_key

return 0;

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc!=1 || optionExists("-help"))
    usage();

workingDir = optionVal("dir", workingDir);
encValData = optionVal("encValData", encValData);
quickMd5sum = optionExists("quickMd5sum");

return validateManifest(workingDir);

}
