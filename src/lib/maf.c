/* maf.c - Read/write maf format. */
#include "common.h"
#include "linefile.h"
#include "errabort.h"
#include "obscure.h"
#include "dnautil.h"
#include "axt.h"
#include "maf.h"
#include "hash.h"
#include <fcntl.h>


char *mafRegDefTxUpstream = "txupstream";  // transcription start size upstream region

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

void mafRewind(struct mafFile *mf)
/* Seek to beginning of open maf file */
{
if (mf == NULL)
    errAbort("maf file rewind failed -- file not open");
lineFileSeek(mf->lf, 0, SEEK_SET);
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

static void mafRegDefParse(struct mafFile *mf, struct mafAli *ali, char *line)
/* parse a 'r' line of an 'a' paragraph. */
{
if (ali->regDef != NULL)
    errAbort("multiple 'r' lines in an alignment paragraph: %d of %s", mf->lf->lineIx, mf->lf->fileName);
char *row[3];
int wordCount = chopByWhite(line, row, ArraySize(row));
if (wordCount != 3)
    lineFileExpectWords(mf->lf, 3+1, wordCount+1); // +1 for 'r'
ali->regDef = mafRegDefNew(row[0], lineFileNeedFullNum(mf->lf, row, 1),
                           row[2]);
}

struct mafAli *mafNextWithPos(struct mafFile *mf, off_t *retOffset)
/* Return next alignment in FILE or NULL if at end.  If retOffset is
 * nonNULL, return start offset of record in file. */
{
struct lineFile *lf = mf->lf;
struct mafAli *ali;
char *line, *word;

/* Loop until get an alignment paragraph or reach end of file. */
for (;;)
    {
    /* Get alignment header line.  If it's not there assume end of file. */
    if (!nextLine(lf, &line))
	{
	lineFileClose(&mf->lf);
	return NULL;
	}

    /* Parse alignment header line. */
    word = nextWord(&line);
    if (word == NULL)
	continue;	/* Ignore blank lines. */
	
    if (sameString(word, "a"))
	{
	if (retOffset != NULL)
	    *retOffset = lineFileTell(mf->lf);
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
		break;
	    word = nextWord(&line);
	    if (word == NULL)
		break;
	    if (sameString(word, "s") || sameString(word, "e"))
		{
		struct mafComp *comp;
		int wordCount;
		char *row[7];
		int textSize;

		/* Chop line up by white space.  This involves a few +-1's because
		 * have already chopped out first word. */
		row[0] = word;
		wordCount = chopByWhite(line, row+1, ArraySize(row)-1) + 1; /* +-1 because of "s" */
		lineFileExpectWords(lf, ArraySize(row), wordCount);
		AllocVar(comp);

		/* Convert ascii text representation to mafComp structure. */
		comp->src = cloneString(row[1]);
		comp->srcSize = lineFileNeedNum(lf, row, 5);
		comp->strand = row[4][0];
		comp->start = lineFileNeedNum(lf, row, 2);

		if (sameString(word, "e"))
		    {
		    comp->size = 0;
		    comp->rightLen = comp->leftLen = lineFileNeedNum(lf, row, 3);
		    comp->rightStatus = comp->leftStatus = *row[6];
		    }
		else
		    {
		    comp->size = lineFileNeedNum(lf, row, 3);
		    comp->text = cloneString(row[6]);
		    textSize = strlen(comp->text);

		    /* Fill in ali->text size. */
		    if (ali->textSize == 0)
			ali->textSize = textSize;
		    else if (ali->textSize != textSize)
			errAbort("Text size inconsistent (%d vs %d) line %d of %s",
			    textSize, ali->textSize, lf->lineIx, lf->fileName);
		    }

		/* Do some sanity checking. */
		if (comp->srcSize < 0 || comp->size < 0)
		     errAbort("Got a negative size line %d of %s", lf->lineIx, lf->fileName);
		if (comp->start < 0 || comp->start + comp->size > comp->srcSize)
		     errAbort("Coordinates out of range line %d of %s", lf->lineIx, lf->fileName);
		  
		/* Add component to head of list. */
		slAddHead(&ali->components, comp);
		}
	    if (sameString(word, "i"))
		{
		struct mafComp *comp;
		int wordCount;
		char *row[6];

		/* Chop line up by white space.  This involves a few +-1's because
		 * have already chopped out first word. */
		row[0] = word;
		wordCount = chopByWhite(line, row+1, ArraySize(row)-1) + 1; /* +-1 because of "s" */
		lineFileExpectWords(lf, ArraySize(row), wordCount);
		if (!sameString(row[1],ali->components->src))
		    errAbort("i line src mismatch: i is %s :: s is %s\n", row[1], ali->components->src);

		comp = ali->components;
		comp->leftStatus = *row[2];
		comp->leftLen = atoi(row[3]);
		comp->rightStatus = *row[4];
		comp->rightLen = atoi(row[5]);
		}
            if (sameString(word, "q"))
		{
		struct mafComp *comp;
		int wordCount;
		char *row[3];

		/* Chop line up by white space.  This involves a few +-1's because
		 * have already chopped out first word. */
		row[0] = word;
		wordCount = chopByWhite(line, row+1, ArraySize(row)-1) + 1; /* +-1 because of "s" */
		lineFileExpectWords(lf, ArraySize(row), wordCount);
		if (!sameString(row[1],ali->components->src))
		    errAbort("q line src mismatch: q is %s :: s is %s\n", row[1], ali->components->src);

			comp = ali->components;
			comp->quality = cloneString(row[2]);
		}
	    if (sameString(word, "r"))
                mafRegDefParse(mf, ali, line);
	    }
	slReverse(&ali->components);
	return ali;
	}
    else  /* Skip over paragraph we don't understand. */
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


struct mafAli *mafNext(struct mafFile *mf)
/* Return next alignment in FILE or NULL if at end. */
{
return mafNextWithPos(mf, NULL);
}


struct mafFile *mafReadAll(char *fileName)
/* Read all elements in a maf file */
{
struct mafFile *mf = mafOpen(fileName);
struct mafAli *ali;
while ((ali = mafNext(mf)) != NULL)
    {
    slAddHead(&mf->alignments, ali);
    }
slReverse(&mf->alignments);
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
fprintf(f, "a score=%f\n", ali->score);

/* include region definition */
if (ali->regDef != NULL)
    fprintf(f, "r %s %d %s\n", ali->regDef->type, ali->regDef->size, ali->regDef->id);

/* Figure out length of each field. */
for (comp = ali->components; comp != NULL; comp = comp->next)
    {
    int len = 0;
    /* a name like '.' will break some tools, so replace it
    * with a generic name */
    if (sameString(comp->src,"."))
	comp->src=cloneString("defaultName");
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
for (comp = ali->components; comp != NULL; comp = comp->next)
    {
    if ((comp->size == 0) && (comp->leftStatus))
	fprintf(f, "e %-*s %*d %*d %c %*d %c\n", 
	    srcChars, comp->src, startChars, comp->start, 
	    sizeChars, comp->leftLen, comp->strand, 
	    srcSizeChars, comp->srcSize, comp->leftStatus);
    else
	{
	fprintf(f, "s %-*s %*d %*d %c %*d %s\n", 
	    srcChars, comp->src, startChars, comp->start, 
	    sizeChars, comp->size, comp->strand, 
	    srcSizeChars, comp->srcSize, comp->text);

	if (comp->quality)
		fprintf(f, "q %-*s %s\n",
		srcChars + startChars + sizeChars + srcSizeChars + 5,
		comp->src, comp->quality);

	if (comp->leftStatus)
	    fprintf(f,"i %-*s %c %d %c %d\n",srcChars,comp->src,
		comp->leftStatus,comp->leftLen,comp->rightStatus,comp->rightLen);
	}

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
for (ali = mf->alignments; ali != NULL; ali = ali->next)
    mafWrite(f, ali);
mafWriteEnd(f);
carefulClose(&f);
}

void mafCompFree(struct mafComp **pObj)
/* Free up a maf component. */
{
struct mafComp *obj = *pObj;
if (obj == NULL)
    return;
freeMem(obj->src);
freeMem(obj->text);
freeMem(obj->quality);
freez(pObj);
}

void mafCompFreeList(struct mafComp **pList)
/* Free up a list of maf components. */
{
struct mafComp *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    mafCompFree(&el);
    }
*pList = NULL;
}

char *mafCompGetSrcDb(struct mafComp *mc, char *buf, int bufSize)
/* parse the srcDb name from the mafComp src name, return NULL if no srcDb */
{
char *e = strchr(mc->src, '.');
if (e == NULL)
    return NULL;
int len = e - mc->src;
if (len >= bufSize-1)
    errAbort("srcDb name in \"%s\" overflows buffer length of %d", mc->src, bufSize);
strncpy(buf, mc->src, len);
buf[len] = '\0';
return buf;
}

char *mafCompGetSrcName(struct mafComp *mc)
/* parse the src sequence name from the mafComp src name */
{
char *e = strchr(mc->src, '.');
if (e == NULL)
    return mc->src;
else
    return e+1;
}

int mafPlusStart(struct mafComp *comp)
/* Return start relative to plus strand of src. */
{
if (comp->strand == '-') 
    return comp->srcSize - (comp->start + comp->size);
else
    return comp->start;
}

void mafAliFree(struct mafAli **pObj)
/* Free up a maf alignment. */
{
struct mafAli *obj = *pObj;
if (obj == NULL)
    return;
mafCompFreeList(&obj->components);
mafRegDefFree(&obj->regDef);
freez(pObj);
}

void mafAliFreeList(struct mafAli **pList)
/* Free up a list of maf alignmentx. */
{
struct mafAli *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    mafAliFree(&el);
    }
*pList = NULL;
}

void mafFileFree(struct mafFile **pObj)
/* Free up a maf file. */
{
struct mafFile *obj = *pObj;
if (obj == NULL)
    return;
lineFileClose(&obj->lf);
freeMem(obj->scoring);
mafAliFreeList(&obj->alignments);
freez(pObj);
}

void mafFileFreeList(struct mafFile **pList)
/* Free up a list of maf files. */
{
struct mafFile *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    mafFileFree(&el);
    }
*pList = NULL;
}

struct mafComp *mafMayFindComponent(struct mafAli *maf, char *src)
/* Find component of given source. Return NULL if not found. */
{
struct mafComp *mc;
for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    if (sameString(mc->src, src))
        return mc;
    }
return NULL;
}

