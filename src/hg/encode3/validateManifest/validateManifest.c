/* validate ENCODE3 manifest.txt creating output validate.txt */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "md5.h"
#include "hex.h"
#include "sqlNum.h"

char *version = "1.0";
char *workingDir = ".";

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
    "\n"
    "   Input files in the working directory: \n"
    "     manifest.txt - current input manifest file\n"
    "     validate.txt - input from previous run of validateManifest\n" 
    "\n"
    "   Output file in the working directory: \n"
    "     validate.txt - results of validated input\n"
    "\n"
    , version
    );
}

static struct optionSpec options[] = {
    {"dir", OPTION_STRING},
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

boolean firstTime = TRUE;
lineFileSetUniqueMetaData(lf);  // this seems to be the only way to save the comments with linefile

while (lineFileNextReal(lf, &row))
    {
    if (firstTime)
	{
	firstTime = FALSE;
	// grab fieldnames from metadata
	char *metaLine = NULL;
	struct hash *hash = lf->metaLines;
	int i;
	for (i=0; i<hash->size; ++i)
	    {
	    if (hash->table[i])
		{
		metaLine = cloneString(hash->table[i]->name);
		break;
		}
	    }
	if (!metaLine)
	    errAbort("Expected 1st line to contain a comment line listing field names.");
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

    //uglyf("%s\n", row); // DEBUG REMOVE

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

slReverse(&allRecs);
if (pAllRecs)
    *pAllRecs = allRecs;	    


lineFileClose(&lf);

return fieldCount;

}


char *calcValidKey(char *md5Hex, long long fileSize)
/* calculate validation key to prevent faking */
{
if (strlen(md5Hex) != 32)
    errAbort("Invalid md5Hex string: %s\n", md5Hex);
long long sum = 0;
sum += fileSize;
while (*md5Hex)
    {
    unsigned char n = hexToByte(md5Hex);
    sum += n;
    md5Hex += 2;
    }
int vNum = sum % 10000;
char buf[256];
safef(buf, sizeof buf, "V%d", vNum);
return cloneString(buf);
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

void validateManifest(char *workingDir)
/* Validate the manifest.txt input file creating validate.txt output */
{

chdir(workingDir);
uglyf("workingDir=%s\n", workingDir);

if (!fileExists("manifest.txt"))
    usage();


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



int m_file_name_i = -1;
int i = 0;
// find field numbers needed for required fields.
for (i=0; i<mFieldCount; ++i)
    {
    if (sameString(manifestFields->words[i], "file_name"))
	m_file_name_i = i;
    }
if (m_file_name_i == -1)
    errAbort("field file_name not found in manifest.txt");

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
int v_md5_sum_i = -1;
int v_size_i = -1;
int v_modified_i = -1;
int v_valid_key_i = -1;
if (haveVal)
    {
    for (i = mFieldCount; i < vFieldCount; ++i)
	{
	if (sameString(vFields->words[i], "md5_sum"))
	    v_md5_sum_i = i;
	if (sameString(vFields->words[i], "size"))
	    v_size_i = i;
	if (sameString(vFields->words[i], "modified"))
	    v_modified_i = i;
	if (sameString(vFields->words[i], "valid_key"))
	    v_valid_key_i = i;
	}
    if ( v_md5_sum_i   == -1) errAbort("field "  "md5_sum not found in old validated.txt");
    if ( v_size_i      == -1) errAbort("field "     "size not found in old validated.txt");
    if ( v_modified_i  == -1) errAbort("field " "modified not found in old validated.txt");
    if ( v_valid_key_i == -1) errAbort("field ""valid_key not found in old validated.txt");
    }

// calling for the side-effect of checking for duplicate file_names.
struct hash *mFileNameHash = NULL;  // split on two lines to suppress compiler warning : unused var
mFileNameHash = makeFileNameHash(manifestRecs, m_file_name_i);

// hash old validated records by file_name for quick lookup.
struct hash *valHash = NULL;
if (haveVal)
    valHash = makeFileNameHash(vRecs, m_file_name_i);


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
    char *mFileName = rec->words[m_file_name_i];

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
    	vRec = (struct slRecord *) hashFindVal(valHash, rec->words[m_file_name_i]);
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
    		vFileSize = sqlLongLong(vRec->words[v_size_i]);  // TODO maybe use my special functions from the validator
		if (vFileSize != mFileSize) dataMatches = FALSE;
		}
	    if (dataMatches)
		{
		vFileTime = sqlLongLong(vRec->words[v_modified_i]);  // There is no sqlLong function, but there should be!
		if (vFileTime != mFileTime) dataMatches = FALSE;
		}
	    // verify vValidKey against vMd5Hex.
	    if (dataMatches)
		{
		vMd5Hex   = vRec->words[v_md5_sum_i];
		vValidKey = vRec->words[v_valid_key_i];
		char *checkValidKey = calcValidKey(vMd5Hex, vFileSize);
		if (!sameString(vValidKey,checkValidKey)) 
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
    	//char *mMd5Hex = mMd5HexForFile(mFileName);   // DEBUG RESTORE
	// DEBUG REMOVE -- hack for speed for development.
	if (mFileSize > 100 * 1024 * 1024)
	    mMd5Hex = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    	else
	    mMd5Hex = md5HexForFile(mFileName);

	mValidKey = calcValidKey(mMd5Hex, mFileSize);


	boolean fileIsValid = FALSE;
	// TODO At this point it should call the validator on the file type.
	// DEBUG fake it for now!
	fileIsValid = TRUE;

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

    ++recNo;
    }

carefulClose(&f);
rename("validated.tmp", "validated.txt"); // replace the old validated file with the new one

// #file_name      format  experiment      replicate       output_type     biosample       target  localization    update
// md5_sum size modified valid_key

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc!=1)
    usage();

workingDir = optionVal("dir", workingDir);

validateManifest(workingDir);

return 0;
}
