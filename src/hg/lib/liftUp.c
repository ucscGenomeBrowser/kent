/* liftUp - stores offsets for translating coordinates. */
#include "common.h"
#include "linefile.h"
#include "portable.h"
#include "hash.h"
#include "liftUp.h"

static char const rcsid[] = "$Id: liftUp.c,v 1.4 2008/09/17 18:10:14 kent Exp $";

struct liftSpec *readLifts(char *fileName)
/* Read in lift file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount;
char *words[16];
struct liftSpec *list = NULL, *el;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    char *offs;
    if (wordCount < 5)
        errAbort("Need at least 5 words line %d of %s", lf->lineIx, lf->fileName);
    offs = words[0];
    if (!isdigit(offs[0]) && !(offs[0] == '-' && isdigit(offs[1])))
	errAbort("Expecting number in first field line %d of %s", lf->lineIx, lf->fileName);
    if (!isdigit(words[4][0]))
	errAbort("Expecting number in fifth field line %d of %s", lf->lineIx, lf->fileName);
    AllocVar(el);
    el->offset = atoi(offs);
    el->oldName = cloneString(words[1]);
    el->oldSize = atoi(words[2]);
    el->newName = cloneString(words[3]);
    el->newSize = atoi(words[4]);
    if (wordCount >= 6)
        {
	char c = words[5][0];
	if (c == '+' || c == '-')
	    el->strand = c;
	else
	    errAbort("Expecting + or - field 6, line %d of %s", lf->lineIx, lf->fileName);
	}
    else
        el->strand = '+';
    slAddHead(&list, el);
    }
slReverse(&list);
lineFileClose(&lf);
if (list == NULL)
    errAbort("Empty liftSpec file %s", fileName);
return list;
}

struct hash *hashLift(struct liftSpec *list, boolean revOk)
/* Return a hash of the lift spec.   If revOk, allow - strand elements. */
{
struct hash *hash = newHash(0);
struct liftSpec *el;
for (el = list; el != NULL; el = el->next)
    {
    if (!revOk && el->strand != '+')
        errAbort("Can't lift from minus strand contigs (like %s) on this file type", el->oldName);
    if (hashLookup(hash, el->oldName))
        /* tolerate multiple instances of gap lines (residue from AGP's) */
        if (!sameString(el->oldName, "gap"))
            errAbort("%s appears twice in .lft file\n", el->oldName);
    hashAdd(hash, el->oldName, el);
    }
return hash;
}

