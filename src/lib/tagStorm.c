/* tagStorm - stuff to parse and interpret a genome-hub metadata.txt file, which is in 
 * a hierarchical ra format.  That is something like:
 *     cellLine HELA
 *     lab ucscCore
 *
 *         target H3K4Me3
 *         antibody abCamAntiH3k4me3
 *       
 *            fileName hg19/chipSeq/helaH3k4me3.narrowPeak.bigBed
 *            format narrowPeak
 *
 *            fileName hg19/chipSeq/helaH3K4me3.broadPeak.bigBed
 *            formet broadPeak
 *
 *         target CTCF
 *         antibody abCamAntiCtcf
 *
 *            fileName hg19/chipSeq/helaCTCF.narrowPeak.bigBed
 *            format narrowPeak
 *
 *            fileName hg19/chipSeq/helaCTCF.broadPeak.bigBed
 *            formet broadPeak
 *
 * The file is interpreted so that lower level stanzas inherit tags from higher level ones.
 */


#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "ra.h"
#include "tagStorm.h"
#include "errAbort.h"


static void rReverseStanzaList(struct tagStanza **pList)
/* Reverse order of stanzas, used to compensate for all of the add-heads */
{
slReverse(pList);
struct tagStanza *stanza;
for (stanza = *pList; stanza != NULL; stanza = stanza->next)
    {
    if (stanza->children != NULL)
	rReverseStanzaList(&stanza->children);
    }
}

struct tagStorm *tagStormFromFile(char *fileName)
/* Load up all tags from file.  */
{
int depth = 0, maxDepth = 32;
int indentStack[maxDepth];
indentStack[0] = 0;

/* Open up file first thing.  Abort if there's a problem here. */
struct lineFile *lf = lineFileOpen(fileName, TRUE);

/* Set up local memory pool and put tagStorm base structure in it. */
struct lm *lm = lmInit(0);
struct tagStorm *tagStorm = lmAlloc(lm, sizeof(*tagStorm));
tagStorm->lm = lm;
tagStorm->fileName = lmCloneString(lm, fileName);

struct tagStanza *stanza, *parent = NULL, *lastStanza = NULL;
int currentIndent = 0;
int stanzaCount = 0;
int tagCount = 0;
while (raSkipLeadingEmptyLines(lf, NULL))
    {
    ++stanzaCount;
    char *tag, *val;
    int stanzaIndent, tagIndent;
    lmAllocVar(lm, stanza);
    struct slPair *pairList = NULL, *pair;
    while (raNextTagValWithIndent(lf, &tag, &val, NULL, &tagIndent))
        {
	lmAllocVar(lm, pair);
	pair->name = lmCloneString(lm, tag);
	pair->val = lmCloneString(lm, val);
	
	if (pairList == NULL)	/* If this is first tag of a new stanza check indentation
	                         * and put stanza in appropriate level of hierarchy */
	    {
	    if (tagIndent != currentIndent)
		{
		stanzaIndent = tagIndent;
		if (stanzaIndent > currentIndent)
		    {
		    if (++depth >= maxDepth)
		        errAbort("Tags nested too deep line %d of %s. Max nesting is %d",
			    lf->lineIx, lf->fileName, maxDepth);
		    indentStack[depth] = stanzaIndent;
		    if (lastStanza == NULL)
			errAbort("Initial stanza needs to be non-indented line %d of %s", 
			    lf->lineIx, lf->fileName);
		    parent = lastStanza;
		    }
		else  /* going up */
		    {
		    /* Find stanza in parent chain at same level of indentation.  This
		     * will be an older sibling */
		    struct tagStanza *olderSibling;
		    for (olderSibling = parent; olderSibling != NULL; 
			olderSibling = olderSibling->parent)
		        {
			--depth;
			if (indentStack[depth] == stanzaIndent)
			    break;
			}
		    if (olderSibling == NULL)
		        {
			warn("Indentation inconsistent line %d of %s.", lf->lineIx, lf->fileName);
			warn("If you are using tabs, check your tab stop is set to 8.");
			warn("Otherwise check that when you are reducing indentation in a stanza");
			warn("that it is the same as the previous stanza at the same level.");
			noWarnAbort();
			}
		    parent = olderSibling->parent;
		    }
		currentIndent = tagIndent;
		}
	    if (parent == NULL)
	        slAddHead(&tagStorm->forest, stanza);
	    else
		slAddHead(&parent->children, stanza);
	    stanza->parent = parent;
	    pairList = pair;
	    lastStanza = stanza;
	    }
	else
	    {
	    if (tagIndent != currentIndent)
	         errAbort("Tags in stanza inconsistently indented line %d of %s",
		    lf->lineIx, lf->fileName);
	    slAddHead(&pairList, pair);
	    }
	++tagCount;
	}
    slReverse(&pairList);
    stanza->tagList = pairList;
    }
lineFileClose(&lf);
printf("%d tags in %d stanzas\n", tagCount, stanzaCount);
rReverseStanzaList(&tagStorm->forest);
return tagStorm;
}

