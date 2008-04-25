/* seg.c - read/write seg format. */
#include "common.h"
#include "linefile.h"
#include "errabort.h"
#include "obscure.h"
#include "seg.h"
#include "hash.h"
#include <fcntl.h>


static char const rcsid[] = "$Id: seg.c,v 1.7 2008/04/25 17:38:39 rico Exp $";


void segCompFree(struct segComp **pObj)
/* Free up a segment component. */
{
struct segComp *obj = *pObj;
if (obj == NULL)
	return;
freeMem(obj->src);
freez(pObj);
}


void segCompFreeList(struct segComp **pList)
/* Free up a list of segment components. */
{
struct segComp *el, *next;

for (el = *pList; el != NULL; el = next)
	{
	next = el->next;
	segCompFree(&el);
	}
*pList = NULL;
}


void segBlockFree(struct segBlock **pObj)
/* Free up a segment block. */
{
struct segBlock *obj = *pObj;
if (obj == NULL)
	return;
freeMem(obj->name);
segCompFreeList(&obj->components);
freez(pObj);
}


void segBlockFreeList(struct segBlock **pList)
/* Free up a list of segment blocks. */
{
struct segBlock *el, *next;

for (el = *pList; el != NULL; el = next)
	{
	next = el->next;
	segBlockFree(&el);
	}
*pList = NULL;
}


void segFileFree(struct segFile **pObj)
/* Free up a segment file including closing file handle if necessary. */
{
struct segFile *obj = *pObj;
if (obj == NULL)
	return;
segBlockFreeList(&obj->blocks);
lineFileClose(&obj->lf);
freez(pObj);
}


void segFileFreeList(struct segFile **pList)
/* Free up a list of segment files. */
{
struct segFile *el, *next;

for (el = *pList; el != NULL; el = next)
	{
	next = el->next;
	segFileFree(&el);
	}
*pList = NULL;
}


struct segFile *segMayOpen(char *fileName)
/* Open up a segment file for reading. Read header and verify. Prepare
 * for subsequent calls to segNext(). Return NULL if file does not exist. */
{
struct segFile *sf;
struct lineFile *lf;
char *line, *name, *val, *word;
char *sig = "##seg";

/* Open fileName. */
if ((lf = lineFileMayOpen(fileName, TRUE)) == NULL)
	return NULL;
AllocVar(sf);
sf->lf = lf;

/* Check for a valid signature. */
lineFileNeedNext(lf, &line, NULL);
if (!startsWith(sig, line))
	errAbort("%s does not start with %s", fileName, sig);
line += strlen(sig);

/* parse name=val. */
while ((word = nextWord(&line)) != NULL)
	{
	name = word;
	val = strchr(word, '=');
	if (val == NULL)
		errAbort("Missing = after %s line 1 of %s", name, fileName);
	*val++ = 0;

	if (sameString(name, "version"))
		sf->version = atoi(val);
	}

if (sf->version == 0)
	errAbort("No version line 1 of %s", fileName);

return sf;
}


struct segFile *segOpen(char *fileName)
/* Like segMayOpen() above, but prints an error message and aborts if
 * there is a problem. */
{
struct segFile *sf = segMayOpen(fileName);
if (sf == NULL)
	errnoAbort("Couldn't open %s", fileName);
return sf;
}


void segRewind(struct segFile *sf)
/* Seek to beginning of open segment file */
{
if (sf == NULL)
	errAbort("segment file rewind failed -- file not open");
lineFileSeek(sf->lf, 0, SEEK_SET);
}

static boolean nextLine(struct lineFile *lf, char **pLine)
/* Get next line that is not a comment. */
{
for (;;)
	{
	if (!lineFileNext(lf, pLine, NULL))
		return FALSE;
	if (**pLine != '#')
		return TRUE;
	}
}


