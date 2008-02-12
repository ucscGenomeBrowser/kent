/* anc.c - Read/write anc format. */
#include "common.h"
#include "linefile.h"
#include "errabort.h"
#include "obscure.h"
#include "anc.h"
#include "hash.h"
#include <fcntl.h>

static char const rcsid[] = "$Id: anc.c,v 1.1 2008/02/12 18:31:02 rico Exp $";

void ancCompFree(struct ancComp **pObj)
/* Free up an anchor component. */
{
struct ancComp *obj = *pObj;
if (obj == NULL)
	return;
freeMem(obj->src);
freez(pObj);
}

void ancCompFreeList(struct ancComp **pList)
/* Free up a list of anchor components. */
{
struct ancComp *el, *next;

for (el = *pList; el != NULL; el = next)
	{
	next = el->next;
	ancCompFree(&el);
	}
*pList = NULL;
}

void ancBlockFree(struct ancBlock **pObj)
/* Free up an anchor block. */
{
struct ancBlock *obj = *pObj;
if (obj == NULL)
	return;
ancCompFreeList(&obj->components);
freez(pObj);
}

void ancBlockFreeList(struct ancBlock **pList)
/* Free up a list of anchor blocks. */
{
struct ancBlock *el, *next;

for (el = *pList; el != NULL; el = next)
	{
	next = el->next;
	ancBlockFree(&el);
	}
*pList = NULL;
}

void ancFileFree(struct ancFile **pObj)
/* Free up an anchor file including closing file handle if necessary. */
{
struct ancFile *obj = *pObj;
if (obj == NULL)
	return;
lineFileClose(&obj->lf);
ancBlockFreeList(&obj->anchors);
freez(pObj);
}

void ancFileFreeList(struct ancFile **pList)
/* Free up a list of anchor files. */
{
struct ancFile *el, *next;

for (el = *pList; el != NULL; el = next)
	{
	next = el->next;
	ancFileFree(&el);
	}
*pList = NULL;
}

struct ancFile *ancMayOpen(char *fileName)
/* Open up an anchor file and verify header. */
{
struct ancFile *af;
struct lineFile *lf;
char *line, *word;
char *sig = "##anc";

if ((lf = lineFileMayOpen(fileName, TRUE)) == NULL)
	return NULL;
AllocVar(af);
af->lf = lf;

lineFileNeedNext(lf, &line, NULL);
if (!startsWith(sig, line))
	{
	errAbort("%s does not start with %s", fileName, sig);
	}
line += strlen(sig);

while ((word = nextWord(&line)) != NULL)
	{
	/* Parse name=val. */
	char *name = word;
	char *val = strchr(word, '=');
	if (val == NULL)
		errAbort("<issing = after %s line 1 of %s\n", name, fileName);
	*val++ = 0;

	if (sameString(name, "version"))
		af->version = atoi(val);
	else if (sameString(name, "minLen"))
		af->minLen = atoi(val);
	}

if (af->version == 0)
	errAbort("No version line 1 of %s\n", fileName);
return af;
}

struct ancFile *ancOpen(char *fileName)
/* Open up an anchor file. Squawk and die if there's a problem. */
{
struct ancFile *af = ancMayOpen(fileName);
if (af == NULL)
	errnoAbort("Couldn't open %s\n", fileName);
return af;
}

void ancRewind(struct ancFile *af)
/* Seek to beginning of open anchor file */
{
if (af == NULL)
	errAbort("anchor file rewind failed -- file not open");
lineFileSeek(af->lf, 0, SEEK_SET);
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

struct ancBlock *ancNextWithPos(struct ancFile *af, off_t *retOffset)
/* Return next anchor in FILE or NULL it at end. If refOffset is
 * nonNULL, return start offset of record in file. */
{
struct lineFile *lf = af->lf;
struct ancBlock *block;
char *line, *word;

/* Loop until get an anchor paragraph or reach end of file. */
for (;;)
	{
	/* Get anchor header line. If it's not there assume end of file. */
	if (!nextLine(lf, &line))
		{
		lineFileClose(&af->lf);
		return NULL;
		}

	/* Parse anchor header line. */
	word = nextWord(&line);
	if (word == NULL)
		continue;	/* Ignore blank lines. */

	if (sameString(word, "a"))
		{
		if (retOffset != NULL)
			*retOffset = lineFileTell(af->lf);
		AllocVar(block);
		while ((word = nextWord(&line)) != NULL)
			{
			/* Parse name=val. */
			char *name = word;
			char *val = strchr(word, '=');
			if (val == NULL)
				errAbort("Missing = after %s line 1 of %s", name, lf->fileName);
			*val++ = 0;

			if (sameString(name, "ancLen"))
				block->ancLen = atoi(val);
			}

		/* Parse anchor components until blank line. */
		for (;;)
			{
			if (!nextLine(lf, &line))
				errAbort("Unexpected end of file %s", lf->fileName);
			word = nextWord(&line);
			if (word == NULL)
				break;
			if (sameString(word, "s"))
				{
				struct ancComp *comp;
				int wordCount;
				char *row[5];

				/* Chop line up by whote space. This involves a few +-1's
				 * because have already chopped out first word. */
				row[0] = word;
				wordCount = chopByWhite(line, row+1, ArraySize(row)-1) + 1;	/* +-1 because of "s" */
				lineFileExpectWords(lf, ArraySize(row), wordCount);
				AllocVar(comp);

				/* Convert ascii test representation to ancComp structure. */
				comp->src = cloneString(row[1]);
				comp->start = lineFileNeedNum(lf, row, 2);
				comp->strand = row[3][0];
				comp->srcSize = lineFileNeedNum(lf, row, 4);

				/* Do some sanity checking. */
				if (comp->srcSize < 0)
					errAbort("Got a negative size line %d of %s", lf->lineIx, lf->fileName);
				if (comp->start < 0 || comp->start + block->ancLen > comp->srcSize)
					errAbort("Coordinates out of range line %d of %s", lf->lineIx, lf->fileName);

				/* Add component to head of list. */
				slAddHead(&block->components, comp);
				}
			}
		slReverse(&block->components);
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

struct ancBlock *ancNext(struct ancFile *af)
/* Return next anchor in FILE or NULL if at end. */
{
return ancNextWithPos(af, NULL);
}

struct ancFile *ancReadAll(char *fileName)
/* Read all anchors in an anchor file */
{
struct ancFile *af = ancOpen(fileName);
struct ancBlock *block;
while ((block = ancNext(af)) != NULL)
	{
	slAddHead(&af->anchors, block);
	}
slReverse(&af->anchors);
return af;
}

void ancWriteStart(FILE *f, int minLen)
/* Write anchor header and minLen. */
{
fprintf(f, "##anc version=1 minLen=%d\n", minLen);
}

void ancWrite(FILE *f, struct ancBlock *block)
/* Write next anchor to file. */
{
struct ancComp *comp;

/* Write anchor header */
fprintf(f, "a ancLen=%d\n", block->ancLen);

for (comp = block->components; comp != NULL; comp = comp->next)
	fprintf(f, "s %s %d %c %d\n", comp->src, comp->start,
		comp->strand, comp->srcSize);
fputc('\n', f);
}

void ancWriteEnd(FILE *f)
/* Write anchor footer. In this case nothing. */
{
}

