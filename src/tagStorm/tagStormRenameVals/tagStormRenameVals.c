/* tagStormRenameVals - Rename values for a particular tag. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormRenameVals - Rename values for a particular tag.\n"
  "usage:\n"
  "usage:\n"
  "   tagStormRenameVals in.tags 3column.tab out.tags\n"
  "where in.tags is the input tagstorm file, out.tags the output\n"
  "and 3column.tab is a tab or white space separated file where the\n"
  "first column is the tag name, and the second the old val, the third the new val.\n"
  "   If in.tags and out.tags are the same, it will create a backup of\n"
  "in.tags in in.tags.bak\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct renamer
/* Help rename things */
    {
    struct renamer *next;  // next in list
    char *oldVal;   // old value
    char *newVal;   // new value
    };

struct hash *readThreeCol(char *fileName)
/* Read in three column file and return it as a hash of renamers keyed by tagName */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = hashNew(0);
char *row[4];
int rowSize;
int renameCount = 0;
char *line;
while (lineFileNextReal(lf, &line))
    {
    /* Make sure really have three fields. */
    ++renameCount;
    rowSize = chopTabs(line, row);
    if (rowSize != 3)
        errAbort("Expecting three tab separated fields, got %d line %d of %s\n", 
	    rowSize, lf->lineIx, lf->fileName);

    /* Create renamer object. */
    struct renamer *renamer;
    AllocVar(renamer);
    renamer->oldVal = cloneString(row[1]);
    renamer->newVal = cloneString(row[2]);
    
    /* Find tag in hash. If it doesn't exist make new hash item, otherwise just add to existing one. */
    char *tag = row[0];
    struct hashEl *hel = hashLookup(hash, tag);
    if (hel == NULL)
	hashAdd(hash, tag, renamer);
    else
	slAddHead(&hel->val, renamer);
    }
lineFileClose(&lf);
verbose(1, "Got %d renames on %d tags in %s\n", renameCount, hash->elCount, fileName);
return hash;
}

void tagStormRenameVals(char *inName, char *threeColName, char *outName)
/* tagStormRenameVals - Rename values for a particular tag. */
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

// Read threColName into a hash
struct hash *hash = readThreeCol(threeColName);

// Open up output as a FILE
FILE *f = mustOpen(outName, "w");

// Loop through input lines.  Write them out unchanged unless first real word matches
// something in our hash.
char *line;
int subCount = 0;
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
	// Parse out first word
	char *tag = nextWord(&s);
	s = emptyForNull(s);
	struct renamer *renamer = hashFindVal(hash, tag);
	if (renamer != NULL)
	    {
	    while (renamer != NULL)
	        {
		if (sameString(renamer->oldVal, s))
		    {
		    s = renamer->newVal;
		    ++subCount;
		    break;
		    }
		renamer = renamer->next;
		}
	    }
        mustWrite(f, line, leadingSpaces);
	fprintf(f, "%s %s\n", tag, s);
	}
    }
verbose(1, "Made %d substitutions\n", subCount);
// Close output
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
tagStormRenameVals(argv[1], argv[2], argv[3]);
return 0;
}