struct mafComp *mafMayFindComponentDb(struct mafAli *maf, char *db)
/* Find component of given database, allowing component to be 
 * labeled "db", or "db.chrom" . Return NULL if not found. */
{
struct mafComp *mc;
char *p, *q;
for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    for (p = mc->src, q = db; *p && *q; p++, q++)
        {
        if (*p != *q)
            break;
        }
    if (*p == '.' && *q == 0)
        return mc;
    if (*p == *q)
        return mc;
    }
return NULL;
}

struct mafComp *mafFindComponent(struct mafAli *maf, char *src)
/* Find component of given source or die trying. */
{
struct mafComp *mc = mafMayFindComponent(maf, src);
if (mc == NULL)
    errAbort("Couldn't find %s in maf", src);
return mc;
}

struct mafComp *mafMayFindCompSpecies(struct mafAli *maf, char *species, char sepChar)
/* Find component of given source that starts with species possibly followed by sepChar or \0 .
   Return NULL if not found. */
{
struct mafComp *mc;
int speciesLen = strlen(species);

for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    if (startsWith(species, mc->src) )
	{
	char endChar = mc->src[speciesLen];

	if ((endChar == '\0') || (endChar == sepChar))
	    return mc;
	}
    }
return NULL;
}


struct mafComp *mafFindCompSpecies(struct mafAli *maf, char *species, char sepChar)
/* Find component of given source that starts with species followed by sepChar
   or die trying. */
{
struct mafComp *mc = mafMayFindCompSpecies(maf, species, sepChar);
if (mc == NULL)
    errAbort("Couldn't find %s%c or just %s... in maf", species,sepChar,species);
return mc;
}

