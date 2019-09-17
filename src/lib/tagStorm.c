/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* tagStorm - stuff to parse and interpret a genome-hub metadata.txt file, which is in 
 * a hierarchical ra format.  That is something like:
 *     cellLine HELA
 *     lab ucscCore
 *
 *         target H3K4Me3
 *         antibody abCamAntiH3k4me3
 *       
 *            file hg19/chipSeq/helaH3k4me3.narrowPeak.bigBed
 *            format narrowPeak
 *
 *            file hg19/chipSeq/helaH3K4me3.broadPeak.bigBed
 *            format broadPeak
 *
 *         target CTCF
 *         antibody abCamAntiCtcf
 *
 *            file hg19/chipSeq/helaCTCF.narrowPeak.bigBed
 *            format narrowPeak
 *
 *            file hg19/chipSeq/helaCTCF.broadPeak.bigBed
 *            format broadPeak
 *
 * The file is interpreted so that lower level stanzas inherit tags from higher level ones.
 */


#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "ra.h"
#include "errAbort.h"
#include "rql.h"
#include "tagStorm.h"
#include "csv.h"


struct tagStorm *tagStormNew(char *name)
/* Create a new, empty, tagStorm. */
{
struct lm *lm = lmInit(0);
struct tagStorm *tagStorm = lmAlloc(lm, sizeof(*tagStorm));
tagStorm->lm = lm;
tagStorm->fileName = lmCloneString(lm, name);
return tagStorm;
}

struct tagStanza *tagStanzaNew(struct tagStorm *tagStorm, struct tagStanza *parent)
/* Create a new, empty stanza that is added as to head of child list of parent,
 * or to tagStorm->forest if parent is NULL. */
{
struct tagStanza *stanza;
lmAllocVar(tagStorm->lm, stanza);
if (parent != NULL)
    {
    stanza->parent = parent;
    slAddHead(&parent->children, stanza);
    }
else
    {
    slAddHead(&tagStorm->forest, stanza);
    }
return stanza;
}

struct tagStanza *tagStanzaNewAtEnd(struct tagStorm *tagStorm, struct tagStanza *parent)
/* Create a new, empty stanza that is added as to head of child list of parent,
 * or to tagStorm->forest if parent is NULL. */
/* Create new empty stanza that is added at tail of child list of parent */
{
struct tagStanza *stanza;
lmAllocVar(tagStorm->lm, stanza);
if (parent != NULL)
    {
    stanza->parent = parent;
    slAddTail(&parent->children, stanza);
    }
else
    {
    slAddTail(&tagStorm->forest, stanza);
    }
return stanza;
}

struct slPair *tagStanzaAdd(struct tagStorm *tagStorm, struct tagStanza *stanza, 
    char *tag, char *val)
/* Add tag with given value to beginning of stanza */
{
struct lm *lm = tagStorm->lm;
struct slPair *pair;
lmAllocVar(lm, pair);
pair->name = lmCloneString(lm, tag);
pair->val = lmCloneString(lm, val);
slAddHead(&stanza->tagList, pair);
return pair;
}

struct slPair *tagStanzaAppend(struct tagStorm *tagStorm, struct tagStanza *stanza, 
    char *tag, char *val)
/* Add tag with given value to end of stanza */
{
struct lm *lm = tagStorm->lm;
struct slPair *pair;
lmAllocVar(lm, pair);
pair->name = lmCloneString(lm, tag);
pair->val = lmCloneString(lm, val);
slAddTail(&stanza->tagList, pair);
return pair;
}

void tagStanzaAddLongLong(struct tagStorm *tagStorm, struct tagStanza *stanza, char *var, 
    long long val)
/* Add long long integer valued tag to stanza */
{
char buf[32];
safef(buf, sizeof(buf), "%lld", val);
tagStanzaAdd(tagStorm, stanza, var, buf);
}

void tagStanzaAddDouble(struct tagStorm *tagStorm, struct tagStanza *stanza, char *var, 
    double val)
/* Add double valued tag to stanza */
{
char buf[32];
safef(buf, sizeof(buf), "%g", val);
tagStanzaAdd(tagStorm, stanza, var, buf);
}

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

void tagStormReverseAll(struct tagStorm *tagStorm)
/* Reverse order of all lists in tagStorm.  Use when all done with tagStormAddStanza
 * and tagStanzaAddTag (which for speed build lists backwards). */
{
rReverseStanzaList(&tagStorm->forest);
}

