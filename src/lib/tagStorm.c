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
 *            formet broadPeak
 *
 *         target CTCF
 *         antibody abCamAntiCtcf
 *
 *            file hg19/chipSeq/helaCTCF.narrowPeak.bigBed
 *            format narrowPeak
 *
 *            file hg19/chipSeq/helaCTCF.broadPeak.bigBed
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
/* Add tag with given value to stanza */
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
/* Add tag with given value to stanza */
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
struct lineFile *lf = lineFileOpen(fileName, TRUE);

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

#ifdef OLD
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
#endif /* OLD */

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

static void rTsWrite(struct tagStanza *list, FILE *f, int maxDepth, int depth)
/* Recursively write out list to file */
{
if (depth >= maxDepth)
    return;
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *pair;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
        {
	repeatCharOut(f, '\t', depth);
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
     int maxDepth, int depth, boolean leavesOnly)
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
		    fputc('\t', f);
		char *val = naForNull(hashFindVal(uniq, field->name));
		fputs(val, f);
		}
	    fputc('\n', f);
	    }
	hashFree(&uniq);
	}

    rTsWriteAsFlatTab(stanza->children, fieldList, f, idTag, withParent, maxDepth, depth+1, leavesOnly);
    }
}

static void rGetAllFields(struct tagStanza *list, struct hash *uniqHash, struct slName **pList)
/* Recursively add all fields in tag-storm */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *pair;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
        {
	if (hashLookup(uniqHash, pair->name) == NULL)
	    {
	    hashAdd(uniqHash, pair->name, pair->val);
	    slNameAddHead(pList, pair->name);
	    }
	rGetAllFields(stanza->children, uniqHash, pList);
	}
    }
}


static struct slName *getAllFields(struct tagStorm *tagStorm)
/* Return list of all fields */
{
struct slName *list = NULL;
struct hash *uniqHash = hashNew(0);
rGetAllFields(tagStorm->forest, uniqHash, &list);
hashFree(&uniqHash);
slReverse(&list);
return list;
}
 
void tagStormWriteAsFlatTab(struct tagStorm *tagStorm, char *fileName, char *idTag, 
    boolean withParent, int maxDepth, boolean leavesOnly)
/* Write tag storm flattening out hierarchy so kids have all of parents tags in .ra format */
{
FILE *f = mustOpen(fileName, "w");
struct slName *fieldList = getAllFields(tagStorm), *field;
if (withParent && slNameFind(fieldList, "parent") == NULL)
    slNameAddHead(&fieldList, "parent");
fputc('#', f);
for (field = fieldList; field != NULL; field = field->next)
    {
    if (field != fieldList)
	fputc('\t', f);
    fprintf(f, "%s", field->name);
    }
fputc('\n', f);
rTsWriteAsFlatTab(tagStorm->forest, fieldList, f, idTag, withParent, maxDepth, 0, leavesOnly);
carefulClose(&f);
}

void tagStormUpdateTag(struct tagStorm *tagStorm, struct tagStanza *stanza, char *tag, char *val)
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

static void rTagStormCountDistinct(struct tagStanza *list, char *tag, struct hash *uniq)
/* Fill in hash with number of times have seen each value of tag */
{
char *requiredTag = "accession";
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (tagFindVal(stanza, requiredTag))
	{
	char *val = tagFindVal(stanza, tag);
	if (val != NULL)
	    {
	    hashIncInt(uniq, val);
	    }
	}
    rTagStormCountDistinct(stanza->children, tag, uniq);
    }
}

struct hash *tagStormCountTagVals(struct tagStorm *tags, char *tag)
/* Return an integer valued hash keyed by all the different values
 * of tag seen in tagStorm.  The hash is filled with counts of the
 * number of times each value is used that can be recovered with 
 * hashIntVal(hash, key) */
{
struct hash *uniq = hashNew(0);
rTagStormCountDistinct(tags->forest, tag, uniq);
return uniq;
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