struct mafComp *mafMayFindCompPrefix(struct mafAli *maf, char *pre, char *sep)
/* Find component of given source that starts with pre followed by sep.
   Return NULL if not found. */
{
struct mafComp *mc;
char prefix[256];

if (sep == NULL)
    sep = "";
snprintf(prefix, 256, "%s%s", pre, sep);

for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    if (startsWith(prefix, mc->src))
        return mc;
    }
return NULL;
}

struct mafComp *mafFindCompPrefix(struct mafAli *maf, char *pre, char *sep)
/* Find component of given source that starts with pre followed by sep
   or die trying. */
{
struct mafComp *mc = mafMayFindCompPrefix(maf, pre, sep);
if (mc == NULL)
    errAbort("Couldn't find %s%s... in maf", pre,sep);
return mc;
}

struct mafComp *mafMayFindComponentInHash(struct mafAli *maf, struct hash *cHash) 
/* Find arbitrary component of given source that matches any string in the cHash.
   Return NULL if not found. */
{
struct mafComp *mc;

for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    if (hashFindVal(cHash, mc->src))
        return mc;
    }
return NULL;
}

struct mafComp *mafMayFindSpeciesInHash(struct mafAli *maf, struct hash *cHash, char sepChar) 
/* Find arbitrary component of given who's source prefix (ended by sep)
   matches matches any string in the cHash.  Return NULL if not found. */
{
struct mafComp *mc;

for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    char *sep = strchr(mc->src, sepChar);
    if (sep != NULL)
        *sep = '\0';
    boolean hit = hashFindVal(cHash, mc->src) != NULL;
    if (sep != NULL)
        *sep = sepChar;
    if (hit)
        return mc;
    }