struct tagStorm *tagStormFromFile(char *fileName)
/* Load up all tags from file.  */
{
int depth = 0, maxDepth = 32;
int indentStack[maxDepth];
indentStack[0] = 0;

/* Open up file first thing.  Abort if there's a problem here. */
struct lineFile *lf = lineFileUdcMayOpen(fileName, TRUE);
if (lf == NULL)
    errAbort("Cannot open tagStorm file %s\n", fileName);

/* Set up new empty tag storm and get local pointer to memory pool. */
struct tagStorm *tagStorm = tagStormNew(fileName);
struct lm *lm = tagStorm->lm;

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
    stanza->startLineIx = lf->lineIx;
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

static void rAddIndex(struct tagStorm *tagStorm, struct tagStanza *list, 
    struct hash *hash, char *tag, char *parentVal,
    boolean unique, boolean inherit)
/* Add stanza to hash index if it has a value for tag, or if val is passed in. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    char *val = slPairFindVal(stanza->tagList, tag);
    if (val == NULL && inherit)
	val = parentVal;
    if (val != NULL)
	{
	if (unique)
	    {
	    struct tagStanza *oldStanza = hashFindVal(hash, val);
	    if (oldStanza != NULL)
	        errAbort("tag %s value %s not unique in %s", tag, val, tagStorm->fileName);
	    }
	hashAdd(hash, val, stanza);
	}
    if (stanza->children != NULL)
        rAddIndex(tagStorm, stanza->children, hash, tag, val, unique, inherit);
    }
}


struct hash *tagStormIndexExtended(struct tagStorm *tagStorm, char *tag, 
    boolean unique, boolean inherit)
/* Produce a hash of stanzas containing a tag keyed by tag value. 
 * If unique parameter is set then the tag values must all be unique
 * If inherit is set then tags set in parent stanzas will be considered too. */
{
struct hash *hash = hashNew(0);
rAddIndex(tagStorm, tagStorm->forest, hash, tag, NULL, unique, inherit);
return hash;
}

struct hash *tagStormIndex(struct tagStorm *tagStorm, char *tag)
/* Produce a hash of stanzas containing a tag keyed by tag value */
{
return tagStormIndexExtended(tagStorm, tag, FALSE, TRUE);
}


struct hash *tagStormUniqueIndex(struct tagStorm *tagStorm, char *tag)
/* Produce a hash of stanzas containing a tag where tag is unique across
 * stanzas */
{
return tagStormIndexExtended(tagStorm, tag, TRUE, TRUE);
}

struct tagStanza *tagStanzaFindInHash(struct hash *hash, char *key)
/* Find tag stanza that matches key in an index hash returned from tagStormUniqueIndex.
 * Returns NULL if no such stanza in the hash.
 * (Just a wrapper around hashFindVal.)  Do not free tagStanza that it returns. For
 * multivalued indexes returned from tagStormIndex use hashLookup and hashLookupNext. */
{
return hashFindVal(hash, key);
}

void tagStanzaSimpleWrite(struct tagStanza *stanza, FILE *f, int depth)
/* Write out tag stanza to file.  Do not recurse or add last blank line */
{
struct slPair *pair;
for (pair = stanza->tagList; pair != NULL; pair = pair->next)
    {
    repeatCharOut(f, '\t', depth);
    fprintf(f, "%s %s\n", pair->name, (char*)(pair->val));
    }
}

void tagStanzaRecursiveWrite(struct tagStanza *list, FILE *f, int maxDepth, int depth)
/* Recursively write out stanza list and children to open file */
{
if (depth >= maxDepth)
    return;
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (stanza->tagList == NULL)
        {
	repeatCharOut(f, '\t', depth);
	fprintf(f, "#empty\n");
	}
    tagStanzaSimpleWrite(stanza, f, depth);
    fputc('\n', f);
    tagStanzaRecursiveWrite(stanza->children, f, maxDepth, depth+1);
    }
}
 
void tagStormWrite(struct tagStorm *tagStorm, char *fileName, int maxDepth)
/* Write all of tag storm to file.  If maxDepth is nonzero, just write up to that depth */
{
FILE *f = mustOpen(fileName, "w");
if (maxDepth == 0)
    maxDepth = BIGNUM;
tagStanzaRecursiveWrite(tagStorm->forest, f, maxDepth, 0);
carefulClose(&f);
}

