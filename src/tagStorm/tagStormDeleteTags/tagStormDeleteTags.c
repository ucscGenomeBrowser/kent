/* tagStormDeleteTags - deletes selected tags from a tagStorm file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagSchema.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "\ntagStormDeleteTags - deletes selected tags from a tagStorm file\n\n"
  "usage:\n"
  "   tagStormDeleteTags in.tags delete.txt out.tags\n\n"
  "where in.tags is the input tagStorm file, out.tags is the output\n"
  "file and delete.txt is a single-column text file containing the\n"
  "names of the tags to be deleted.\n"
/*  "options:\n" */
/*  "   -xxx=XXX\n" */
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void tagStormDeleteTags(char *inName, char *deleteName, char *outName)
/* tagStormDeleteTags - deletes selected tags from a tagStorm file. */
{
// Open up inName as a lineFile
struct lineFile *lf = lineFileOpen(inName, TRUE);

// Open up deleteName as an slName list and read it into a hash
struct slName *deleteList = slNameLoadReal(deleteName);
struct hash *deleteHash = hashFromSlNameList(deleteList);

// Open up outName as a FILE
FILE *f = mustOpen(outName, "w");

// Loop through input lines.  Write them out unchanged unless first real word matches
// something in our hash.
char *line;
struct dyString *arrayDy = dyStringNew(0);
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
	char *arrayedTag = tagSchemaFigureArrayName(tag, arrayDy);
        char *tagExists = hashFindVal(deleteHash, arrayedTag);
        if (tagExists == NULL)
            {
            mustWrite(f, line, leadingSpaces);
            fprintf(f, "%s %s\n", tag, emptyForNull(s));
            }
        }
    }

// Close input
lineFileClose(&lf);

// Close output
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
tagStormDeleteTags(argv[1], argv[2], argv[3]);
return 0;
}
