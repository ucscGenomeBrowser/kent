/* customPp - custom track preprocessor.  This handles the line-oriented
 * input/output for the custom track system.  The main services it provides
 * are taking care of references to URLs, which are treated as includes,
 * and moving lines that start with "browser" to a list.
 *
 * Also this allows unlimited "pushBack" of lines, which is handy, since
 * the system will analyse a number of lines of a track to see what format
 * it's in. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "net.h"
#include "customPp.h"
#include "hgConfig.h"
#include "customTrack.h"
#ifdef PROGRESS_METER
#include "udc.h"
#endif


struct customPp *customDocPpNew(struct lineFile *lf)
/* Return customPp that will ignore browser lines, for doc files */
{
struct customPp *cpp = customPpNew(lf);
cpp->ignoreBrowserLines = TRUE;
return cpp;
}

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
    slFreeList(&cpp->reusedLines);
    freeMem(cpp->inReuse);
    slFreeList(&cpp->skippedLines);
    }
}

static char* bigUrlToTrackLine(char *url)
/* given the URL to a big file, create a custom track
 * line for it, has to be freed. Return NULL
 * if it's not a big file URL  */
{
char *type = customTrackTypeFromBigFile(url);
if (type==NULL)
    return url;

// grab the last element in the path
char *baseName = cloneString(url);
char *ptr;
if ((ptr = strrchr(baseName, '/')) != NULL)
    baseName = ptr + 1;

// For vcfTabix files that end in ".vcf.gz", only the ".gz" is stripped off by splitPath;
// strip off the remaining ".vcf":
if (endsWith(baseName, ".vcf"))
    baseName[strlen(baseName)-4] = '\0';

// cap track name at 100 chars for sanity's sake
if (strlen(baseName) > 100)
    baseName[100] = 0;

char *trackName = baseName;

char buf[4000];
safef(buf, sizeof(buf), "track name=%s bigDataUrl=%s type=%s\n", trackName, url, type);

freeMem(type);
return cloneString(buf);
}

char *customPpNext(struct customPp *cpp)
/* Return next line. */
{
/* Check first for line to reuse. */
struct slName *reused = cpp->reusedLines;
if (reused)
    {
    /* We need to keep line actually reusing in memory until next
     * call to customPpNext, so we move it to inReuse rather than
     * immediately freeing it. */
    freeMem(cpp->inReuse);   
    cpp->reusedLines = reused->next;
    cpp->inReuse = reused;
    return reused->name;
    }

/* Get next line from file on top of stack.  If at EOF
 * go to next file in stack.  If get a http:// or https:// or ftp:// line
 * open file this references and push it onto stack. Meanwhile
 * squirrel away 'browser' lines. */
struct lineFile *lf;
while ((lf = cpp->fileStack) != NULL)
    {
    char *line;
    if (lineFileNext(lf, &line, NULL))
        {
        // if user pastes just a URL, create a track line automatically
        // also allow a filename from a local directory if it has been allowed via udc.localDir in hg.conf
        bool isLocalFile = cfgOption("udc.localDir")!=NULL && startsWith(cfgOption("udc.localDir"), line);
	if (startsWith("http://", line) || startsWith("https://", line) || startsWith("ftp://", line) ||
            isLocalFile)
	    {
            if (customTrackIsBigData(line))
                line = bigUrlToTrackLine(line);
            else
                {
                lf = netLineFileOpen(line);
                slAddHead(&cpp->fileStack, lf);
    #ifdef PROGRESS_METER
                off_t remoteSize = 0;
                remoteSize = remoteFileSize(line);
                cpp->remoteFileSize = remoteSize;
    #endif
                continue;
                }
	    }
	else if (!cpp->ignoreBrowserLines && startsWith("browser", line))
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
/* Return next line that's nonempty, non-space and not a comment.
 * Save skipped comment lines to cpp->skippedLines. */
{
slFreeList(&cpp->skippedLines);
for (;;)
    {
    char *line = customPpNext(cpp);
    if (line == NULL)
        return line;
    char *s = skipLeadingSpaces(line);
    char c = *s;
    if (c != 0 && c != '#')
        return line;
    else if (c == '#')
	slNameAddHead(&cpp->skippedLines, line);
    }
}

void customPpReuse(struct customPp *cpp, char *line)
/* Reuse line.  May be called many times before next customPpNext/NextReal.
 * Should be called with last line to be reused first if called multiply. */
{
struct slName *s = slNameNew(line);
slAddHead(&cpp->reusedLines, s);
}

struct slName *customPpTakeBrowserLines(struct customPp *cpp)
/* Grab browser lines from cpp, which will no longer have them. */
{
struct slName *browserLines = cpp->browserLines;
cpp->browserLines = NULL;
return browserLines;
}

struct slName *customPpCloneSkippedLines(struct customPp *cpp)
/* Return a clone of most recent nonempty skipped (comment/header) lines from cpp,
 * which will still have them.  slFreeList when done. */
{
struct slName *skippedLines = NULL, *sl;
for (sl = cpp->skippedLines;  sl != NULL;  sl = sl->next)
    slNameAddHead(&skippedLines, sl->name);
return skippedLines;
}

char *customPpFileName(struct customPp *cpp)
/* Return the name of the current file being parsed (top of fileStack), or NULL
 * if fileStack is NULL.  Free when done. */
{
if (cpp->fileStack == NULL)
    return NULL;
return cloneString(cpp->fileStack->fileName);
}