static void rTsWriteAsFlatRa(struct tagStanza *list, FILE *f, char *idTag, boolean withParent,
     boolean leavesOnly, int maxDepth, int depth)
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
    if (!leavesOnly || stanza->children == NULL)
	{
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
	}
    hashFree(&uniq);

    rTsWriteAsFlatRa(stanza->children, f, idTag, withParent, leavesOnly, maxDepth, depth+1);
    }
}
 
void tagStormWriteAsFlatRa(struct tagStorm *tagStorm, char *fileName, char *idTag, 
    boolean withParent, int maxDepth, boolean leavesOnly)
/* Write tag storm flattening out hierarchy so kids have all of parents tags in .ra format */
{
FILE *f = mustOpen(fileName, "w");
rTsWriteAsFlatRa(tagStorm->forest, f, idTag, withParent, leavesOnly, maxDepth, 0);
carefulClose(&f);
}

static void rTsWriteAsFlatTab(struct tagStanza *list, struct slName *fieldList,
    FILE *f, char *idTag, boolean withParent,
     int maxDepth, int depth, boolean leavesOnly, char *nullVal, boolean isCsv)
/* Recursively write out list to file */
{
if (depth > maxDepth)
    return;
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    char *idVal = NULL;
    if (idTag != NULL)
	{
        idVal = slPairFindVal(stanza->tagList, idTag);
	}
    if (idTag == NULL || idVal != NULL)
	{
	struct hash *uniq = hashNew(0);
	if (withParent && stanza->parent != NULL && idTag != NULL)
	    {
	    char *parentVal = slPairFindVal(stanza->parent->tagList, idTag);
	    if (parentVal != NULL)
		{
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
		    hashAdd(uniq, pair->name, pair->val);
		    }
		}
	    }
	if (!leavesOnly || stanza->children == NULL)
	    {
	    struct slName *field;
	    for (field = fieldList; field != NULL; field = field->next)
		{
		if (field != fieldList)
		    {
		    if (isCsv)
			fputc(',', f);
		    else
			fputc('\t', f);
		    }
		char *val = hashFindVal(uniq, field->name);
		if (val == NULL)
		    val = nullVal;
		if (isCsv)
		    csvWriteVal(val, f);
		else
		    fputs(val, f);
		}
	    fputc('\n', f);
	    }
	hashFree(&uniq);
	}

    rTsWriteAsFlatTab(stanza->children, fieldList, f, idTag, withParent, maxDepth, depth+1, 
	leavesOnly, nullVal, isCsv);
    }
}

void tagStormWriteAsFlatTabOrCsv(struct tagStorm *tagStorm, char *fileName, char *idTag, 
    boolean withParent, int maxDepth, boolean leavesOnly, char *nullVal, boolean sharpLabel, boolean isCsv)
/* Write tag storm flattening out hierarchy so kids have all of parents tags in .tsv format */
{
FILE *f = mustOpen(fileName, "w");
struct slName *fieldList = tagStormFieldList(tagStorm), *field;
if (withParent && slNameFind(fieldList, "parent") == NULL)
    slNameAddHead(&fieldList, "parent");
if (maxDepth == 0)
    maxDepth = BIGNUM;
if (sharpLabel)
    fputc('#', f);
for (field = fieldList; field != NULL; field = field->next)
    {
    if (field != fieldList)
	{
	if (isCsv)
	    fputc(',', f);
	else
	    fputc('\t', f);
	}
    if (isCsv)
	csvWriteVal(field->name, f);
    else
	fputs(field->name, f);
    }
fputc('\n', f);
rTsWriteAsFlatTab(tagStorm->forest, fieldList, f, idTag, withParent, maxDepth, 0, leavesOnly,
    nullVal, isCsv);
carefulClose(&f);
}

void tagStormWriteAsFlatTab(struct tagStorm *tagStorm, char *fileName, char *idTag, 
    boolean withParent, int maxDepth, boolean leavesOnly, char *nullVal, boolean sharpLabel)
/* Write tag storm flattening out hierarchy so kids have all of parents tags in .tsv format */
{
tagStormWriteAsFlatTabOrCsv(tagStorm, fileName, idTag, 
    withParent, maxDepth, leavesOnly, nullVal, sharpLabel, FALSE);
}

void tagStormWriteAsFlatCsv(struct tagStorm *tagStorm, char *fileName, char *idTag, 
    boolean withParent, int maxDepth, boolean leavesOnly, char *nullVal)
/* Write tag storm flattening out hierarchy so kids have all of parents tags in .csv format */
{
tagStormWriteAsFlatTabOrCsv(tagStorm, fileName, idTag, 
    withParent, maxDepth, leavesOnly, nullVal, FALSE, TRUE);
}

