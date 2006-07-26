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
    slFreeList(&cpp->toReuse);
    freeMem(cpp->doneReuse);
    slFreeList(&cpp->browserLines);
    }
}

char *customPpNext(struct customPp *cpp)
/* Return next line. */
{
struct lineFile *lf;

/* If there's a line to reuse, move that line off of toReuse and
 * onto doneReuse.  Free whatever used to be on doneReuse.  This
 * little bit of complexity makes it so that the reuse lines don't
 * indefinitely take up memory, but the line is still available in
 * unfreed memory on return from this function call until the next 
 * call. */
if (cpp->toReuse)
    {
    struct slName *n;
    freeMem(cpp->doneReuse);
    cpp->doneReuse = n = cpp->toReuse;
    cpp->toReuse = n->next;
    n->next = NULL;
    return n->name;
    }

/* Otherwise get next line from file on top of stack.  If at EOF
 * go to next file in stack.  If get a http:// or ftp:// line
 * open file this references and push it onto stack. Meanwhile
 * squirrel away 'browser' lines. */
while ((lf = cpp->fileStack) != NULL)
    {
    char *line;
    if (lineFileNextReal(lf, &line))
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

void customPpReuse(struct customPp *cpp, char *line)
/* Reuse line.  May be called repeatedly */
{
slNameAddHead(&cpp->toReuse, line);
}

struct slName *customPpTakeBrowserLines(struct customPp *cpp)
/* Grab browser lines from cpp, which will no longer have them. */
{
struct slName *browserLines = cpp->browserLines;
cpp->browserLines = NULL;
return browserLines;
}
