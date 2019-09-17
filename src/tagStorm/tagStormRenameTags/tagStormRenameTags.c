/* tagStormRenameTags - rename tags in a tagStorm file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "portable.h"
#include "dystring.h"
#include "tagSchema.h"

boolean clNoArray = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormRenameTags - rename tags in a tagStorm file\n"
  "usage:\n"
  "   tagStormRenameTags in.tags 2column.tab out.tags\n"
  "where in.tags is the input tagstorm file, out.tags the output\n"
  "and 2column.tab is a tab or white space separated file where the\n"
  "first column is the old tag name, and the second the new tag name.\n"
  "   If in.tags and out.tags are the same, it will create a backup of\n"
  "in.tags in in.tags.bak\n"
  "Options:\n"
  "   -noArray - if set then don't treat .1. .2. and so forth as array indexes\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"noArray", OPTION_BOOLEAN},
   {NULL, 0},
};

void tagStormRenameTags(char *inName, char *twoColName, char *outName)
/* tagStormRenameTags - rename tags in a tagStorm file. */
{
char *sourceName = inName;
if (sameString(inName, outName))
    {
    char backupName[PATH_LEN];
    safef(backupName, sizeof(backupName), "%s.bak", inName);
    sourceName = backupName;
    mustRename(inName, backupName);
    }
// Open up input as a lineFile
struct lineFile *lf = lineFileOpen(sourceName, TRUE);

// Read twoColName into a hash
struct hash *hash = hashTwoColumnFile(twoColName);

// Set up some variables that'll help us process tags that are fields from arrays
int indexes[16];
struct dyString *bracketedDy = dyStringNew(0);
struct dyString *numberedDy = dyStringNew(0);

// Open up output as a FILE
FILE *f = mustOpen(outName, "w");

// Loop through input lines.  Write them out unchanged unless first real word matches
// something in our hash.
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    // See if a blank line or starts with # - these are comments and record separators passed through
    char *s = skipLeadingSpaces(line);
    int leadingSpaces = s - line;
    char c = s[0];
    if (c == '#' || c == 0)
         fprintf(f, "%s\n", line);
    else
        {
	// Parse out first word and make a variable to store the revised version
	char *tag = nextWord(&s);
	char *newTag = NULL;

	// Handle array bits which are a bit complex, then easy non-array case to find new symbol
	int indexCount = 0;
	if (!clNoArray)
	    indexCount = tagSchemaParseIndexes(tag, indexes, ArraySize(indexes));
	if (indexCount > 0)
	    {
	    char *oldBracketed = tagSchemaFigureArrayName(tag, bracketedDy);
	    char *newBracketed = hashFindVal(hash, oldBracketed);
	    if (newBracketed != NULL)
		newTag = tagSchemaInsertIndexes(newBracketed, indexes, indexCount, numberedDy);
	    }
	else
	    newTag = hashFindVal(hash, tag);

	if (newTag == NULL)
	     newTag = tag;   // Just replace ourselves with ourselves
        mustWrite(f, line, leadingSpaces);
	fprintf(f, "%s %s\n", newTag, emptyForNull(s));
	}
    }

// Close output
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
clNoArray = optionExists("noArray");
tagStormRenameTags(argv[1], argv[2], argv[3]);
return 0;
}
