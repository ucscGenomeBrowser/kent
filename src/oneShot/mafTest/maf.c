/* maf.c - Read/write maf format. */
#include "common.h"
#include "linefile.h"
#include "errabort.h"
#include "obscure.h"
#include "maf.h"

struct mafFile *mafMayOpen(char *fileName)
/* Open up a maf file and verify header. */
{
struct mafFile *mf;
struct lineFile *lf;
char *line, *word;
char *sig = "##maf";

if ((lf = lineFileMayOpen(fileName, TRUE)) == NULL)
    return NULL;
AllocVar(mf);
mf->lf = lf;

lineFileNeedNext(lf, &line, NULL);
if (!startsWith(sig, line))
    errAbort("%s does not start with %s", fileName, sig);
line += strlen(sig);

while ((word = nextWord(&line)) != NULL)
    {
    /* Parse name=val. */
    char *name = word;
    char *val = strchr(word, '=');
    if (val == NULL)
       errAbort("Missing = after %s line 1 of %s\n", name, fileName);
    *val++ = 0;

    if (sameString(name, "version"))
        mf->version = atoi(val);
    else if (sameString(name, "scoring"))
        mf->scoring = cloneString(val);
    }
if (mf->version == 0)
    errAbort("No version line 1 of %s\n", fileName);
return mf;
}

struct mafFile *mafOpen(char *fileName)
/* Open up a maf file.  Squawk and die if there's a problem. */
{
struct mafFile *mf = mafMayOpen(fileName);
if (mf == NULL)
    errnoAbort("Couldn't open %s\n", fileName);
return mf;
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

struct mafAli *mafNext(struct mafFile *mf)
/* Return next alignment in FILE or NULL if at end. */
{
struct lineFile *lf = mf->lf;
struct mafAli *ali;
char *line, *word;

/* Get alignment header line.  If it's not there assume end of file. */
if (!nextLine(lf, &line))
    {
    lineFileClose(&mf->lf);
    return NULL;
    }

/* Parse alignment header line. */
word = nextWord(&line);
if (word == NULL || !sameString(word, "a"))
    errAbort("Expecting 'a' at start of line %d of %s\n", lf->lineIx, lf->fileName);
AllocVar(ali);
while ((word = nextWord(&line)) != NULL)
    {
    /* Parse name=val. */
    char *name = word;
    char *val = strchr(word, '=');
    if (val == NULL)
       errAbort("Missing = after %s line 1 of %s", name, lf->fileName);
    *val++ = 0;

    if (sameString(name, "score"))
        ali->score = atof(val);
    }

/* Parse alignment components until blank line. */
for (;;)
    {
    if (!nextLine(lf, &line))
	errAbort("Unexpected end of file %s", lf->fileName);
    word = nextWord(&line);
    if (word == NULL)
        break;
    if (sameString(word, "s"))
        {
	struct mafComp *comp;
	int wordCount;
	char *row[7];

	/* Chop line up by white space.  This involves a few +-1's because
	 * have already chopped out first word. */
	row[0] = word;
	wordCount = chopByWhite(line, row+1, ArraySize(row)-1) + 1; /* +-1 because of "s" */
	lineFileExpectWords(lf, ArraySize(row), wordCount);
	AllocVar(comp);

	comp->text = cloneString(row[6]);
	comp->src = cloneString(row[1]);
	comp->srcSize = lineFileNeedNum(lf, row, 5);
	comp->strand = cloneString(row[4]);
	comp->start = lineFileNeedNum(lf, row, 2);
	comp->size = lineFileNeedNum(lf, row, 3);
	slAddHead(&ali->components, comp);
	}
    }
slReverse(&ali->components);
return ali;
}

struct mafFile *mafReadAll(char *fileName)
/* Read all elements in a maf file */
{
struct mafFile *mf = mafOpen(fileName);
struct mafAli *ali;
while ((ali = mafNext(mf)) != NULL)
    {
    slAddHead(&mf->mafAli, ali);
    }
slReverse(&mf->mafAli);
return mf;
}

void mafWriteStart(FILE *f, char *scoring)
/* Write maf header and scoring scheme name (may be null) */
{
fprintf(f, "##maf version=1");
if (scoring != NULL)
    fprintf(f, " scoring=%s", scoring);
fprintf(f, "\n");
}

void mafWrite(FILE *f, struct mafAli *ali)
/* Write next alignment to file. */
{
struct mafComp *comp;
int srcChars = 0, startChars = 0, sizeChars = 0, srcSizeChars = 0;

/* Write out alignment header */
fputc('a', f);
if (ali->score != 0.0)
    fprintf(f, " score=%f", ali->score);
fputc('\n', f);

/* Figure out length of each field. */
for (comp = ali->components; comp != NULL; comp = comp->next)
    {
    int len = strlen(comp->src);
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
for (comp = ali->components; comp != NULL; comp = comp->next)
    {
    fprintf(f, "s %-*s %*d %*d %s %*d %s\n", 
    	srcChars, comp->src, startChars, comp->start, 
	sizeChars, comp->size, comp->strand, 
	srcSizeChars, comp->srcSize, comp->text);
    }

/* Write out blank separator line. */
fprintf(f, "\n");
}

void mafWriteEnd(FILE *f)
/* Write maf footer. In this case nothing */
{
}

void mafWriteAll(struct mafFile *mf, char *fileName)
/* Write out full mafFile. */
{
FILE *f = mustOpen(fileName, "w");
struct mafAli *ali;
mafWriteStart(f, mf->scoring);
for (ali = mf->mafAli; ali != NULL; ali = ali->next)
    mafWrite(f, ali);
mafWriteEnd(f);
carefulClose(&f);
}

