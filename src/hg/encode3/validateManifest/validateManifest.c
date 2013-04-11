/* validate ENCODE3 manifest.txt creating output validate.txt */

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

char *version = "1.1";
char *workingDir = ".";
char *encValData = "encValData";
char *ucscDb = NULL;

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
    "     validate.txt - input from previous run of validateManifest\n" 
    "\n"
    "   Output file in the working directory: \n"
    "     validate.txt - results of validated input\n"
    "\n"
    , version, encValData
    );
}

static struct optionSpec options[] = {
    {"dir", OPTION_STRING},
    {"encValData", OPTION_STRING},
    {"quickMd5sum", OPTION_BOOLEAN},     // Testing option, user should not use
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
	 startsWith("#ucsc_db", row))
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
verbose(2, "cmdLine=[%s]\n",cmdLine);
int retCode = system(cmdLine); 
verbose(2, "retCode=%d\n", retCode);
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
boolean quicky = fileExists(twoBit);  // QUICK-run by removing -genome and mismatches and stuff.
if (quicky)
    {
    // simple existence check for the corresponding .bam.bai since without -genome=, 
    //  vf will not even open the bam index.
    char *bamBai = getBamBai(fileName);
    if (!fileExists(bamBai))
	{
	warn("Bam Index file missing: %s. Use SAM Tools to create.", bamBai);
	return FALSE;
	}
    safef(cmdLine, sizeof cmdLine, "validateFiles -type=bam -chromInfo=%s %s", chromInfo, fileName);
    }
else
    safef(cmdLine, sizeof cmdLine, "validateFiles -type=bam -mismatches=%d -chromInfo=%s -genome=%s %s", mismatches, chromInfo, twoBit, fileName);
return runCmdLine(cmdLine);
}

boolean validateBedRnaElements(char *fileName)
/* Validate bedRnaElements file */
{
// TODO the current example manifest.txt is wrong because this should be bigBed-based (not bed-based)
//  so that we need to  change this into bigBed with a particular bedRnaElements.as ?
char *asFile = getAs("bedRnaElements.as");  // TODO this probably has to change
char *chromInfo = getChromInfo(fileName);
char cmdLine[1024];
safef(cmdLine, sizeof cmdLine, "validateFiles -type=bigBed6+3 -as=%s -chromInfo=%s %s", asFile, chromInfo, fileName);
return runCmdLine(cmdLine);
}

boolean validateBigBed(char *fileName)
/* Validate bigBed file */
{
char *asFile = getAs("modPepMap-std.as");  // TODO this wrong but how do we know what to put here?
char *chromInfo = getChromInfo(fileName);
char cmdLine[1024];
// TODO probably need to do more work to define what the right type= and .as is
//  going to be, and how to get it.
// The following line is nothing but pure hack taken from the first example found in the manifest,
//  and probably will fail miserably on other lines of the manifest, as this approach is too simple to work still
safef(cmdLine, sizeof cmdLine, "validateFiles -type=bigBed12+4 -as=%s -chromInfo=%s %s", asFile, chromInfo, fileName);
// TODO actually run the validator
return runCmdLine(cmdLine);
}

boolean validateBigWig(char *fileName)
/* Validate bigWig file */
{
char *chromInfo = getChromInfo(fileName);
char cmdLine[1024];
safef(cmdLine, sizeof cmdLine, "validateFiles -type=bigWig -chromInfo=%s %s", chromInfo, fileName);
return runCmdLine(cmdLine);
}