void tagStanzaUpdateTag(struct tagStorm *tagStorm, struct tagStanza *stanza, char *tag, char *val)
/* Add tag to stanza in storm, replacing existing tag if any. If tag is added it's added to
 * end. */
{
struct lm *lm = tagStorm->lm;

/* First loop through to replace an existing tag. */
struct slPair *pair;
for (pair = stanza->tagList; pair != NULL; pair = pair->next)
    {
    if (sameString(pair->name, tag))
       {
       if (!sameString(pair->val, val))
	   {
	   verbose(3, "Updating %s from '%s' to '%s'\n", pair->name, (char *)pair->val, val);
	   pair->val = lmCloneString(lm, val);
	   }
       return;
       }
    }
/* If didn't make it then add new tag (at start) */
lmAllocVar(lm, pair);
pair->name = lmCloneString(lm, tag);
pair->val = lmCloneString(lm, val);
slAddTail(&stanza->tagList, pair);
}

char *tagFindLocalVal(struct tagStanza *stanza, char *name)
/* Return value of tag of given name within stanza, or NULL * if tag does not exist. 
 * This does *not* look at parent tags. */
{
return slPairFindVal(stanza->tagList, name);
}

char *tagFindVal(struct tagStanza *stanza, char *name)
/* Return value of tag of given name within stanza or any of it's parents. */
{
struct tagStanza *ancestor;
for (ancestor = stanza; ancestor != NULL; ancestor = ancestor->parent)
    {
    char *val = slPairFindVal(ancestor->tagList, name);
    if (val != NULL)
        return val;
    }
return NULL;
}

void tagStanzaDeleteTag(struct tagStanza *stanza, char *tag)
/* Remove a tag from a stanza */
{
struct slPair *p = slPairFind(stanza->tagList, tag);
if (p != NULL)
    slRemoveEl(&stanza->tagList, p);
}

void rRemoveEmpties(struct tagStanza **pList)
/* Remove empty stanzas.  Replace them on list with children if any */
{
struct tagStanza *stanza;
for (;;)
    {
    stanza = *pList;
    if (stanza == NULL)
        break;
    if (stanza->children != NULL)
        rRemoveEmpties(&stanza->children);
    if (stanza->tagList == NULL)
        {
	verbose(3, "removing empty stanza with %d children and %d remaining sibs\n", 
	    slCount(stanza->children), slCount(stanza->next));
	*pList = slCat(stanza->children, stanza->next);
	stanza->next = stanza->children = NULL;
	}
    else
        {
	pList = &stanza->next;
	}
    }
}

void tagStormRemoveEmpties(struct tagStorm *tagStorm)
/* Remove any empty stanzas, promoting children if need be. */
{
rRemoveEmpties(&tagStorm->forest);
}

static boolean localTagRemove(struct tagStanza *stanza, char *tag)
/* Find given variable in list and remove it. Returns TRUE if it
 * actually removed it,  FALSE if it never found it. */
{
struct slPair **ln = &stanza->tagList;
struct slPair *v;
for (v = *ln; v != NULL; v = v->next)
    {
    if (sameString(v->name, tag))
        {
	*ln = v->next;
	return TRUE;
	}
    ln = &v->next;
    }
return FALSE;
}

static void hoistOne(struct tagStorm *tagStorm, struct tagStanza *stanza, char *tag, char *val)
/* We've already determined that tag exists and has same value in all children.
 * What we do here is add it to ourselves and remove it from children. */
{
tagStanzaUpdateTag(tagStorm, stanza, tag, val);
struct tagStanza *child;
for (child = stanza->children; child != NULL; child = child->next)
    localTagRemove(child, tag);
}

static struct slName *tagsInAny(struct tagStanza *stanzaList)
/* Return list of variables that are used in any node in list. */
{
struct hash *tagHash = hashNew(6);
struct slName *tag, *tagList = NULL;
struct tagStanza *stanza;
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *v;
    for (v = stanza->tagList; v != NULL; v = v->next)
        {
	if (!hashLookup(tagHash, v->name))
	    {
	    tag = slNameAddHead(&tagList, v->name);
	    hashAdd(tagHash, tag->name, tag);
	    }
	}
    }
hashFree(&tagHash);
return tagList;
}