return NULL;
}

boolean mafMayFindAllComponents(struct mafAli *maf, struct hash *cHash) 
/* Find component of given source that starts matches any string in the cHash.
   Return NULL if not found. */
{
struct hashCookie cookie = hashFirst(cHash);
struct hashEl *el;

while ((el = hashNext(&cookie)) != NULL)
    if (mafMayFindComponent(maf, el->name) == NULL)
	return FALSE;
return TRUE;
}

struct mafAli *mafSubset(struct mafAli *maf, char *componentSource,
	int newStart, int newEnd)
{
return mafSubsetE(maf, componentSource, newStart, newEnd, FALSE);
}

struct mafAli *mafSubsetE(struct mafAli *maf, char *componentSource,
	int newStart, int newEnd, bool getInitialDashes)
/* Extract subset of maf that intersects a given range
 * in a component sequence.  The newStart and newEnd
 * are given in the forward strand coordinates of the
 * component sequence.  The componentSource is typically
 * something like 'mm3.chr1'.  This will return NULL
 * if maf does not intersect range.  The score field
 * in the returned maf will not be filled in (since
 * we don't know which scoring scheme to use). */
{
struct mafComp *mcMaster = mafFindComponent(maf, componentSource);
struct mafAli *subset;
struct mafComp *mc, *subMc;
char *s, *e;
int textStart, textSize;

/* Reverse complement input range if necessary. */
if (mcMaster->strand == '-')
    reverseIntRange(&newStart, &newEnd, mcMaster->srcSize);

/* Check if any real intersection and return NULL if not. */
if (newStart >= newEnd)
    return NULL;
if (newStart >= mcMaster->start + mcMaster->size)
    return NULL;
if (newEnd <= mcMaster->start)
    return NULL;

/* Clip to bounds of actual data. */
if (newStart < mcMaster->start)
    newStart = mcMaster->start;
if (newEnd > mcMaster->start + mcMaster->size)
    newEnd = mcMaster->start + mcMaster->size;

/* Translate position in master sequence to position in
 * multiple alignment. */
s = skipIgnoringDash(mcMaster->text, newStart - mcMaster->start, TRUE);
e = skipIgnoringDash(s, newEnd - newStart, TRUE);
textStart = s - mcMaster->text;
textSize = e - s;

if (getInitialDashes && (newStart == mcMaster->start))
    {
    textStart = 0;
    textSize += s - mcMaster->text;
    }

/* Allocate subset structure and fill it in */
AllocVar(subset);
subset->textSize = textSize;
for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    AllocVar(subMc);
    subMc->src = cloneString(mc->src);
    subMc->srcSize = mc->srcSize;
    subMc->strand = mc->strand;
    if (mc->size != 0)
        {
        subMc->start = mc->start + countNonDash(mc->text, textStart);
        subMc->size = countNonDash(mc->text+textStart, textSize);
        subMc->text = cloneStringZ(mc->text + textStart, textSize);
        if (mc->quality != NULL)
            subMc->quality = cloneStringZ(mc->quality + textStart, textSize);
        }
    else
	{
        /* empty row annotation */
        subMc->size = 0;
        subMc->start = mc->start;
	}

    subMc->leftStatus = mc->leftStatus;
    subMc->leftLen = mc->leftLen;
    subMc->rightStatus = mc->rightStatus;
    subMc->rightLen = mc->rightLen;

    slAddHead(&subset->components, subMc);
    }