boolean validateFastq(char *fileName)
/* Validate fastq file */
{
char cmdLine[1024];
safef(cmdLine, sizeof cmdLine, "validateFiles -type=fastq %s", fileName);
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

boolean validateNarrowPeak(char *fileName)
/* Validate narrowPeak file */
{
char *asFile = getAs("narrowPeak.as");
char *chromInfo = getChromInfo(fileName);
char cmdLine[1024];
safef(cmdLine, sizeof cmdLine, "validateFiles -type=bigBed6+4 -as=%s -chromInfo=%s %s", asFile, chromInfo, fileName);
return runCmdLine(cmdLine);
}

boolean validateBroadPeak(char *fileName)
/* Validate broadPeak file */
{
char *asFile = getAs("broadPeak.as");
char *chromInfo = getChromInfo(fileName);
char cmdLine[1024];
safef(cmdLine, sizeof cmdLine, "validateFiles -type=bigBed6+3 -as=%s -chromInfo=%s %s", asFile, chromInfo, fileName);
return runCmdLine(cmdLine);
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

// Call the handler based on format
if (sameString(format,"bam"))
    result = validateBam(fileName);
else if (startsWith(format,"bedRnaElements"))
    result = validateBedRnaElements(fileName);
else if (startsWith(format,"bigBed"))
    result = validateBigBed(fileName);
else if (startsWith(format,"bigWig"))
    result = validateBigWig(fileName);
else if (startsWith(format,"fastq"))
    result = validateFastq(fileName);
else if (startsWith(format,"gtf"))
    result = validateGtf(fileName);
else if (startsWith(format,"narrowPeak"))
    result = validateNarrowPeak(fileName);
else if (startsWith(format,"broadPeak"))
    result = validateBroadPeak(fileName);
else
    {
    warn("Unknown format: %s", format);
    result = FALSE;
    }
return result;
}



void validateManifest(char *workingDir)
/* Validate the manifest.txt input file creating validate.txt output */
{

chdir(workingDir);
if (!fileExists("manifest.txt"))
    {
    warn("manifest.txt not found in workingDir %s", workingDir);
    usage();
    }

uglyf("workingDir=%s\n", workingDir);

char *fakeMd5sum = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
if (quickMd5sum)
    uglyf("DEBUG: because md5sum calculations are slow for big files, for testing purposes big files will be assigned md5sum=%s\n", fakeMd5sum);


struct slRecord *manifestFields = NULL;
struct slRecord *manifestRecs = NULL;

uglyf("reading manifest.txt\n");
int mFieldCount = readManifest("manifest.txt", &manifestFields, &manifestRecs);

struct slRecord *vFields = NULL;
struct slRecord *vRecs = NULL;

boolean haveVal = FALSE;
int vFieldCount = -1;

if (fileExists("validated.txt"))  // read in the old validated.txt file to save time
    {
    uglyf("reading validated.txt\n");
    vFieldCount = readManifest("validated.txt", &vFields, &vRecs);
    if (vFieldCount != mFieldCount + 4) // TODO this might be allowed someday if good case exists for it.
	errAbort("Error: the number of fields in validated.txt %d does not match the number of fields %d in manifest.txt", vFieldCount, mFieldCount);
    haveVal = TRUE;
    }



int mFileNameIdx = -1;
int mFormatIdx = -1;
int mUcscDbIdx = -1;    // optional field ucsc_db
int i = 0;
// find field numbers needed for required fields.
for (i=0; i<mFieldCount; ++i)
    {
    if (sameString(manifestFields->words[i], "file_name"))
	mFileNameIdx = i;
    if (sameString(manifestFields->words[i], "format"))
	mFormatIdx = i;
    if (sameString(manifestFields->words[i], "ucsc_db"))
	mUcscDbIdx = i;
    }
if (mFileNameIdx == -1)
    errAbort("field file_name not found in manifest.txt");
if (mFormatIdx == -1)
    errAbort("field format not found in manifest.txt");

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
struct hash *mFileNameHash = NULL;  // split on two lines to suppress compiler warning : unused var
mFileNameHash = makeFileNameHash(manifestRecs, mFileNameIdx);

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

fprintf(f,"#version %s\n", version);  // write vm version as a comment

// loop through manifest recs
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

    // get file_name, size, datetime
    char *mFileName = rec->words[mFileNameIdx];
    if (mUcscDbIdx != -1)
	ucscDb = rec->words[mUcscDbIdx];

    off_t mFileSize = fileSize(mFileName);
    off_t vFileSize = -1;
    time_t mFileTime = fileModTime(mFileName);
    time_t vFileTime = -1;

    char *mMd5Hex = NULL;
    char *mValidKey = NULL;
    char *vMd5Hex = NULL;
    char *vValidKey = NULL;
    
    boolean dataMatches = FALSE;
    // look for a matching record in old validated
    struct slRecord *vRec = NULL;
    if (haveVal)
	{
    	vRec = (struct slRecord *) hashFindVal(valHash, rec->words[mFileNameIdx]);
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
	    mMd5Hex = md5HexForFile(mFileName);

	mValidKey = encode3CalcValidationKey(mMd5Hex, mFileSize);

	char *mFormat = rec->words[mFormatIdx];
	boolean fileIsValid = validateFile(mFileName, mFormat); // Call the validator on the file and format.

	if (!fileIsValid)
	    mValidKey = "ERROR";

	}

    uglyf("mFileName = %s size=%lld time=%ld md5=%s validKey=%s\n", mFileName, (long long)mFileSize, (long)mFileTime, mMd5Hex, mValidKey);


    // write to output
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

    ++recNo;
    }


carefulClose(&f);
rename("validated.tmp", "validated.txt"); // replace the old validated file with the new one

// #file_name      format  experiment      replicate       output_type     biosample       target  localization    update
// ucsc_db   (this is optional but overrides attempts to get db from file_name path)
// md5_sum size modified valid_key

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc!=1)
    usage();

workingDir = optionVal("dir", workingDir);
encValData = optionVal("encValData", encValData);
quickMd5sum = optionExists("quickMd5sum");

validateManifest(workingDir);

return 0;
}