static char *allSameVal(char *tag, struct tagStanza *stanzaList)
/* Return value of tag if it exists and is the same in each meta on list */
{
char *val = NULL;
struct tagStanza *stanza;
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    char *oneVal = slPairFindVal(stanza->tagList, tag);
    if (oneVal == NULL)
        return NULL;
    if (val == NULL)
        val = oneVal;
    else
        {
	if (!sameString(oneVal, val))
	    return NULL;
	}
    }
return val;
}

static void rHoist(struct tagStorm *tagStorm, struct tagStanza *stanza, char *selectedTag)
/* Move tags that are the same in all children up to parent. */
{
/* Do depth first recursion, but get early return if we're a leaf. */
struct tagStanza *child;
if (stanza->children == NULL)
    return;
for (child = stanza->children; child != NULL; child = child->next)
    rHoist(tagStorm, child, selectedTag);

/* Build up list of tags used in any child. */
struct slName *tag, *tagList = tagsInAny(stanza->children);

/* Go through list and figure out ones that are same in all children. */
for (tag = tagList; tag != NULL; tag = tag->next)
    {
    char *name = tag->name;
    if (selectedTag == NULL || sameString(name, selectedTag))
	{
	char *val;
	val = allSameVal(name, stanza->children);
	if (val != NULL)
	    hoistOne(tagStorm, stanza, name, val);
	}
    }
slFreeList(&tagList);
}

void tagStormHoist(struct tagStorm *tagStorm, char *selectedTag)
/* Hoist tags that are identical in all children to parent.  If selectedTag is
 * non-NULL, just do it for tag of that name rather than all tags. */
{
/* If there is more than one highest level stanza, make a dummy empty root one to hoist into. */
struct tagStanza *stanza;
if (slCount(tagStorm->forest) > 1)
    {
    struct tagStanza *root;
    lmAllocVar(tagStorm->lm, root);
    for (stanza = tagStorm->forest; stanza != NULL; stanza = stanza->next)
        stanza->parent = root;
    root->children = tagStorm->forest;
    tagStorm->forest = root;
    }

for (stanza = tagStorm->forest; stanza != NULL; stanza = stanza->next)
    rHoist(tagStorm, stanza, selectedTag);
tagStormRemoveEmpties(tagStorm);
}

static void rSortAlpha(struct tagStanza *list)
/* Recursively sort stanzas alphabetically */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    rSortAlpha(stanza->children);
    slSort(&stanza->tagList, slPairCmp);
    }
}

void tagStormAlphaSort(struct tagStorm *tagStorm)
/* Sort tags in stanza alphabetically */
{
rSortAlpha(tagStorm->forest);
}

// Sadly not thread safe helper vars for sorting according to predefined order
static char **sOrderFields;
static int sOrderCount;

static int cmpPairByOrder(const void *va, const void *vb)
/* Compare two slPairs according to order of names in sOrderFields */
{
const struct slPair *a = *((struct slPair **)va);
const struct slPair *b = *((struct slPair **)vb);
return cmpStringOrder(a->name, b->name, sOrderFields, sOrderCount);
}

static void rOrderSort(struct tagStanza *list)
/* Recursely sort by predefined order */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    rOrderSort(stanza->children);
    slSort(&stanza->tagList, cmpPairByOrder);
    }
}

void tagStormOrderSort(struct tagStorm *tagStorm, char **orderFields, int orderCount)
/* Sort tags in stanza to be in same order as orderFields input  which is orderCount long */
{
sOrderFields = orderFields;
sOrderCount = orderCount;
rOrderSort(tagStorm->forest);
}

static void rDeleteTags(struct tagStanza *stanzaList, char *tagName)
/* Recursively delete tag from all stanzas */
{
struct tagStanza *stanza;
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    tagStanzaDeleteTag(stanza, tagName);
    if (stanza->children)
	rDeleteTags(stanza->children, tagName);
    }
}

void tagStormDeleteTags(struct tagStorm *tagStorm, char *tagName)
/* Delete all tags of given name from tagStorm */
{
rDeleteTags(tagStorm->forest, tagName);
}

struct slPair *tagStanzaDeleteTagsInHash(struct tagStanza *stanza, struct hash *weedHash)
/* Delete any tags in stanza that have names that match hash. Return list of removed tags. */
{
struct slPair *pair, *next;
struct slPair *newList = NULL, *removedList = NULL;
for (pair = stanza->tagList; pair != NULL; pair = next)
    {
    next = pair->next;
    struct slPair **dest;
    if (hashLookup(weedHash, pair->name))
	dest = &removedList;
    else
        dest = &newList;
    slAddHead(dest, pair);
    }
slReverse(&newList);
stanza->tagList = newList;
slReverse(&removedList);
return removedList;
}

