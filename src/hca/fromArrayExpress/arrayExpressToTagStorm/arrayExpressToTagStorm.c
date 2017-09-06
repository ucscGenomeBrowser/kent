/* arrayExpressToTagStorm - Convert from ArrayExpress idf.txt + sdrf.txt format to tagStorm.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "fieldedTable.h"
#include "tagStorm.h"
#include "csv.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "arrayExpressToTagStorm - Convert from ArrayExpress idf.txt + sdrf.txt format to tagStorm.\n"
  "usage:\n"
  "   arrayExpressToTagStorm in.idf.txt in.sdrf.txt out.tags\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void aeFieldToNormalField(char *prefix, char *aeName, char *outName, int outMaxSize)
/* Convert something that may look like "Comment[Submitted Name]" or "Protocol Type"
 * or even "Comment[AdditionalFile:Data1]" to something with underbars in place of
 * spaces and other non alpha-numeric characters.  Run filter so only do one _ in a 
 * row. */
{
/* Sanity check and copy in prefix to output */
int prefixSize = strlen(prefix);
if (prefixSize >= outMaxSize)
    internalErr();
int outSize = prefixSize;
memcpy(outName, prefix, prefixSize);

/* Loop through doing conversion */
char c, lastC = 0, *in = aeName;
while ((c = *in++) != 0)
    {
    if (!isalnum(c))
	{
	if (outSize == prefixSize)
	    continue;	// Skip leading ones too
	if (c == ']')
	    continue;
	c = '_';
	if (c == '_' && lastC == '_')
	    {
	    continue;
	    }
	}
    outName[outSize] = c;
    if (++outSize >= outMaxSize)
	errAbort("aeName %s too long", aeName);
    lastC = c;
    }

// Trim trailing underbars
while (outName[outSize-1] == '_')
     --outSize;

outName[outSize] = 0;
}

struct tagStorm *idfToStormTop(char *fileName)
/* Convert an idf.txt format file to a tagStorm with a single top-level stanza */
{
/* Create a tag storm with one as yet empty stanza */
struct tagStorm *storm = tagStormNew(fileName);
struct tagStanza *stanza = tagStanzaNew(storm, NULL);

/* Some stuff to help turn File_Data1, File_Data2, etc to a comma separated list */
char *additionalFilePrefix = "idf.Comment_AdditionalFile_Data";
struct dyString *additionalFileDy = dyStringNew(0);

/* There can be multiple secondary accession tags, so handle these too */
char *secondaryAccessionTag = "idf.Comment_SecondaryAccession";
struct dyString *secondaryAccessionDy = dyStringNew(0);


/* Parse lines from idf file into stanza */
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
struct dyString *dyVal = dyStringNew(0);
while (lineFileNextReal(lf, &line))
    {
    /* Erase trailing tab... */
    eraseTrailingSpaces(line);

    /* Parse line into tab-separated array and make sure it's a reasonable size */
    char *row[256];
    int rowSize = chopTabs(line, row);
    if (rowSize == ArraySize(row))
        errAbort("Line %d of %s has too many fields", lf->lineIx, lf->fileName);
    if (rowSize < 2)
	continue;

    /* Convert first element to tagName */
    char tagName[256];
    aeFieldToNormalField("idf.", trimSpaces(row[0]), tagName, sizeof(tagName));

    /* Special case where we already are a comma separated list */
    if (sameString(tagName, "idf.Publication_Author_List"))
        {
	tagStanzaAppend(storm, stanza, tagName, row[1]);
	}
    else if (startsWith(additionalFilePrefix, tagName))
        {
	csvEscapeAndAppend(additionalFileDy, row[1]);
	}
    else if (sameString(secondaryAccessionTag, tagName))
        {
	csvEscapeAndAppend(secondaryAccessionDy, row[1]);
	}
    else
	{
	/* Convert rest of elements to possibly comma separated values */
	dyStringClear(dyVal);
	int i;
	for (i=1; i<rowSize; ++i)
	    csvEscapeAndAppend(dyVal, row[i]);
	tagStanzaAppend(storm, stanza, tagName, dyVal->string);
	}
    }
if (additionalFileDy->stringSize != 0)
     tagStanzaAppend(storm, stanza, additionalFilePrefix, additionalFileDy->string);
if (secondaryAccessionDy->stringSize != 0)
     tagStanzaAppend(storm, stanza, secondaryAccessionTag, secondaryAccessionDy->string);
dyStringFree(&secondaryAccessionDy);
dyStringFree(&additionalFileDy);
dyStringFree(&dyVal);
lineFileClose(&lf);
return storm;
}


boolean sameExceptForSome(char **a, char **b, int size, bool *some)
/* Go through a and b of given size.  Return TRUE if they are the same 
 * except for places where the some array it TRUE */
{
int i;
for (i=0; i<size; ++i)
    {
    if (!some[i])
        if (differentString(a[i], b[i]))
	    return FALSE;
    }
return TRUE;
}

