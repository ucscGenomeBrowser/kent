/* customPp - custom track preprocessor.  This handles the line-oriented
 * input/output for the custom track system.  The main services it provides
 * are taking care of references to URLs, which are treated as includes,
 * and moving lines that start with "browser" to a list.
 *
 * Also this allows unlimited "pushBack" of lines, which is handy, since
 * the system will analyse a number of lines of a track to see what format
 * it's in. */

#include "common.h"
#include "linefile.h"
#include "net.h"
#include "customPp.h"

struct customPp *customPpNew(struct lineFile *lf)
/* Return customPp on lineFile */
{
struct customPp *cpp;

AllocVar(cpp);
cpp->fileStack = lf;
return cpp;
}

void customPpFree(struct customPp **pCpp)
/* Close files and free up customPp. */
{
struct customPp *cpp = *pCpp;
if (cpp)
    {
    lineFileCloseList(&cpp->fileStack);
    slFreeList(&cpp->browserLines);
    }
}

char *customPpNext(struct customPp *cpp)
/* Return next line. */
{
struct lineFile *lf;

/* Get next line from file on top of stack.  If at EOF
 * go to next file in stack.  If get a http:// or ftp:// line
 * open file this references and push it onto stack. Meanwhile
 * squirrel away 'browser' lines. */
while ((lf = cpp->fileStack) != NULL)
    {
    char *line;
    if (lineFileNext(lf, &line, NULL))
        {
	if (startsWith("http://", line) || startsWith("ftp://", line))
	    {
	    struct lineFile *lf = netLineFileOpen(line);
	    slAddHead(&cpp->fileStack, lf);
	    continue;
	    }
	else if (startsWith("browser", line))
	    {
	    char afterPattern = line[7];
	    if (isspace(afterPattern) || afterPattern == 0)
	        {
		slNameAddTail(&cpp->browserLines, line);
		continue;
		}
	    }
	return line;
	}
    else
        {
	cpp->fileStack = lf->next;
	lineFileClose(&lf);
	}
    }
return NULL;
}

char *customPpNextReal(struct customPp *cpp)
/* Return next line that's nonempty and non-space. */
{
for (;;)
    {
    char *line = customPpNext(cpp);
    if (line == NULL)
        return line;
    char *s = skipLeadingSpaces(line);
    char c = *s;
    if (c != 0 && c != '#')
        return line;
    }
}

void customPpReuse(struct customPp *cpp, char *line)
/* Reuse line.  May be called only once before next customPpNext/NextReal */
{
lineFileReuse(cpp->fileStack);
}

struct slName *customPpTakeBrowserLines(struct customPp *cpp)
/* Grab browser lines from cpp, which will no longer have them. */
{
struct slName *browserLines = cpp->browserLines;
cpp->browserLines = NULL;
return browserLines;
}
