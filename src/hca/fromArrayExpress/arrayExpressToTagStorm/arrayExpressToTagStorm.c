/* arrayExpressToTagStorm - Convert from ArrayExpress idf.txt + sdrf.txt format to tagStorm.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
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
	if (c == ']')
	    continue;
	c = '_';
	if (c == '_' && lastC == '_')
	    {
	    uglyf("Skipping double _ in %s", aeName);
	    continue;
	    }
	}
    outName[outSize] = c;
    if (++outSize >= outMaxSize)
	errAbort("aeName %s too long", aeName);
    lastC = c;
    }
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


/* Parse lines from idf file into stanza */
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
struct dyString *csvEscaper = dyStringNew(0), *dyVal = dyStringNew(0);
char *csvEscapeToDyString(struct dyString *dy, char *string);
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
        errAbort("Expecting at least 2 fields but only got %d line %d of %s",
	    rowSize, lf->lineIx, lf->fileName);

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
	if (additionalFileDy->stringSize != 0)
	     dyStringAppendC(additionalFileDy, ',');
	char *escaped = csvEscapeToDyString(csvEscaper, row[1]);
	dyStringAppend(additionalFileDy, escaped);
	}
    else
	{
	/* Convert rest of elements to possibly comma separated values */
	dyStringClear(dyVal);
	int i;
	for (i=1; i<rowSize; ++i)
	    {
	    if (i > 1)
		dyStringAppendC(dyVal, ',');
	    char *escaped = csvEscapeToDyString(csvEscaper, row[i]);
	    dyStringAppend(dyVal, escaped);
	    }
	tagStanzaAppend(storm, stanza, tagName, dyVal->string);
	}
    }
if (additionalFileDy->stringSize != 0)
     tagStanzaAppend(storm, stanza, additionalFilePrefix, additionalFileDy->string);
dyStringFree(&additionalFileDy);
dyStringFree(&csvEscaper);
dyStringFree(&dyVal);
return storm;
}

void arrayExpressToTagStorm(char *idfName, char *sdrfName, char *outName)
/* arrayExpressToTagStorm - Convert from ArrayExpress idf.txt + sdrf.txt format to tagStorm.. */
{
struct tagStorm *storm = idfToStormTop(idfName);
// addSdrfToStormTop(sdrfName, storm);

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