void addSdrfToStormTop(char *sdrfFile, struct tagStorm *storm)
/* Add lines of sdrfFile as children of first top level stanza in storm. */
{
struct fieldedTable *table = fieldedTableFromTabFile(sdrfFile, sdrfFile, NULL, 0 );


/* Convert ArrayExpress field names to our field names */
int fieldIx;
char *lastNonTerm = NULL;
char *lastNonUnit = NULL;
for (fieldIx=0; fieldIx < table->fieldCount; fieldIx += 1)
    {
    char tagName[256];
    aeFieldToNormalField("sdrf.", table->fields[fieldIx], tagName, sizeof(tagName));
    if (lastNonTerm != NULL && sameString("sdrf.Term_Source_REF", tagName))
	 {
         safef(tagName, sizeof(tagName), "%s_Term_Source_REF", lastNonTerm);
	 table->fields[fieldIx] = lmCloneString(table->lm, tagName);
	 }
    else if (lastNonTerm != NULL && sameString("sdrf.Term_Accession_Number", tagName))
	 {
         safef(tagName, sizeof(tagName), "%s_Term_Accession_Number", lastNonTerm);
	 table->fields[fieldIx] = lmCloneString(table->lm, tagName);
	 }
    else if (lastNonUnit != NULL && startsWith("sdrf.Unit_", tagName))
         {
	 safef(tagName, sizeof(tagName), "%s_Unit", lastNonUnit);
	 lastNonTerm = lmCloneString(table->lm, tagName);
	 table->fields[fieldIx] = lastNonTerm;
	 }
    else
	 {
         lastNonTerm = lastNonUnit = lmCloneString(table->lm, tagName);
	 table->fields[fieldIx] = lastNonTerm;
	 }
    }


/* Make up fastq field indexes to handle processing of paired reads in fastq, which
 * take two lines of sdrf file. */
char *fieldsWithFastqs[] = 
/* Fields that contain the fastq file names */
    {
    "sdrf.Comment_FASTQ_URI",
    "sdrf.Comment_SUBMITTED_FILE_NAME",
    "sdrf.Scan_Name",
    };
boolean mightReuseStanza = TRUE;
bool *reuseMultiFields;  // If set this field can vary and line still reused
AllocArray(reuseMultiFields, table->fieldCount);
int i;
for (i=0; i<ArraySize(fieldsWithFastqs); ++i)
    {
    char *field = fieldsWithFastqs[i];
    int ix = stringArrayIx(field, table->fields, table->fieldCount);
    if (ix >=0)
	reuseMultiFields[ix] = TRUE;
    else if (i == 0)
	{
	mightReuseStanza = FALSE;
        break;	    // Make sure has first one if going to do paired read fastq processing
	}
    }


/* Make up a list and hash of fieldMergers to handle conversion of columns that occur
 * multiple times to a comma-separated list of values in a single column. */
struct fieldMerger
/* Something to help merge multiple columns with same name */
    {
    struct fieldMerger *next;	/* Next in list */
    char *name;	
    struct dyString *val;	/* Comma separated value */
    };
struct hash *fieldHash = hashNew(0);
struct fieldMerger *fmList = NULL;
for (fieldIx = 0; fieldIx < table->fieldCount; ++fieldIx)
    {
    char *fieldName = table->fields[fieldIx];
    if (hashLookup(fieldHash, fieldName) == NULL)
        {
	struct fieldMerger *fm;
	AllocVar(fm);
	fm->name = fieldName;
	fm->val = dyStringNew(0);
	slAddTail(&fmList, fm);
	hashAdd(fieldHash, fieldName, fm);
	}
    }

/* Grab top level stanza and make sure there is only one. */
struct tagStanza *topStanza = storm->forest;
if (topStanza == NULL || topStanza->next != NULL)
    internalErr();

/* Scan through table, making new stanzas for each row and hooking them into topStanza */
struct fieldedRow *fr, *lastFr = NULL;
struct tagStanza *stanza = NULL;
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    /* Empty out any existing vals */
    struct fieldMerger *fm;
    for (fm = fmList; fm != NULL; fm = fm->next)
	dyStringClear(fm->val);

    /* Add all non-empty values from this row to our fieldMergers. */
    char **row = fr->row;
    for (fieldIx = 0; fieldIx < table->fieldCount; ++fieldIx)
        {
	char *fieldName = table->fields[fieldIx];
	fm = hashMustFindVal(fieldHash, fieldName);
	char *val = row[fieldIx];
	if (!isEmpty(val))
	    csvEscapeAndAppend(fm->val, val);
	}

    /* If only the reuseMultiFields are varying, append to those values in previous stanza,
     * otherwise make a new stanza */
    if (mightReuseStanza && lastFr != NULL 
        && sameExceptForSome(lastFr->row, fr->row, table->fieldCount, reuseMultiFields))
	{
	int i;
	for (i=0; i<ArraySize(fieldsWithFastqs); ++i)
	    {
	    char *fieldName = fieldsWithFastqs[i];
	    if ((fm = hashFindVal(fieldHash, fieldName)) != NULL)
	        {
		char *newVal = fm->val->string;
		char *oldVal = tagMustFindVal(stanza, fieldName);
		int bothSize = strlen(newVal) + strlen(oldVal) + 1 + 1;
		char bothBuf[bothSize];
		safef(bothBuf, bothSize, "%s,%s", oldVal, newVal);
		tagStanzaUpdateTag(storm, stanza, fieldName, bothBuf);
		}
	    }
	}
    else
        {
	/* Output all nonempty vals to stanza */
	stanza = tagStanzaNew(storm, topStanza);
	for (fm = fmList; fm != NULL; fm = fm->next)
	    if (fm->val->stringSize > 0)
		tagStanzaAppend(storm, stanza, fm->name, fm->val->string);
	}

    lastFr = fr;
    }
slReverse(&topStanza->children);
}

void arrayExpressToTagStorm(char *idfName, char *sdrfName, char *outName)
/* arrayExpressToTagStorm - Convert from ArrayExpress idf.txt + sdrf.txt format to tagStorm.. */
{
struct tagStorm *storm = idfToStormTop(idfName);
addSdrfToStormTop(sdrfName, storm);

tagStormWrite(storm, outName, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
arrayExpressToTagStorm(argv[1], argv[2], argv[3]);
return 0;
}