static void rSubTags(struct tagStanza *stanzaList, char *oldName, char *newName)
/* Recursively rename tag in all stanzas */
{
struct tagStanza *stanza;
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *pair = slPairFind(stanza->tagList, oldName);
    if (pair != NULL)
        pair->name = newName;
    if (stanza->children)
        rSubTags(stanza->children, oldName, newName);
    }
}

void tagStormSubTags(struct tagStorm *tagStorm, char *oldName, char *newName)
/* Rename all tags with oldName to newName */
{
char *newCopy = lmCloneString(tagStorm->lm, newName);
rSubTags(tagStorm->forest, oldName, newCopy);
}

void tagStanzaSubTagsInHash(struct tagStanza *stanza, struct hash *valHash)
/* Delete any tags in stanza that have names that match hash. Return list of removed tags. */
{
struct slPair *pair;
for (pair = stanza->tagList; pair != NULL; pair = pair->next)
    {
    char *val = hashFindVal(valHash, pair->name);
    if (val != NULL)
	pair->name = val;
    }
}


void tagStanzaRecursiveRemoveWeeds(struct tagStanza *list, struct hash *weedHash)
/* Recursively remove weeds in list and any children in list */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    tagStanzaDeleteTagsInHash(stanza, weedHash);
    tagStanzaRecursiveRemoveWeeds(stanza->children, weedHash);
    }
}

void tagStormWeedArray(struct tagStorm *tagStorm, char **weeds, int weedCount)
/* Remove all tags with names matching any of the weeds from storm */
{
struct hash *weedHash = hashFromNameArray(weeds, weedCount);
tagStanzaRecursiveRemoveWeeds(tagStorm->forest, weedHash);
freeHash(&weedHash);
}

void tagStanzaRecursiveSubTags(struct tagStanza *list, struct hash *subHash)
/* Recursively remove weeds in list and any children in list */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    tagStanzaSubTagsInHash(stanza, subHash);
    tagStanzaRecursiveSubTags(stanza->children, subHash);
    }
}

void tagStormSubArray(struct tagStorm *tagStorm, char *subs[][2], int subCount)
/* Substitute all tag names with substitutions from subs array */
{
struct hash *weedHash = hashFromNameValArray(subs, subCount);
tagStanzaRecursiveSubTags(tagStorm->forest, weedHash);
freeHash(&weedHash);
}



char *tagMustFindVal(struct tagStanza *stanza, char *name)
/* Return value of tag of given name within stanza or any of it's parents. Abort if
 * not found. */
{
char *val = tagFindVal(stanza, name);
if (val == NULL)
    errAbort("Can't find tag named %s in stanza", name);
return val;
}

static void rListFields(struct tagStanza *stanzaList, struct hash *uniq, struct slName **retList)
/* Recurse through stanzas adding tags we've never seen before to uniq hash and *retList */
{
struct tagStanza *stanza;
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *pair;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
        {
	if (!hashLookup(uniq, pair->name))
	    {
	    hashAdd(uniq, pair->name, NULL);
	    slNameAddHead(retList, pair->name);
	    }
	}
    rListFields(stanza->children, uniq, retList);
    }
}

struct slName *tagStormFieldList(struct tagStorm *tagStorm)
/* Return list of all fields in storm. */
{
struct slName *list = NULL;
struct hash *uniq = hashNew(0);
rListFields(tagStorm->forest, uniq, &list);
hashFree(&uniq);
slReverse(&list);
return list;
}

static void rCountFields(struct tagStanza *stanzaList, struct hash *hash)
/* Recurse through stanzas adding tags we've never seen before to uniq hash and *retList */
{
struct tagStanza *stanza;
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *pair;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
	hashIncInt(hash, pair->name);
    rCountFields(stanza->children, hash);
    }
}

struct hash *tagStormFieldHash(struct tagStorm *tagStorm)
/* Return an integer-valued hash of fields, keyed by tag name and with value
 * number of times field is used.  For most purposes just used to make sure
 * field exists though. */
{
struct hash *hash = hashNew(0);
rCountFields(tagStorm->forest, hash);
return hash;
}