slReverse(&subset->components);
return subset;
}

void mafMoveComponentToTop(struct mafAli *maf, char *componentSource)
/* Move given component to head of component list. */
{
struct mafComp *mcMaster = mafFindComponent(maf, componentSource);
slRemoveEl(&maf->components, mcMaster);
slAddHead(&maf->components, mcMaster);
}

boolean mafNeedSubset(struct mafAli *maf, char *componentSource,
	int newStart, int newEnd)
/* Return TRUE if maf only partially fits between newStart/newEnd
 * in given component. */
{
struct mafComp *mcMaster = mafFindComponent(maf, componentSource);

/* Reverse complement input range if necessary. */
if (mcMaster->strand == '-')
    reverseIntRange(&newStart, &newEnd, mcMaster->srcSize);

return newStart > mcMaster->start || newEnd < mcMaster->start + mcMaster->size;
}

void mafFlipStrand(struct mafAli *maf)
/* Reverse complement maf. */
{
struct mafComp *mc;
for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    int e = mc->start + mc->size;
    reverseIntRange(&mc->start, &e, mc->srcSize);
    if (mc->text != NULL)
        reverseComplement(mc->text, maf->textSize);
	if (mc->quality != NULL)
		reverseBytes(mc->quality, maf->textSize);
    if (mc->strand == '-')
        mc->strand = '+';
    else
        mc->strand = '-';
    char holdStatus = mc->leftStatus;
    mc->leftStatus = mc->rightStatus;
    mc->rightStatus = holdStatus;
    int holdLen = mc->leftLen;
    mc->leftLen = mc->rightLen;
    mc->rightLen = holdLen;
    }
}

void mafSrcDb(char *name, char *retDb, int retDbSize)
/* Parse out just database part of name (up to but not including
 * first dot). If dot found, return entire name */
{
int len;
char *e = strchr(name, '.');
/* Put prefix up to dot into buf. */
len = (e == NULL ? strlen(name) : e - name);
if (len >= retDbSize)
     len = retDbSize-1;
memcpy(retDb, name, len);
retDb[len] = 0;
}