void tagStormFree(struct tagStorm **pTagStorm)
/* Free up memory associated with tag storm */
{
struct tagStorm *tagStorm = *pTagStorm;
if (tagStorm != NULL)
    {
    lmCleanup(&tagStorm->lm);
    *pTagStorm = NULL;
    }
}

char *tagStanzaVal(struct tagStanza *stanza, char *tag)
/* Return value associated with tag in stanza or any of parent stanzas */
{
while (stanza != NULL)
    {
    char *val = slPairFindVal(stanza->tagList, tag);
    if (val != NULL)
         return val;
    stanza = stanza->parent;
    }
return NULL;
}

static void rAddIndex(struct tagStanza *list, struct hash *hash, char *tag, char *parentVal)
/* Add stanza to hash index if it has a value for tag, or if val is passed in. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    char *val = slPairFindVal(stanza->tagList, tag);
    if (val == NULL)
	val = parentVal;
    if (val != NULL)
	hashAdd(hash, val, stanza);
    if (stanza->children != NULL)
        rAddIndex(stanza->children, hash, tag, val);
    }
}

struct hash *tagStormIndex(struct tagStorm *tagStorm, char *tag)
/* Produce a hash of stanzas containing a tag (or whose parents contain tag
 * keyed by tag value */
{
struct hash *hash = hashNew(0);
rAddIndex(tagStorm->forest, hash, tag, NULL);
return hash;
}

static void rTsWrite(struct tagStanza *list, FILE *f, int maxDepth, int depth)
/* Recursively write out list to file */
{
if (depth >= maxDepth)
    return;
struct tagStanza *stanza;
int indent = depth * 3;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *pair;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
        {
	spaceOut(f, indent);
	fprintf(f, "%s %s\n", pair->name, (char*)(pair->val));
	}
    fputc('\n', f);
    rTsWrite(stanza->children, f, maxDepth, depth+1);
    }
}
 
void tagStormWrite(struct tagStorm *tagStorm, char *fileName, int maxDepth)
/* Write all of tag storm to file.  If maxDepth is nonzero, just write up to that depth */
{
FILE *f = mustOpen(fileName, "w");
if (maxDepth == 0)
    maxDepth = BIGNUM;
rTsWrite(tagStorm->forest, f, maxDepth, 0);
carefulClose(&f);
}

static void rTsWriteAsFlatRa(struct tagStanza *list, FILE *f, char *idTag, boolean withParent,
     int maxDepth, int depth)
/* Recursively write out list to file */
{
if (depth > maxDepth)
    return;
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    char *idVal = NULL;
    if (idTag != NULL)
        idVal = slPairFindVal(stanza->tagList, idTag);
    struct hash *uniq = hashNew(0);
    if (idVal != NULL)
        {
	fprintf(f, "%s %s\n", idTag, idVal);
	hashAdd(uniq, idTag, idVal);
	}
    if (withParent && stanza->parent != NULL && idTag != NULL)
	{
	char *parentVal = slPairFindVal(stanza->parent->tagList, idTag);
	if (parentVal != NULL)
	    {
	    fprintf(f, "parent %s\n", parentVal);
	    hashAdd(uniq, "parent", parentVal);
	    }
	}
    struct tagStanza *family;
    for (family = stanza; family != NULL; family = family->parent)
	{
	struct slPair *pair;
	for (pair = family->tagList; pair != NULL; pair = pair->next)
	    {
	    if (!hashLookup(uniq, pair->name))
		{
		fprintf(f, "%s %s\n", pair->name, (char*)(pair->val));
		hashAdd(uniq, pair->name, pair->val);
		}
	    }
	}
    fputc('\n', f);
    hashFree(&uniq);

    rTsWriteAsFlatRa(stanza->children, f, idTag, withParent, maxDepth, depth+1);
    }
}
 
void tagStormWriteAsFlatRa(struct tagStorm *tagStorm, char *fileName, char *idTag, 
    boolean withParent, int maxDepth)
/* Write tag storm flattening out hierarchy so kids have all of parents tags in .ra format */
{
FILE *f = mustOpen(fileName, "w");
rTsWriteAsFlatRa(tagStorm->forest, f, idTag, withParent, maxDepth, 0);
carefulClose(&f);
}

void tagStormAdd(struct tagStorm *tagStorm, struct tagStanza *stanza, char *tag, char *val)
/* Add tag to stanza in storm, replacing existing tag if any */
{
struct lm *lm = tagStorm->lm;

/* First loop through to replace an existing tag. */
struct slPair *pair;
for (pair = stanza->tagList; pair != NULL; pair = pair->next)
    {
    if (sameString(pair->name, tag))
       {
       pair->val = lmCloneString(lm, val);
       return;
       }
    }
/* If didn't make it then add new tag (at end) */
lmAllocVar(lm, pair);
pair->name = lmCloneString(lm, tag);
pair->val = lmCloneString(lm, val);
slAddTail(&stanza->tagList, pair);
}