static void rTagStormCountDistinct(struct tagStanza *list, char *tag, struct hash *uniq, char *requiredTag)
/* Fill in hash with number of times have seen each value of tag */
{
//char *requiredTag = "accession";
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if ((requiredTag == NULL) || tagFindVal(stanza, requiredTag))
	{
	char *val = tagFindVal(stanza, tag);
	if (val != NULL)
	    {
	    hashIncInt(uniq, val);
	    }
	}
    rTagStormCountDistinct(stanza->children, tag, uniq, requiredTag);
    }
}

struct hash *tagStormCountTagVals(struct tagStorm *tags, char *tag, char *required)
/* Return an integer valued hash keyed by all the different values
 * of tag seen in tagStorm.  The hash is filled with counts of the
 * number of times each value is used that can be recovered with 
 * hashIntVal(hash, key).  If requiredTag is not-NULL, stanza must 
 * have that tag. */
{
struct hash *uniq = hashNew(0);
rTagStormCountDistinct(tags->forest, tag, uniq, required);
return uniq;
}

static void rMaxDepth(struct tagStanza *list, int depth, int *pMaxDepth)
/* Recursively calculate max depth */
{
if (list == NULL)
    return;
++depth;
if (*pMaxDepth < depth) *pMaxDepth = depth;
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    rMaxDepth(stanza->children, depth, pMaxDepth);
}

int tagStormMaxDepth(struct tagStorm *ts)
/* Calculate deepest extent of tagStorm */
{
int maxDepth = 0;
rMaxDepth(ts->forest, 0, &maxDepth);
return maxDepth;
}

static void rCountStanzas(struct tagStanza *list, int *pCount)
/* Recursively count stanzas */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    *pCount += 1;
    rCountStanzas(stanza->children, pCount);
    }
}

int tagStormCountStanzas(struct tagStorm *ts)
/* Return number of stanzas in storm */
{
int count = 0;
rCountStanzas(ts->forest, &count);
return count;
}

static void rCountTags(struct tagStanza *list, int *pCount)
/* Return number of tags in storm */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    *pCount += slCount(stanza->tagList);
    rCountTags(stanza->children, pCount);
    }
}

int tagStormCountTags(struct tagStorm *ts)
/* Return number of stanzas in storm. Does not include expanding ancestors */
{
int count = 0;
rCountTags(ts->forest, &count);
return count;
}

int tagStormCountFields(struct tagStorm *ts)
/* Return number of distinct tag types (fields) in storm */
{
struct hash *hash = tagStormFieldHash(ts);
int count = hash->elCount;
hashFree(&hash);
return count;
}


struct slPair *tagListIncludingParents(struct tagStanza *stanza)
/* Return a list of all tags including ones defined in parents. */
{
struct hash *uniq = hashNew(0);
struct slPair *list = NULL;
struct tagStanza *ts;
for (ts = stanza; ts != NULL; ts = ts->parent)
    {
    struct slPair *pair;
    for (pair = ts->tagList; pair != NULL; pair = pair->next)
       {
       if (!hashLookup(uniq, pair->name))
           {
	   slPairAdd(&list, pair->name, pair->val);
	   hashAdd(uniq, pair->name, pair);
	   }
       }
    }
hashFree(&uniq);
slReverse(&list);
return list;
}

void tagStormTraverse(struct tagStorm *storm, struct tagStanza *stanzaList, void *context,
    void (*doStanza)(struct tagStorm *storm, struct tagStanza *stanza, void *context))
/* Traverse tagStormStanzas recursively applying doStanza with to each stanza in
 * stanzaList and any children.  Pass through context */
{
struct tagStanza *stanza;
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    (*doStanza)(storm, stanza, context);
    tagStormTraverse(storm, stanza->children, context, doStanza);
    }
}
    
struct copyTagContext
/* Information for recursive tagStorm traversing function copyToNewTag*/
    {
    char *oldTag;   // Existing tag name
    char *newTag;   // New tag name
    };

static void copyToNewTag(struct tagStorm *storm, struct tagStanza *stanza, void *context)
/* Add fields from stanza to list of converts in context */
{
struct copyTagContext *copyContext = context;
char *val = tagFindLocalVal(stanza, copyContext->oldTag);
if (val)
    tagStanzaAdd(storm, stanza, copyContext->newTag, val);
}

void tagStormCopyTags(struct tagStorm *tagStorm, char *oldTag, char *newTag)
/* Make a newTag that has same value as oldTag every place oldTag occurs */
{
struct copyTagContext copyContext; 
copyContext.oldTag = oldTag;
copyContext.newTag = newTag;
tagStormTraverse(tagStorm, tagStorm->forest, &copyContext, copyToNewTag);
}


