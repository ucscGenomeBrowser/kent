/* encode2GffDoctor - Fix up gff/gtf files from encode phase 2 a bit.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2GffDoctor - Fix up gff/gtf files from encode phase 2 a bit.\n"
  "usage:\n"
  "   encode2GffDoctor in.gff out.gff\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char *replaceFieldInGffGroup(char *in, char *tag, char *newVal)
/* Update value of tag */
{
char *tagStart = stringIn(tag, in);
if (tagStart == NULL)
    internalErr();
char *valEnd = strchr(tagStart, ';');
if (valEnd == NULL)
    internalErr();
struct dyString *dy = dyStringNew(strlen(in));
dyStringAppendN(dy, in, tagStart - in);
dyStringAppend(dy, tag);
dyStringAppendC(dy, ' ');
dyStringPrintf(dy, "%s", newVal);
dyStringAppend(dy, valEnd);
return dyStringCannibalize(&dy);
}

char *subForSmaller(char *string, char *oldWay, char *newWay)
/* Return copy of string with first if any oldWay substituted with newWay. */
{
int stringSize = strlen(string);
int oldSize = strlen(oldWay);
int newSize = strlen(newWay);
assert(oldSize >= newSize);
char *s = stringIn(oldWay, string);
int remainingSize = strlen(s);
if (s != NULL)
    {
    memcpy(s, newWay, newSize);
    if (oldSize != newSize)
	memcpy(s+newSize, s+oldSize, remainingSize - oldSize + 1);
    }
assert(strlen(string) == stringSize + newSize - oldSize);
return string;
}

boolean isSharpCommentOrEmpty(char *line)
/* Return TRUE if line's first non-white-space char is a #  or if line is all white space*/
{
char *s = skipLeadingSpaces(line);
char c = s[0];
return (c == 0 || c == '#');
}

char *findNthChar(char *line, char c, int n)
/* Find the nth occurence of c in line and return pointer to next char.  
 * Return NULL if not found. */
{
int count = 0;
char x;
while ((x = *line++) != 0)
    {
    if (x == c)
        {
	if (++count >= n)
	    return line;
	}
    }
return NULL;
}

char *mustFindNthChar(char *line, char c, int n)
/* Find the nth occurence of char in line or die. */
{
char *result = findNthChar(line, c, n);
if (result == NULL)
    errAbort("Could not find the expected %d '%c' chars in %s", n, c, line);
return result;
}

boolean groupTagExists(struct slName *lineList, char *tag)
/* Return true if there are any match to tag in the list */
{
struct slName *lineEl;
struct dyString *temp = dyStringNew(1024);
boolean exists = FALSE;
for (lineEl = lineList; lineEl != NULL; lineEl = lineEl->next)
    {
    char *line = lineEl->name;
    if (isSharpCommentOrEmpty(line))
        continue;

    /* Find group field in tab-separated line and copy it to where we can modify it. */
    char *lastField = mustFindNthChar(line, '\t', 8);
    dyStringClear(temp);
    dyStringAppend(temp, lastField);
    char *group = temp->string;

    /* Go through each word looking for a match. */
    char *word;
    while ((word = nextWord(&group)) != NULL)
        {
	if (sameString(word, tag))
	    {
	    exists = TRUE;
	    break;
	    }
	}
    if (exists)
        break;
    }
dyStringFree(&temp);
return exists;
}

boolean subWordIntoBuf(char *in,  char *source, int sourceSize, char *dest, int destSize, 
    char *out, int outSize)
/* Scan through s looking for source word and replacing it with dest word in output buf. */
{
char *s = in;
while ((s = stringIn(source, s)) != NULL)
    {
    char c = s[sourceSize];
    if (!isalnum(c) && c != '_')
        {
	int prefixSize = s - in;
	memcpy(out, in, prefixSize);
	memcpy(out+prefixSize, dest, destSize);
	char *remainder = in + prefixSize + sourceSize;
	int remainderSize = strlen(remainder);
	assert(prefixSize+destSize+remainderSize< outSize);
	memcpy(out+prefixSize+destSize, remainder, remainderSize);
	return TRUE;
	}
    }
return FALSE;
}

int subbedTagCount;  // Records number of tag substitutions made.