struct segBlock *segNextWithPos(struct segFile *sf, off_t *retOffset)
/* Return next segment in segment file or NULL if at end. If retOffset
 * is not NULL, return start offset of record in file. */
{
struct lineFile *lf = sf->lf;
struct segBlock *block;
struct segComp *comp, *tail = NULL;
char *line, *name, *row[6], *val, *word;
int wordCount;

/* Loop until we get a segment paragraph or reach end of file. */
for (;;)
	{
	/* Get segment header line. If it's not there assume end of file. */
	if (!nextLine(lf, &line))
		{
		lineFileClose(&sf->lf);
		return NULL;
		}

	/* Parse segment header line. */
	word = nextWord(&line);
	if (word == NULL)
		continue;	/* Ignore blank lines. */

	if (sameString(word, "b"))
		{
		if (retOffset != NULL)
			*retOffset = lineFileTell(sf->lf);
		AllocVar(block);
		/* Parse name=val. */
		while ((word = nextWord(&line)) != NULL)
			{
			name = word;
			val = strchr(word, '=');
			if (val == NULL)
				errAbort("Missing = after %s line %d of %s",
					name, lf->lineIx, lf->fileName);
			*val++ = 0;

			if (sameString(name, "name"))
				block->name = cloneString(val);
			else if (sameString(name, "val"))
				block->val = atoi(val);
			}

		/* Parse segment components until blank line. */
		for (;;)
			{
			if (!nextLine(lf, &line))
				errAbort("Unexpected end of file %s", lf->fileName);
			word = nextWord(&line);
			if (word == NULL)
				break;
			if (sameString(word, "s"))
				{
				/* Chop line up by white space. This involves a few +=1's
				 * because we have already chopped out the first word. */
				row[0] = word;
				wordCount = chopByWhite(line, row+1, ArraySize(row)-1) +1;
				lineFileExpectWords(lf, ArraySize(row), wordCount);
				AllocVar(comp);

				/* Convert ASCII text representation to segComp structure. */
				comp->src     = cloneString(row[1]);
				comp->start   = lineFileNeedNum(lf, row, 2);
				comp->size    = lineFileNeedNum(lf, row, 3);
				comp->strand  = row[4][0];
				comp->srcSize = lineFileNeedNum(lf, row, 5);

				/* Do some sanity checking. */
				if (comp->size <= 0 || comp->srcSize <= 0)
					errAbort("Got a negative or zero size line %d of %s",
						lf->lineIx, lf->fileName);
				if (comp->start < 0 || comp->start + comp->size > comp->srcSize)
					errAbort("Coordinates out of range line %d of %s",
						lf->lineIx, lf->fileName);
				if (comp->strand != '+' && comp->strand != '-')
					errAbort("Invalid strand line %d of %s",
						lf->lineIx, lf->fileName);

				/* Add the new component to the current list. */
				if (block->components == NULL)
					block->components = comp;
				else
					tail->next = comp;
				tail = comp;
				}
			}
		return block;
		}
		else	/* Skip over paragraph we don't understand. */
		{
		for (;;)
			{
			if (!nextLine(lf, &line))
				return NULL;
			if (nextWord(&line) == NULL)
				break;
			}
		}
	}
}


struct segBlock *segNext(struct segFile *sf)
/* Return next segment in segment file or NULL if at end.  This will
 * close the open file handle at the end as well. */
{
return segNextWithPos(sf, NULL);
}


struct segFile *segReadAll(char *fileName)
/* Read in full segment file */
{
struct segFile *sf = segOpen(fileName);
struct segBlock *block, *tail = NULL;
while ((block = segNext(sf)) != NULL)
	{
	if (sf->blocks == NULL)
		sf->blocks = block;
	else
		{
		tail->next = block;
		block->prev = tail;
		}
	tail = block;
	}
return sf;
}


void segWriteStart(FILE *f)
/* Write segment file header to the file. */
{
fprintf(f, "##seg version=1\n");
}