static void rListLeaves(struct tagStanza *list, struct tagStanzaRef **pList)
/* Recursively add leaf stanzas to *pList */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (stanza->children)
        rListLeaves(stanza->children, pList);
    else
        {
	struct tagStanzaRef *ref;
	AllocVar(ref);
	ref->stanza = stanza;
	slAddHead(pList, ref);
	}
    }
}

struct tagStanzaRef *tagStormListLeaves(struct tagStorm *tagStorm)
/* Return list of references to all stanzas in tagStorm.  Free this
 * result with slFreeList. */
{
struct tagStanzaRef *list = NULL;
rListLeaves(tagStorm->forest, &list);
slReverse(&list);
return list;
}

char *tagStanzaRqlLookupField(void *record, char *key)
/* Lookup a field in a tagStanza for rql. */
{
struct tagStanza *stanza = record;
return tagFindVal(stanza, key);
}

boolean tagStanzaRqlMatch(struct rqlStatement *rql, struct tagStanza *stanza,
	struct lm *lm)
/* Return TRUE if where clause and tableList in statement evaluates true for stanza. */
{
struct rqlParse *whereClause = rql->whereClause;
if (whereClause == NULL)
    return TRUE;
else
    {
    struct rqlEval res = rqlEvalOnRecord(whereClause, stanza, tagStanzaRqlLookupField, lm);
    res = rqlEvalCoerceToBoolean(res);
    return res.val.b;
    }
}

static void rQuery(struct tagStorm *tags, struct tagStanza *list, 
    struct rqlStatement *rql, struct lm *lm, struct tagStanza **pResultList)
/* Recursively traverse stanzas on list. */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (stanza->children)
	rQuery(tags, stanza->children, rql, lm, pResultList);
    else    /* Just apply query to leaves */
	{
	if (tagStanzaRqlMatch(rql, stanza, lm))
	    {
	    struct tagStanza *result;
	    AllocVar(result);
	    struct slPair *resultTagList = NULL;
	    struct slName *field;
	    for (field = rql->fieldList; field != NULL; field = field->next)
		{
		char *val = tagFindVal(stanza, field->name);
		if (val != NULL)
		    {
		    struct slPair *resultTag = slPairNew(field->name, val);
		    slAddHead(&resultTagList, resultTag);
		    }
		}
	    slReverse(&resultTagList);
	    result->tagList = resultTagList;
	    slAddHead(pResultList, result);
	    }
	}
    }
}

static void tagStanzaFree(struct tagStanza **pStanza)
/* Free up memory associated with a tagStanza.  Use this judiciously because the
 * stanzas inside of a tagStorm->forest or allocated with tagStanzaNew are allocated with 
 * local memory, while this is assumes stanza is in regular memory. */
{
struct tagStanza *stanza;

if ((stanza = *pStanza) != NULL)
    {
    slPairFreeList(&stanza->tagList);
    freez(pStanza);
    }
}

void tagStanzaFreeList(struct tagStanza **pList)
/* Free up tagStanza list from tagStormQuery. Don't try to free up stanzas from the
 * tagStorm->forest with this though, as those are in a diffent, local, memory pool. */
{
struct tagStanza *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    tagStanzaFree(&el);
    }
*pList = NULL;
}

struct tagStanza *tagStormQuery(struct tagStorm *tagStorm, char *fields, char *where)
/* Returns a list of tagStanzas that match the properties describe in  the where parameter.  
 * The field parameter is a comma separated list of fields that may include wildcards.  
 * For instance "*" will select all fields,  "a*,b*" will select all fields starting with 
 * an "a" or a "b." The where parameter is a Boolean expression similar to what could appear 
 * in a SQL where clause.  Use tagStanzaFreeList to free up result when done. */
{
/* Construct and parse an rql statement */
struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "select %s from tagStorm where ", fields);
dyStringAppend(query, where);
struct rqlStatement *rql = rqlStatementParseString(query->string);

/* Expand any field names with wildcards. */
struct slName *allFieldList = tagStormFieldList(tagStorm);
rql->fieldList = wildExpandList(allFieldList, rql->fieldList, TRUE);

/* Traverse tree applying query to build result list. */
struct tagStanza *resultList = NULL;
struct lm *lm = lmInit(0);
rQuery(tagStorm, tagStorm->forest, rql, lm, &resultList);
slReverse(&resultList);

/* Clean up and go home. */
lmCleanup(&lm);
rqlStatementFree(&rql);
dyStringFree(&query);
return resultList;
}