boolean mafColumnEmpty(struct mafAli *maf, int col)
/* Return TRUE if the column is all '-' or '.' */
{
assert(col < maf->textSize);
struct mafComp *comp;
for (comp = maf->components; comp != NULL; comp = comp->next)
    if (comp->text != NULL)
        {
        char c = comp->text[col];
        if (c != '.' && c != '-')
            return FALSE;
        }
return TRUE;
}

void mafStripEmptyColumns(struct mafAli *maf)
/* Remove columns that are all '-' or '.' from  maf. */
{
/* Selectively copy over non-empty columns. */
int readIx=0, writeIx = 0;
struct mafComp *comp;
for (readIx=0; readIx < maf->textSize; ++readIx)
    {
    if (!mafColumnEmpty(maf, readIx))
        {
        for (comp = maf->components; comp != NULL; comp = comp->next) 
            {
            if(comp->text != NULL)
                comp->text[writeIx] = comp->text[readIx];
            if (comp->quality != NULL)
                comp->quality[writeIx] = comp->quality[readIx];
            }
        ++writeIx;
        }
    }
/* Zero terminate text, and update textSize. */
for (comp = maf->components; comp != NULL; comp = comp->next)
    {
    if (comp->text != NULL)
        comp->text[writeIx] = 0;
    if (comp->quality != NULL)
        comp->quality[writeIx] = 0;
    }
maf->textSize = writeIx;
}

struct mafRegDef *mafRegDefNew(char *type, int size, char *id)
/* construct a new mafRegDef object */
{
struct mafRegDef *mrd;
AllocVar(mrd);
if (sameString(type, mafRegDefTxUpstream))
    mrd->type = mafRegDefTxUpstream;
else
    errAbort("invalid mafRefDef type: %s", type);
mrd->size = size;
mrd->id = cloneString(id);
return mrd;
}

void mafRegDefFree(struct mafRegDef **mrdPtr)
/* Free a mafRegDef object */
{
struct mafRegDef *mrd = *mrdPtr;
if (mrd != NULL)
    {
    freeMem(mrd->id);
    freeMem(mrd);
    *mrdPtr = NULL;
    }
}

boolean isContigOrTandem(char status)
/* is status MAF_CONTIG_STATUS or MAF_TANDEM_STATUS */
{
return ((status == MAF_CONTIG_STATUS) ||
	(status == MAF_TANDEM_STATUS));
}

struct mafComp *mafCompClone(struct mafComp *srcComp)
/* clone a mafComp */
{
struct mafComp *comp;
AllocVar(comp);
comp->src = cloneString(srcComp->src);
comp->srcSize = srcComp->srcSize;
comp->strand = srcComp->strand;
comp->start = srcComp->start;
comp->size = srcComp->size;
comp->text = cloneString(srcComp->text);
comp->quality = cloneString(srcComp->quality);
comp->leftStatus = srcComp->leftStatus;
comp->leftLen = srcComp->leftLen;
comp->rightStatus = srcComp->rightStatus;
comp->rightLen = srcComp->rightLen;
return comp;
}

static struct mafRegDef *mafRegDefClone(struct mafRegDef *srcRegDef)
/* clone a srcRegDef */
{
return mafRegDefNew(srcRegDef->type, srcRegDef->size, srcRegDef->id);
}

struct mafAli *mafAliClone(struct mafAli *srcAli)
/* clone a mafAli */
{
struct mafAli *ali;
AllocVar(ali);
ali->score = srcAli->score;
struct mafComp *srcComp;
for (srcComp = srcAli->components; srcComp != NULL; srcComp = srcComp->next)
    slAddHead(&ali->components, mafCompClone(srcComp));
slReverse(&ali->components);
ali->textSize = srcAli->textSize;
if (srcAli->regDef != NULL)
    ali->regDef = mafRegDefClone(srcAli->regDef);
return ali;
}