void segWrite(FILE *f, struct segBlock *block)
/* Write next segment block to the file. */
{
struct segComp *comp;
int srcChars = 0, startChars = 0, sizeChars = 0, srcSizeChars = 0;

/* Write segment block header. */
fprintf(f, "b");
if (block->name != NULL)
	fprintf(f, " name=%s", block->name);
if (block->val != 0)
	fprintf(f, " val=%d", block->val);
fprintf(f, "\n");

/* Figure out length of each field. */
for (comp = block->components; comp != NULL; comp = comp->next)
	{
	int len = 0;
	/* A name line '.' will break some tools, so replace it with a
	 * generic name */
	if (sameString(comp->src, "."))
		{
		freeMem(&comp->src);
		comp->src = cloneString("defaultName");
		}
	len = strlen(comp->src);
	if (srcChars < len)
		srcChars = len;
	len = digitsBaseTen(comp->start);
	if (startChars < len)
		startChars = len;
	len = digitsBaseTen(comp->size);
	if (sizeChars < len)
		sizeChars = len;
	len = digitsBaseTen(comp->srcSize);
	if (srcSizeChars < len)
		srcSizeChars = len;
	}

/* Write out each component. */
for (comp = block->components; comp != NULL; comp = comp->next)
	{
	fprintf(f, "s %-*s %*d %*d %c %*d\n",
		srcChars, comp->src, startChars, comp->start, sizeChars, comp->size,
		comp->strand, srcSizeChars, comp->srcSize);
	}

/* Write out blank separator line. */
fprintf(f, "\n");
}


void segWriteEnd(FILE *f)
/* Write segment file end tag to the file.  In this case, nothing. */
{
}

struct segComp *segMayFindCompSpecies(struct segBlock *sb, char *src,
	char sepChar)
/* Find component with a source that matches src up to and possibly
   including sepChar. Return NULL if not found. */
{
struct segComp *sc;
char *p, *q;

for (sc = sb->components; sc != NULL; sc = sc->next)
	{
	for (p = sc->src, q = src;
		 *p != '\0' && *p != sepChar && *q != '\0' && *q != sepChar;
		 p++, q++)
		{
		if (*p != *q)
			break;
		}

	if ((*p == *q)
		|| (*p == sepChar && *q == '\0')
		|| (*p == '\0' && *q == sepChar))
		{
		return(sc);
		}
	}

return(NULL);
}


struct segComp *segFindCompSpecies(struct segBlock *sb, char *src,
	char sepChar)
/* Find component with a source that matches src up to and possibly
   including sepChar or die trying. */
{
struct segComp *sc = segMayFindCompSpecies(sb, src, sepChar);
char *p;

if (sc == NULL)
	{
	if ((p = strchr(src, sepChar)) != NULL)
		*p = '\0';

	errAbort("Couldn't find %s%c or just %s... in block.", src, sepChar, src);
	}

return(sc);
}

struct segComp *cloneSegComp(struct segComp *sc)
/* Return a copy of the argument segment component. */
{
struct segComp *newSc;

AllocVar(newSc);

newSc->next    = sc->next;
newSc->src     = cloneString(sc->src);
newSc->start   = sc->start;
newSc->size    = sc->size;
newSc->strand  = sc->strand;
newSc->srcSize = sc->srcSize;

return(newSc);
}

char *segFirstCompSpecies(struct segBlock *sb, char sepChar)
/* Return the species possibly followed by sepChar of the first component
   of the argument block. Return NULL if the block has no components. */
{
char *p, *species;

if (sb->components == NULL)
	return(NULL);

/* Temporarily split src into species and chrom. */
if ((p = strchr(sb->components->src, sepChar)) != NULL)
	*p = '\0';

species = cloneString(sb->components->src);

/* Restore src. */
if (p != NULL)
	*p = sepChar;

return(species);
}

struct slName *segSecSpeciesList(struct segBlock *sb, struct segComp *refComp,
    char sepChar)
/* Return a name list containing the species possibly followed by sepChar
of all components other than refComp on the block. */
{
struct segComp *sc;
char *p;
struct slName *speciesList = NULL;

for (sc = sb->components; sc != NULL; sc = sc->next)
	{
	if (sc == refComp)
		continue;

	if ((p = strchr(sc->src, '.')) != NULL)
		*p = '\0';

	slNameStore(&speciesList, sc->src);

	if (p != NULL)
		*p = '.';
	}

return(speciesList);
}
