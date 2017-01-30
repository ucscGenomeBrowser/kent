/* encode2GffDoctor - Fix up gff/gtf files from encode phase 2 a bit.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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

struct gffRow
/* A row of data from a GFF file. */
    {
    struct gffRow *next;
    char *row[9];
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

#ifdef UNUSED
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
#endif /* UNUSED */

boolean groupTagExists(struct gffRow *list, char *tag)
/* Return true if there are any match to tag in the list */
{
struct gffRow *el;
boolean exists = FALSE;
for (el = list; el != NULL; el = el->next)
    {
    /* Find group field and copy it to where we can modify it. */
    char *lastField = el->row[8];
    int lastFieldSize = strlen(lastField);
    char temp[lastFieldSize+1];
    safef(temp, sizeof(temp), "%s", lastField);
    char *group = temp;

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
	int endPos = prefixSize + destSize + remainderSize;
	assert(endPos < outSize);
	out[endPos] = 0;
	memcpy(out+prefixSize+destSize, remainder, remainderSize);
	return TRUE;
	}
    }
return FALSE;
}

int subbedTagCount;  // Records number of tag substitutions made.

boolean replaceGroupTag(struct gffRow *list, char *source, char *dest)
/* If source exists in *pLine, replace it with dest and return TRUE.  */
{
if (!groupTagExists(list, source))
    return FALSE;
int sourceSize = strlen(source);
int destSize = strlen(dest);
int sizeDiff = destSize - sourceSize;
struct gffRow *el;
for (el = list; el != NULL; el = el->next)
    {
    /* Find group field and copy it to where we can modify it. */
    char *lastField = el->row[8];
    int lastFieldSize = strlen(lastField);
    char temp[lastFieldSize+1];
    safef(temp, sizeof(temp), "%s", lastField);
    char *group = temp;

    /* Figure out how big the replaced version is going to be and make an array that size. */
    int groupSize = strlen(group);
    int outSize = groupSize + sizeDiff + 1;
    char subbedGroup[outSize];

    if (subWordIntoBuf(group, source, sourceSize, dest, destSize, subbedGroup, outSize))
        {
	el->row[8] = cloneString(subbedGroup);
	++subbedTagCount;
	verbose(2, "Substituted %s for %s\n", dest, source);
	}
    }
return TRUE;
}

struct gffRow *readGff(char *fileName)
/* Read GFF and return as rows. */
{
struct gffRow *el, *list = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
while (lineFileNextReal(lf, &line))
    {
    AllocVar(el);
    char *dupe = cloneString(line);
    int wordCount = chopTabs(dupe, el->row);
    lineFileExpectWords(lf, ArraySize(el->row), wordCount);
    slAddHead(&list, el);
    }
slReverse(&list);
return list;
}

boolean gotStringInField(struct gffRow *list, int rowIx, char *string)
/* Return TRUE if string matches any item in rowIx anywhere on list.  Match is case insensitive. */
{
struct gffRow *el;
for (el = list; el != NULL; el = el->next)
    if (sameWord(el->row[rowIx], string))
        return TRUE;
return FALSE;
}

int fieldSubCount = 0;

void subIntoField(struct gffRow *list, int rowIx, char *oldVal, char *newVal)
/* Substitute newVal for oldVal whereever oldVal occurs in given field */
{
struct gffRow *el;
for (el = list; el != NULL; el = el->next)
    if (sameWord(el->row[rowIx], oldVal))
	{
        el->row[rowIx] = newVal;
	++fieldSubCount;
	}
}

char *mostPopularValInField(struct gffRow *list, int rowIx)
/* Return most popular value in list for given field. */
{
/* Create a little hash and list to track words. */
struct word 
    {
    struct word *next;
    char *word;
    int count;
    };
struct word *wordList = NULL;
struct hash *hash = hashNew(0);

/* Loop through list counting occurences of words. */
struct gffRow *el;
for (el = list; el != NULL; el = el->next)
    {
    char *val = el->row[rowIx];
    struct word *word = hashFindVal(hash, val);
    if (word == NULL)
        {
	AllocVar(word);
	word->word = val;
	hashAdd(hash, val, word);
	slAddHead(&wordList, word);
	}
    word->count += 1;
    }

/* Figure out most common word. */
char *bestVal = NULL;
int bestCount = 0;
struct word *word;
for (word = wordList; word != NULL; word = word->next)
    {
    if (word->count > bestCount)
        {
	bestCount = word->count;
	bestVal = word->word;
	}
    }

/* Clean up and go home. */
hashFree(&hash);
slFreeList(&wordList);
return bestVal;
}

boolean tagIn(char *input, char *tag)
/* Return TRUE if tag is in input.  Input is formatted 
 *    tag "value string"; */
{
char *s = input;
for (;;)
    {
    s = skipLeadingSpaces(s);
    if (s == NULL || s[0] == 0)
        return FALSE;
    if (startsWithWord(tag, s))
        return TRUE;
    s = strchr(s, ';');
    if (s != NULL)
       ++s;
    }
}

char *addTag(char *oldString, char *tag, char *val)
/* Return oldString with 
 *     tag "val";
 * added to end of it. */
{
int len = strlen(oldString) + strlen(tag) + strlen(val) + 6;
char *newString = needMem(len);
safef(newString, len, "%s %s \"%s\";", oldString, tag, val);
return newString;
}

void encode2GffDoctor(char *inFile, char *outFile)
/* encode2GffDoctor - Fix up gff/gtf files from encode phase 2 a bit.. */
{
/* Get list of lines and see if we need to add transcript_id somehow. */
struct gffRow *el, *list=readGff(inFile);
char *transcriptTag = "transcript_id";
if (!groupTagExists(list, transcriptTag))
    {
    if (!replaceGroupTag(list, "transcript_ids", transcriptTag))
       if (!replaceGroupTag(list, "gene_ids", transcriptTag))
           if (!replaceGroupTag(list, "gene_id", transcriptTag))
	       errAbort("Can't find any %s or reasonable substitutes in %s", transcriptTag, inFile);
    }

int addGeneIdCount = 0;
for (el = list; el != NULL; el = el->next)
    {
    if (!tagIn(el->row[8], "gene_id"))
	{
        el->row[8] = addTag(el->row[8], "gene_id", "dummy");
	++addGeneIdCount;
	}
    }

/* See if it has any exons or CDS.  Otherwise turn most popular tag into exon. */
if (!gotStringInField(list, 2, "exon") && !gotStringInField(list, 2, "cds"))
    {
    char *common = mostPopularValInField(list, 2);
    verbose(1, "Doctor can't find 'exon', turning '%s' into 'exon'\n", common);
    subIntoField(list, 2, common, "exon");
    }

FILE *f = mustOpen(outFile, "w");
int shortenCount = 0;
int lineIx = 0;
for (el = list; el != NULL; el = el->next)
    {
    ++lineIx;
    char **row = el->row;

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
	    char *id = nextQuotedWord(&s);
	    int maxSize = 200;
	    if (strlen(id) > maxSize)
		{
		/* Looks like we have to abbreviate. Try to turn a comma near the end into ... */
		id[maxSize-4] = 0;   // 4 to leave some room for ...
		char *e = strrchr(id, ',');
		if (e != NULL)
		    {
		    *e++ = '.';
		    *e++ = '.';
		    *e++ = '.';
		    *e++ = '"';
		    *e = 0;
		    }

		/* Do replacement and log it. */
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
    verbose(1, "Doctor shortened %d tags, substituted %d, added gene_id %d, resourced %d exons in %s\n", 
	shortenCount, subbedTagCount, addGeneIdCount, fieldSubCount, inFile );
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
