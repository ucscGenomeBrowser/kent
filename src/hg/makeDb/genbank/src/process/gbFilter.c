#include "common.h"
#include "dnaseq.h"
#include "gbFilter.h"
#include "gbParse.h"
#include "linefile.h"
#include "errabort.h"
#include "keys.h"

static struct filter *makeFilter(char *description, int nLines, char **lines)
/* Create filter from filter specification file. */
{
struct filter *filt;
char *words[128];
int wordCount;
int iLine, i;

AllocVar(filt);
for (iLine = 0; iLine < nLines; iLine++)
    {
    char *line = lines[iLine];
    if (startsWith("//", line) )
        continue;
    if (startsWith("restrict", line))
        {
        char *s = skipToNextWord(line);
        filt->keyExp = keyExpParse(s);
        }
    else if (startsWith("hide", line))
        {
        wordCount = chopLine(line, words);
        for (i=1; i<wordCount; ++i)
            {
            struct slName *name = newSlName(words[i]);
            slAddHead(&filt->hideKeys, name);
            }
        }
    else if (startsWith("type", line))
        {
        char *type;
        wordCount = chopLine(line, words);
        if (wordCount < 2)
            errAbort("Expecting at least two words in type line of %s\n", description);
        type = words[1];
        if (sameWord(type, "BAC"))
            {
            filt->isBAC = TRUE;
            }
        else
            errAbort("Unrecognized type %s in %s\n", type, description);
        }
    else if (skipLeadingSpaces(line) != NULL)
        {
        errAbort("Can't understand line %d of %s:\n%s", iLine+1, description,  line);
        }
    }
return filt;
}

struct filter *makeFilterFromFile(char *fileName)
/* Create filter from filter specification file. */
{
struct slName* lineList = NULL, *lineRec;
char line[1024];
int nLines=0, i;
char **lines;
struct filter* filt;

// read file into an array of lines
FILE *f = mustOpen(fileName, "r");
while (fgets(line, sizeof(line), f))
    {
    lineRec = slNameNew(line);
    slAddHead(&lineList, lineRec);
    nLines++;
    }
carefulClose(&f);
slReverse(&lineList);
lines = alloca(nLines*sizeof(char*));
for (lineRec = lineList, i = 0; lineRec != NULL;
     lineRec = lineRec->next, i++)
    {
    lines[i] = lineRec->name;
    }
filt = makeFilter(fileName, nLines, lines);
slFreeList(&lineList);
return filt;
}

struct filter *makeFilterFromString(char *exprStr)
/* Create filter from a string with semi-colon seperated expressions. */
{
int nLines;
char **lines;

nLines = chopString(exprStr, ";\n", NULL, 0);
lines = alloca(nLines*sizeof(char*));
chopString(exprStr, ";\n", lines, nLines);

return makeFilter("string expression", nLines, lines);
}

struct filter *makeFilterEmpty()
/* Create an empty filter that selects everything */
{
return makeFilter("empty filter", 0, NULL);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