boolean replaceGroupTag(struct slName **pList, char *source, char *dest)
/* If source exists in *pLine, replace it with dest and return TRUE. 
 * Replaces *pLine possibly */
{
if (!groupTagExists(*pList, source))
    return FALSE;
struct slName *lineEl, *next, *newList = NULL;
int sourceSize = strlen(source);
int destSize = strlen(dest);
int sizeDiff = destSize - sourceSize;
for (lineEl = *pList; lineEl != NULL; lineEl = next)
    {
    next = lineEl->next;
    char *line = lineEl->name;
    if (isSharpCommentOrEmpty(line))
        continue;

    int lineSize = strlen(line);

    /* Find group field in tab-separated line and copy it to where we can modify it. */
    char *group = mustFindNthChar(line, '\t', 8);
    int groupSize = strlen(group);
    int outSize = groupSize + sizeDiff + 1;
    char subbedGroup[outSize];
    if (subWordIntoBuf(group, source, sourceSize, dest, destSize, subbedGroup, outSize))
        {
#ifdef SOON
#endif /* SOON */
	int newLineSize = lineSize + sizeDiff;
	struct slName *newEl = needMem(sizeof(struct slName) + newLineSize);
	char *subbedLine = newEl->name;
	int beforeGroupSize = group - line;
	memcpy(subbedLine, line, beforeGroupSize);
	strcpy(subbedLine+beforeGroupSize, subbedGroup);
	slAddHead(&newList, newEl);
	++subbedTagCount;
	verbose(2, "Substituted %s for %s\n", dest, source);
	}
    else
        {
	slAddHead(&newList, lineEl);
	}
    }
slReverse(&newList);
*pList = newList;
return TRUE;
}

void encode2GffDoctor(char *inFile, char *outFile)
/* encode2GffDoctor - Fix up gff/gtf files from encode phase 2 a bit.. */
{
/* Get list of lines and see if we need to add transcript_id somehow. */
struct slName *lineEl, *lineList=readAllLines(inFile);
char *transcriptTag = "transcript_id";
if (!groupTagExists(lineList, transcriptTag))
    {
    if (!replaceGroupTag(&lineList, "gene_id", transcriptTag))
       if (!replaceGroupTag(&lineList, "transcript_ids", transcriptTag))
           if (!replaceGroupTag(&lineList, "gene_ids", transcriptTag))
	       errAbort("Can't find any %s or reasonable substitutes in %s", transcriptTag, inFile);
    }


char *row[9];
FILE *f = mustOpen(outFile, "w");
int shortenCount = 0;
int lineIx = 0;
for (lineEl = lineList; lineEl != NULL; lineEl = lineEl->next)
    {
    ++lineIx;
    char *line = lineEl->name;
    if (isSharpCommentOrEmpty(line))
        continue;

    int wordCount = chopTabs(line, row);
    if (wordCount != 9)
        errAbort("Expecting 9 fields line %d of %s got %d", lineIx, inFile, wordCount);

    /* Fix mitochondria sequence name. */
    if (sameString(row[0], "chrMT"))
        row[0] = "chrM";

    /* Abbreviate really long transcript IDs and gene IDs. */
    char *tagsToShorten[] = {"transcript_id "};
    int i;
    for (i=0; i<ArraySize(tagsToShorten); ++i)
	{
	char *tag = tagsToShorten[i];
	char *val = stringBetween(tag, ";", row[8]);
	if (val != NULL)
	    {
	    char *s = val;
	    char *id = nextWordRespectingQuotes(&s);
	    int maxSize = 200;
	    if (strlen(id) > maxSize)
		{
		id[maxSize-4] = 0;
		char *e = strrchr(id, ',');
		if (e != NULL)
		    {
		    *e++ = '.';
		    *e++ = '.';
		    *e++ = '.';
		    *e++ = '"';
		    *e = 0;
		    }
		row[8] = replaceFieldInGffGroup(row[8], tag, id);
		++shortenCount;
		verbose(2, "Shortened to %s\n", id);
		}
	    freez(&val);
	    }
	}

    /* Output fixed result. */
    fprintf(f, "%s", row[0]);
    for (i=1; i<9; ++i)
        fprintf(f, "\t%s", row[i]);
    fprintf(f, "\n");
    }
if (shortenCount != 0 || subbedTagCount != 0)
    verbose(1, "Doctor shortened %d tags, substituted %d in %s\n", 
	shortenCount, subbedTagCount, inFile );
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
encode2GffDoctor(argv[1], argv[2]);
return 0;
}
