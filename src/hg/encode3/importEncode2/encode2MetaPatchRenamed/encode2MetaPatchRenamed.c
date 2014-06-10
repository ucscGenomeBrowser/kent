/* encode2MetaPatchRenamed - Patch in renamed files (with same metadata) to encode2Meta. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "meta.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2MetaPatchRenamed - Patch in renamed files (with same metadata) to encode2Meta\n"
  "usage:\n"
  "   encode2MetaPatchRenamed inputMeta patchFile outputMeta\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct patch
/* Something to patch - one source, possibly many destinations. */
    {
    struct patch *next;
    char *source;  /* Original item. */
    struct slName *destList;	/* Where all it goes to. */
    };

struct patch *patchLoadAll(char *fileName)
/* Load in file with one patch per line in the format:
 *    original new1 new2 ... newN
 * This suppresses patches where original and new are the same. */
{
struct patch *list = NULL, *patch;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
while (lineFileNextReal(lf, &line))
    {
    AllocVar(patch);
    patch->source = cloneString(nextWord(&line));
    char *dest;
    while ((dest = nextWord(&line)) != NULL)
	slNameAddTail(&patch->destList, cloneString(dest));
    if (patch->destList != NULL)
        slAddHead(&list, patch);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct meta *metaFindFirstMatch(struct meta *metaList, char *tag, char *val)
/* Find first meta on list (including children) that has tag matching val */
{
struct meta *meta;
for (meta = metaList; meta != NULL; meta = meta->next)
    {
    /* Do depth first search so low level tags override higher. */
    if (meta->children)
        {
	struct meta *match = metaFindFirstMatch(meta->children, tag, val);
	if (match != NULL)
	    return match;
	}
    char *ourVal = metaLocalTagVal(meta, tag);
    if (ourVal != NULL && sameString(ourVal, val))
        return meta;
    }
return NULL;
}

struct meta **metaRemove(struct meta **pList, struct meta *target)
/* Remove target from *pList.  Return pointer to where we removed it from
 * in case we want to add it or something like it back.   Will return NULL
 * (arguably it should abort) if not found on list. */
{
struct meta *meta;
for (meta = *pList; meta != NULL; meta = meta->next)
    {
    if (meta == target)
        {
	*pList = meta->next;
	return pList;
	}
    else
        pList = &meta->next;
    if (meta->children != NULL)
        {
	struct meta **oldSpot = metaRemove(&meta->children, target);
	if (oldSpot != NULL)
	    return oldSpot;
	}
    }
return NULL;
}

#ifdef UNUSED
struct metaTagVal *metaTagValListClone(struct metaTagVal *oldList)
/* Make a list that is a clone of the current list, but in it's own memory. */
{
struct metaTagVal *newList = NULL, *mtv, *clone;
for (mtv = oldList; mtv != NULL; mtv  = mtv->next)
    {
    clone = metaTagValNew(mtv->tag, mtv->val);
    slAddHead(&newList, clone);
    }
slReverse(&newList);
return newList;
}
#endif /* UNUSED */

struct metaTagVal *metaTagValListRemove(struct metaTagVal *oldList, char *tag)
/* Return list that has tag removed. */
{
struct metaTagVal *newList = NULL, *mtv, *next;
for (mtv = oldList; mtv != NULL; mtv = next)
    {
    next = mtv->next;
    if (!sameString(mtv->tag, tag))
        slAddHead(&newList, mtv);
    }
slReverse(&newList);
return newList;
}

void encode2MetaPatchRenamed(char *inputMeta, char *patchFile, char *outputMeta)
/* encode2MetaPatchRenamed - Patch in renamed files (with same metadata) to encode2Meta. */
{
struct meta *metaList = metaLoadAll(inputMeta, NULL, NULL, FALSE, FALSE);
verbose(1, "%d in metaList\n", slCount(metaList));
struct patch *patch, *patchList = patchLoadAll(patchFile);
verbose(1, "%d in patchList\n", slCount(patchList));
for (patch =  patchList; patch != NULL; patch = patch->next)
    {
    verbose(2, "patching %s\n", patch->source);
    struct meta *meta = metaFindFirstMatch(metaList, "fileName", patch->source);
    if (meta == NULL)
	{
        verbose(2, "Can't find %s in %s", patch->source, inputMeta);
	continue;
	}
    struct meta *parent = meta->parent;
    assert(parent != NULL);
    boolean multiPatch = (slCount(patch->destList) > 1);
    if (multiPatch)
        {
	struct meta **oldSpot = &meta->children;
	struct slName *dest;
	int destId = 0;
	for (dest = patch->destList; dest != NULL; dest = dest->next)
	    {
	    /* Make up new object name if patching in more than one new thing. */
	    char metaObjName[PATH_LEN];
	    safef(metaObjName, sizeof(metaObjName), "%sSub%d", meta->name, ++destId);

	    /* Create subobject. */
	    struct meta *sub;
	    AllocVar(sub);
	    sub->tagList = metaTagValNew("meta", metaObjName);
	    sub->name = sub->tagList->val;
	    sub->tagList->next = metaTagValNew("fileName", dest->name);

	    /* And integrate into tree. */
	    sub->parent = meta;
	    sub->next = *oldSpot;
	    *oldSpot = sub;
	    oldSpot = &sub->next;
	    }
	/* Remove fileName element from parent. */
	meta->tagList = metaTagValListRemove(meta->tagList, "fileName"); 
	}
    else
        {
	metaAddTag(meta, "fileName", patch->destList->name);
	}
    }
metaWriteAll(metaList, outputMeta, 3, FALSE);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
encode2MetaPatchRenamed(argv[1], argv[2], argv[3]);
return 0;
}
